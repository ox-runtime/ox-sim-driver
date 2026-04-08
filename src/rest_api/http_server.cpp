#include "http_server.h"

#include <ox_sim.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_STATIC
#include "crow/app.h"
#include "crow/json.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace ox_sim {

namespace {

HttpServer g_http_server;

const char* component_type_name(OxSimComponentType type) {
    switch (type) {
        case OX_SIM_COMPONENT_TYPE_BOOLEAN:
            return "boolean";
        case OX_SIM_COMPONENT_TYPE_FLOAT:
            return "float";
        case OX_SIM_COMPONENT_TYPE_VEC2:
            return "vector2";
        default:
            return "unknown";
    }
}

const char* session_state_name(XrSessionState state) {
    switch (state) {
        case XR_SESSION_STATE_UNKNOWN:
            return "unknown";
        case XR_SESSION_STATE_IDLE:
            return "idle";
        case XR_SESSION_STATE_READY:
            return "ready";
        case XR_SESSION_STATE_SYNCHRONIZED:
            return "synchronized";
        case XR_SESSION_STATE_VISIBLE:
            return "visible";
        case XR_SESSION_STATE_FOCUSED:
            return "focused";
        case XR_SESSION_STATE_STOPPING:
            return "stopping";
        case XR_SESSION_STATE_LOSS_PENDING:
            return "loss_pending";
        case XR_SESSION_STATE_EXITING:
            return "exiting";
        default:
            return "unknown";
    }
}

void png_write_callback(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<uint8_t>*>(context);
    auto* bytes = static_cast<uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
}

std::vector<uint8_t> encode_rgba_to_png(const void* rgba_data, uint32_t width, uint32_t height) {
    std::vector<uint8_t> out;
    if (!rgba_data || width == 0 || height == 0) {
        return out;
    }

    const int stride = static_cast<int>(width * 4);
    stbi_write_png_to_func(png_write_callback, &out, static_cast<int>(width), static_cast<int>(height), 4, rgba_data,
                           stride);
    return out;
}

std::vector<uint8_t> scale_rgba_nearest(const uint8_t* pixels, uint32_t src_width, uint32_t src_height,
                                        uint32_t dst_width, uint32_t* out_height) {
    std::vector<uint8_t> scaled;
    if (!pixels || src_width == 0 || src_height == 0 || dst_width == 0) {
        return scaled;
    }

    const uint32_t dst_height = std::max(
        1u, static_cast<uint32_t>(std::lround((static_cast<double>(src_height) * static_cast<double>(dst_width)) /
                                              static_cast<double>(src_width))));
    scaled.resize(static_cast<size_t>(dst_width) * dst_height * 4);

    for (uint32_t y = 0; y < dst_height; ++y) {
        const uint32_t src_y = std::min(src_height - 1, (y * src_height) / dst_height);
        for (uint32_t x = 0; x < dst_width; ++x) {
            const uint32_t src_x = std::min(src_width - 1, (x * src_width) / dst_width);
            const size_t src_index = (static_cast<size_t>(src_y) * src_width + src_x) * 4;
            const size_t dst_index = (static_cast<size_t>(y) * dst_width + x) * 4;
            std::memcpy(&scaled[dst_index], pixels + src_index, 4);
        }
    }

    if (out_height) {
        *out_height = dst_height;
    }
    return scaled;
}

bool split_input_path(const std::string& path, std::string* user_path, std::string* component_path) {
    const size_t input_pos = path.find("/input/");
    if (input_pos == std::string::npos || input_pos == 0) {
        return false;
    }

    if (user_path) {
        *user_path = "/" + path.substr(0, input_pos);
    }
    if (component_path) {
        *component_path = path.substr(input_pos);
    }
    return true;
}

bool find_component_info(const std::string& user_path, const std::string& component_path,
                         OxSimComponentInfo* out_info) {
    uint32_t component_count = 0;
    if (ox_sim_get_component_count(user_path.c_str(), &component_count) != OX_SIM_SUCCESS) {
        return false;
    }

    for (uint32_t index = 0; index < component_count; ++index) {
        OxSimComponentInfo component_info = {};
        if (ox_sim_get_component_info(user_path.c_str(), index, &component_info) != OX_SIM_SUCCESS) {
            continue;
        }
        if (component_path == component_info.path) {
            if (out_info) {
                *out_info = component_info;
            }
            return true;
        }
    }

    return false;
}

}  // namespace

HttpServer& GetHttpServer() { return g_http_server; }

HttpServer::HttpServer() = default;
HttpServer::~HttpServer() { Stop(); }

void HttpServer::SetPort(int port) {
    if (port > 0) {
        port_ = port;
    }
}

bool HttpServer::Start() {
    if (running_.load()) {
        return false;
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    should_stop_.store(false);
    server_thread_ = std::thread(&HttpServer::ServerThread, this);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return running_.load();
}

void HttpServer::Stop() {
    if (running_.load()) {
        should_stop_.store(true);
        if (app_) {
            app_->stop();
        }
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    running_.store(false);
}

void HttpServer::ServerThread() {
    running_.store(true);
    app_ = std::make_unique<crow::SimpleApp>();
    crow::SimpleApp& app = *app_;

    CROW_ROUTE(app, "/v1/status").methods("GET"_method)([]() {
        OxSimStatus status = {};
        if (ox_sim_get_status(&status) != OX_SIM_SUCCESS) {
            return crow::response(503, "Simulator state unavailable");
        }

        crow::json::wvalue response;
        response["session_state"] = session_state_name(status.session_state);
        response["session_state_id"] = static_cast<uint32_t>(status.session_state);
        response["session_active"] = status.session_active != XR_FALSE;
        response["fps"] = status.fps;
        return crow::response(response);
    });

    auto eye_frame_handler = [](const crow::request& req, int eye) -> crow::response {
        uint32_t view_count = 0;
        if (eye < 0 || ox_sim_get_view_count(&view_count) != OX_SIM_SUCCESS ||
            static_cast<uint32_t>(eye) >= view_count) {
            return crow::response(404, "Eye not found");
        }

        OxSimViewInfo view = {};
        const OxSimResult view_result = ox_sim_get_view_info(static_cast<uint32_t>(eye), &view);
        if (view_result == OX_SIM_ERROR_INVALID_ARGUMENT) {
            return crow::response(404, "Eye not found");
        }
        if (view_result != OX_SIM_SUCCESS || view.width == 0 || view.height == 0 || view.data_size == 0) {
            return crow::response(404, "No frame available");
        }

        std::vector<uint8_t> pixels(view.data_size);
        if (ox_sim_get_view(static_cast<uint32_t>(eye), pixels.data(), static_cast<uint32_t>(pixels.size())) !=
            OX_SIM_SUCCESS) {
            return crow::response(503, "Frame preview unavailable");
        }

        uint32_t output_width = view.width;
        uint32_t output_height = view.height;

        if (auto size_param = req.url_params.get("size")) {
            try {
                int requested_width = std::stoi(size_param);
                if (requested_width > 0) {
                    float aspect_ratio = static_cast<float>(view.width) / static_cast<float>(view.height);
                    output_width = static_cast<uint32_t>(requested_width);
                    output_height = static_cast<uint32_t>(requested_width / aspect_ratio);
                }
            } catch (...) {
            }
        }

        std::vector<uint8_t> png;
        if (output_width == view.width && output_height == view.height) {
            png = encode_rgba_to_png(pixels.data(), view.width, view.height);
        } else {
            std::vector<uint8_t> resized_pixels =
                scale_rgba_nearest(pixels.data(), view.width, view.height, output_width, &output_height);
            if (resized_pixels.empty()) {
                return crow::response(500, "Image resizing failed");
            }
            png = encode_rgba_to_png(resized_pixels.data(), output_width, output_height);
        }

        if (png.empty()) {
            return crow::response(500, "PNG encoding failed");
        }

        crow::response response;
        response.code = 200;
        response.set_header("Content-Type", "image/png");
        response.body = std::string(reinterpret_cast<const char*>(png.data()), png.size());
        return response;
    };

    CROW_ROUTE(app, "/v1/views/<int>").methods("GET"_method)(eye_frame_handler);

    CROW_ROUTE(app, "/v1/profile").methods("GET"_method)([]() {
        char profile_id[64] = {};
        OxSimProfileInfo profile_info = {};
        if (ox_sim_get_profile(profile_id, sizeof(profile_id)) != OX_SIM_SUCCESS ||
            ox_sim_get_profile_info(&profile_info) != OX_SIM_SUCCESS) {
            return crow::response(503, "Profile unavailable");
        }

        crow::json::wvalue response;
        response["id"] = profile_id;
        response["name"] = profile_info.name;
        response["manufacturer"] = profile_info.manufacturer;
        response["interaction_profile"] = profile_info.interaction_profile;

        uint32_t device_count = 0;
        if (ox_sim_get_device_count(&device_count) != OX_SIM_SUCCESS) {
            return crow::response(503, "Devices unavailable");
        }

        crow::json::wvalue devices_array(crow::json::type::List);
        for (uint32_t i = 0; i < device_count; ++i) {
            OxSimDeviceInfo device = {};
            if (ox_sim_get_device_info(i, &device) != OX_SIM_SUCCESS) {
                continue;
            }

            crow::json::wvalue dev_obj;
            dev_obj["user_path"] = device.user_path;
            dev_obj["role"] = device.role;
            dev_obj["always_active"] = device.always_active != XR_FALSE;

            crow::json::wvalue components_array(crow::json::type::List);
            uint32_t component_count = 0;
            if (ox_sim_get_component_count(device.user_path, &component_count) == OX_SIM_SUCCESS) {
                for (uint32_t j = 0; j < component_count; ++j) {
                    OxSimComponentInfo component = {};
                    if (ox_sim_get_component_info(device.user_path, j, &component) != OX_SIM_SUCCESS) {
                        continue;
                    }
                    crow::json::wvalue comp_obj;
                    comp_obj["path"] = component.path;
                    comp_obj["type"] = component_type_name(component.type);
                    comp_obj["description"] = component.description;
                    components_array[j] = std::move(comp_obj);
                }
            }

            dev_obj["components"] = std::move(components_array);
            devices_array[i] = std::move(dev_obj);
        }

        response["devices"] = std::move(devices_array);
        return crow::response(response);
    });

    CROW_ROUTE(app, "/v1/profile").methods("PUT"_method)([](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) {
            return crow::response(400, "Invalid JSON");
        }

        const bool has_profile_id = json.has("profile_id") && json["profile_id"].t() == crow::json::type::String;
        const bool has_device = json.has("device") && json["device"].t() == crow::json::type::String;
        if (!has_profile_id && !has_device) {
            return crow::response(400, "Missing required field: profile_id (string)");
        }

        std::string profile_id = has_profile_id ? json["profile_id"].s() : json["device"].s();
        if (ox_sim_set_profile(profile_id.c_str()) != OX_SIM_SUCCESS) {
            return crow::response(404, "Unknown profile: " + profile_id);
        }

        OxSimProfileInfo profile_info = {};
        if (ox_sim_get_profile_info(&profile_info) != OX_SIM_SUCCESS) {
            return crow::response(500, "Failed to load switched profile");
        }

        crow::json::wvalue response;
        response["status"] = "ok";
        response["profile_id"] = profile_id;
        response["name"] = profile_info.name;
        response["interaction_profile"] = profile_info.interaction_profile;
        return crow::response(response);
    });

    CROW_ROUTE(app, "/v1/devices/<path>").methods("GET"_method)([](const std::string& user_path) {
        const std::string full_user_path = "/" + user_path;
        OxDeviceState state = {};
        if (ox_sim_get_device(full_user_path.c_str(), &state) != OX_SIM_SUCCESS) {
            return crow::response(404, "Device not found");
        }

        crow::json::wvalue response;
        response["active"] = state.is_active != XR_FALSE;
        response["position"]["x"] = state.pose.position.x;
        response["position"]["y"] = state.pose.position.y;
        response["position"]["z"] = state.pose.position.z;
        response["orientation"]["x"] = state.pose.orientation.x;
        response["orientation"]["y"] = state.pose.orientation.y;
        response["orientation"]["z"] = state.pose.orientation.z;
        response["orientation"]["w"] = state.pose.orientation.w;
        return crow::response(response);
    });

    CROW_ROUTE(app, "/v1/devices/<path>")
        .methods("PUT"_method)([](const crow::request& req, const std::string& user_path) {
            auto json = crow::json::load(req.body);
            if (!json) {
                return crow::response(400, "Invalid JSON");
            }
            if (!json.has("position") || !json.has("orientation") || !json["position"].has("x") ||
                !json["position"].has("y") || !json["position"].has("z") || !json["orientation"].has("x") ||
                !json["orientation"].has("y") || !json["orientation"].has("z") || !json["orientation"].has("w")) {
                return crow::response(400, "Missing required fields: position{x,y,z}, orientation{x,y,z,w}");
            }

            const std::string full_user_path = "/" + user_path;
            OxDeviceState state = {};
            state.pose.position.x = json["position"]["x"].d();
            state.pose.position.y = json["position"]["y"].d();
            state.pose.position.z = json["position"]["z"].d();
            state.pose.orientation.x = json["orientation"]["x"].d();
            state.pose.orientation.y = json["orientation"]["y"].d();
            state.pose.orientation.z = json["orientation"]["z"].d();
            state.pose.orientation.w = json["orientation"]["w"].d();
            state.is_active = json.has("active") ? (json["active"].b() ? XR_TRUE : XR_FALSE) : XR_TRUE;

            if (ox_sim_set_device(full_user_path.c_str(), &state) != OX_SIM_SUCCESS) {
                return crow::response(500, "Failed to set device state");
            }

            return crow::response(200, "OK");
        });

    CROW_ROUTE(app, "/v1/inputs/<path>").methods("GET"_method)([](const std::string& binding_path) {
        std::string user_path;
        std::string component_path;
        if (!split_input_path("/" + binding_path, &user_path, &component_path)) {
            return crow::response(400, "Invalid binding path");
        }

        OxSimComponentInfo component_info = {};
        if (!find_component_info(user_path, component_path, &component_info)) {
            return crow::response(404, "Input component not found");
        }

        crow::json::wvalue response;
        response["type"] = component_type_name(component_info.type);
        response["description"] = component_info.description;

        switch (component_info.type) {
            case OX_SIM_COMPONENT_TYPE_BOOLEAN: {
                XrBool32 value = XR_FALSE;
                if (ox_sim_get_input_boolean(user_path.c_str(), component_path.c_str(), &value) != OX_SIM_SUCCESS) {
                    return crow::response(404, "Input component not found");
                }
                response["value"] = value != XR_FALSE;
                break;
            }
            case OX_SIM_COMPONENT_TYPE_FLOAT: {
                float value = 0.0f;
                if (ox_sim_get_input_float(user_path.c_str(), component_path.c_str(), &value) != OX_SIM_SUCCESS) {
                    return crow::response(404, "Input component not found");
                }
                response["value"] = value;
                break;
            }
            case OX_SIM_COMPONENT_TYPE_VEC2: {
                XrVector2f value = {};
                if (ox_sim_get_input_vector2f(user_path.c_str(), component_path.c_str(), &value) != OX_SIM_SUCCESS) {
                    return crow::response(404, "Input component not found");
                }
                response["x"] = value.x;
                response["y"] = value.y;
                break;
            }
            default:
                return crow::response(500, "Unsupported component type");
        }

        return crow::response(response);
    });

    CROW_ROUTE(app, "/v1/inputs/<path>")
        .methods("PUT"_method)([](const crow::request& req, const std::string& binding_path) {
            std::string user_path;
            std::string component_path;
            if (!split_input_path("/" + binding_path, &user_path, &component_path)) {
                return crow::response(400, "Invalid binding path");
            }

            OxSimComponentInfo component_info = {};
            if (!find_component_info(user_path, component_path, &component_info)) {
                return crow::response(404, "Input component not found");
            }

            auto json = crow::json::load(req.body);
            if (!json) {
                return crow::response(400, "Invalid JSON");
            }

            OxSimResult result = OX_SIM_ERROR_INVALID_ARGUMENT;
            switch (component_info.type) {
                case OX_SIM_COMPONENT_TYPE_BOOLEAN:
                    if (json.has("value") &&
                        (json["value"].t() == crow::json::type::True || json["value"].t() == crow::json::type::False)) {
                        result = ox_sim_set_input_boolean(user_path.c_str(), component_path.c_str(),
                                                          json["value"].b() ? XR_TRUE : XR_FALSE);
                    }
                    break;
                case OX_SIM_COMPONENT_TYPE_FLOAT:
                    if (json.has("value") && json["value"].t() == crow::json::type::Number) {
                        result = ox_sim_set_input_float(user_path.c_str(), component_path.c_str(),
                                                        static_cast<float>(json["value"].d()));
                    }
                    break;
                case OX_SIM_COMPONENT_TYPE_VEC2:
                    if (json.has("x") && json.has("y")) {
                        XrVector2f vec = {static_cast<float>(json["x"].d()), static_cast<float>(json["y"].d())};
                        result = ox_sim_set_input_vector2f(user_path.c_str(), component_path.c_str(), &vec);
                    }
                    break;
                default:
                    break;
            }

            if (result != OX_SIM_SUCCESS) {
                return crow::response(400, "Failed to update input component");
            }

            return crow::response(200, "OK");
        });

    CROW_ROUTE(app, "/")([]() {
        return "ox Simulator API Server\n\nAvailable endpoints:\n"
               "  GET      /v1/status                 - Session state and app FPS\n"
               "  GET/PUT  /v1/profile                - Get/switch device profile\n"
               "  GET/PUT  /v1/devices/<user_path>    - Get/set device state\n"
               "  GET/PUT  /v1/inputs/<binding_path>  - Get/set typed input state\n"
               "  GET      /v1/views/<eye>            - Eye texture (PNG), eye=0 or 1\n";
    });

    try {
        app.loglevel(crow::LogLevel::Info);
        app.concurrency(1);
        app.bindaddr("127.0.0.1");
        app.port(port_);
        app.run();
    } catch (const std::exception& e) {
        spdlog::error("Exception starting server: {}", e.what());
    } catch (...) {
        spdlog::error("Unknown exception starting server");
    }

    app_.reset();
    running_.store(false);
}

}  // namespace ox_sim