#include "http_server.h"

#include <ox_sim.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_STATIC
#include "crow/app.h"
#include "crow/json.h"
#include "device_profiles.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace ox_sim {

namespace {

HttpServer g_http_server;

const DeviceProfile* current_profile() {
    char profile_name[64] = {};
    if (ox_sim_get_current_profile(profile_name, sizeof(profile_name)) != OX_SIM_SUCCESS) {
        return nullptr;
    }

    return GetDeviceProfileByName(profile_name);
}

const char* component_type_name(ComponentType type) {
    switch (type) {
        case ComponentType::BOOLEAN:
            return "boolean";
        case ComponentType::FLOAT:
            return "float";
        case ComponentType::VEC2:
            return "vec2";
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

bool is_session_active(XrSessionState state) {
    return state == XR_SESSION_STATE_SYNCHRONIZED || state == XR_SESSION_STATE_VISIBLE ||
           state == XR_SESSION_STATE_FOCUSED;
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
        XrSessionState state = XR_SESSION_STATE_UNKNOWN;
        uint32_t fps = 0;
        if (ox_sim_get_session_state(&state) != OX_SIM_SUCCESS || ox_sim_get_app_fps(&fps) != OX_SIM_SUCCESS) {
            return crow::response(503, "Simulator state unavailable");
        }

        crow::json::wvalue response;
        response["session_state"] = session_state_name(state);
        response["session_state_id"] = static_cast<uint32_t>(state);
        response["session_active"] = is_session_active(state);
        response["fps"] = is_session_active(state) ? fps : 0u;
        return crow::response(response);
    });

    auto eye_frame_handler = [](const crow::request& req, int eye) -> crow::response {
        if (eye < 0 || eye > 1) {
            return crow::response(404, "Eye not found");
        }

        OxSimFramePreview preview = {};
        if (ox_sim_get_frame_preview(&preview) != OX_SIM_SUCCESS) {
            return crow::response(503, "Frame preview unavailable");
        }

        if (preview.width == 0 || preview.height == 0 || !preview.pixel_data[eye] || preview.data_size[eye] == 0) {
            return crow::response(404, "No frame available");
        }

        uint32_t output_width = preview.width;
        uint32_t output_height = preview.height;

        if (auto size_param = req.url_params.get("size")) {
            try {
                int requested_width = std::stoi(size_param);
                if (requested_width > 0) {
                    float aspect_ratio = static_cast<float>(preview.width) / static_cast<float>(preview.height);
                    output_width = static_cast<uint32_t>(requested_width);
                    output_height = static_cast<uint32_t>(requested_width / aspect_ratio);
                }
            } catch (...) {
            }
        }

        std::vector<uint8_t> png;
        if (output_width == preview.width && output_height == preview.height) {
            png = encode_rgba_to_png(preview.pixel_data[eye], preview.width, preview.height);
        } else {
            std::vector<uint8_t> resized_pixels =
                scale_rgba_nearest(static_cast<const uint8_t*>(preview.pixel_data[eye]), preview.width, preview.height,
                                   output_width, &output_height);
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
        const DeviceProfile* profile = current_profile();
        if (!profile) {
            return crow::response(500, "No device profile loaded");
        }

        crow::json::wvalue response;
        response["type"] = profile->name;
        response["manufacturer"] = profile->manufacturer;
        response["interaction_profile"] = profile->interaction_profile;

        crow::json::wvalue devices_array(crow::json::type::List);
        for (size_t i = 0; i < profile->devices.size(); ++i) {
            const auto& device = profile->devices[i];
            crow::json::wvalue dev_obj;
            dev_obj["user_path"] = device.user_path;
            dev_obj["role"] = device.role;
            dev_obj["always_active"] = device.always_active;

            crow::json::wvalue components_array(crow::json::type::List);
            for (size_t j = 0; j < device.components.size(); ++j) {
                const auto& component = device.components[j];
                crow::json::wvalue comp_obj;
                comp_obj["path"] = component.path;
                comp_obj["type"] = component_type_name(component.type);
                comp_obj["description"] = component.description;
                components_array[j] = std::move(comp_obj);
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
        if (!json.has("device") || json["device"].t() != crow::json::type::String) {
            return crow::response(400, "Missing required field: device (string)");
        }

        std::string device_name = json["device"].s();
        if (ox_sim_set_current_profile(device_name.c_str()) != OX_SIM_SUCCESS) {
            return crow::response(404, "Unknown device: " + device_name);
        }

        const DeviceProfile* profile = current_profile();
        if (!profile) {
            return crow::response(500, "Failed to load switched profile");
        }

        crow::json::wvalue response;
        response["status"] = "ok";
        response["device"] = profile->name;
        response["interaction_profile"] = profile->interaction_profile;
        return crow::response(response);
    });

    CROW_ROUTE(app, "/v1/devices/<path>").methods("GET"_method)([](const std::string& user_path) {
        const std::string full_user_path = "/" + user_path;
        XrPosef pose = {};
        XrBool32 active = XR_FALSE;
        if (ox_sim_get_device_pose(full_user_path.c_str(), &pose, &active) != OX_SIM_SUCCESS) {
            return crow::response(404, "Device not found");
        }

        crow::json::wvalue response;
        response["active"] = active != 0;
        response["position"]["x"] = pose.position.x;
        response["position"]["y"] = pose.position.y;
        response["position"]["z"] = pose.position.z;
        response["orientation"]["x"] = pose.orientation.x;
        response["orientation"]["y"] = pose.orientation.y;
        response["orientation"]["z"] = pose.orientation.z;
        response["orientation"]["w"] = pose.orientation.w;
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
            XrPosef pose = {};
            pose.position.x = json["position"]["x"].d();
            pose.position.y = json["position"]["y"].d();
            pose.position.z = json["position"]["z"].d();
            pose.orientation.x = json["orientation"]["x"].d();
            pose.orientation.y = json["orientation"]["y"].d();
            pose.orientation.z = json["orientation"]["z"].d();
            pose.orientation.w = json["orientation"]["w"].d();
            XrBool32 active = json.has("active") ? (json["active"].b() ? XR_TRUE : XR_FALSE) : XR_TRUE;

            if (ox_sim_set_device_pose(full_user_path.c_str(), &pose, active) != OX_SIM_SUCCESS) {
                return crow::response(500, "Failed to set device pose");
            }

            return crow::response(200, "OK");
        });

    CROW_ROUTE(app, "/v1/inputs/<path>").methods("GET"_method)([](const std::string& binding_path) {
        std::string user_path;
        std::string component_path;
        if (!split_input_path("/" + binding_path, &user_path, &component_path)) {
            return crow::response(400, "Invalid binding path");
        }

        uint32_t boolean_value = 0;
        float float_value = 0.0f;
        XrVector2f vec2_value = {};
        const OxSimResult bool_result =
            ox_sim_get_input_state_boolean(user_path.c_str(), component_path.c_str(), &boolean_value);
        const OxSimResult float_result =
            ox_sim_get_input_state_float(user_path.c_str(), component_path.c_str(), &float_value);
        const OxSimResult vec2_result =
            ox_sim_get_input_state_vector2f(user_path.c_str(), component_path.c_str(), &vec2_value);

        if (bool_result != OX_SIM_SUCCESS && float_result != OX_SIM_SUCCESS && vec2_result != OX_SIM_SUCCESS) {
            return crow::response(404, "Input component not found");
        }

        crow::json::wvalue response;
        response["boolean_value"] = boolean_value != 0;
        response["float_value"] = float_value;
        response["x"] = vec2_value.x;
        response["y"] = vec2_value.y;
        return crow::response(response);
    });

    CROW_ROUTE(app, "/v1/inputs/<path>")
        .methods("PUT"_method)([](const crow::request& req, const std::string& binding_path) {
            std::string user_path;
            std::string component_path;
            if (!split_input_path("/" + binding_path, &user_path, &component_path)) {
                return crow::response(400, "Invalid binding path");
            }

            auto json = crow::json::load(req.body);
            if (!json) {
                return crow::response(400, "Invalid JSON");
            }

            OxSimResult result = OX_SIM_ERROR_INVALID_ARGUMENT;
            if (json.has("value")) {
                if (json["value"].t() == crow::json::type::True || json["value"].t() == crow::json::type::False) {
                    result = ox_sim_set_input_state_boolean(user_path.c_str(), component_path.c_str(),
                                                            json["value"].b() ? 1u : 0u);
                } else if (json["value"].t() == crow::json::type::Number) {
                    result = ox_sim_set_input_state_float(user_path.c_str(), component_path.c_str(),
                                                          static_cast<float>(json["value"].d()));
                }
            } else if (json.has("x") && json.has("y")) {
                XrVector2f vec = {static_cast<float>(json["x"].d()), static_cast<float>(json["y"].d())};
                result = ox_sim_set_input_state_vector2f(user_path.c_str(), component_path.c_str(), &vec);
            }

            if (result != OX_SIM_SUCCESS) {
                return crow::response(400, "Failed to update input component");
            }

            return crow::response(200, "OK");
        });

    CROW_ROUTE(app, "/")([]() {
        return "ox Simulator API Server\n\nAvailable endpoints:\n"
               "  GET      /v1/status                 - Session state and FPS\n"
               "  GET/PUT  /v1/profile                - Get/switch device profile\n"
               "  GET/PUT  /v1/devices/<user_path>    - Get/set device pose\n"
               "  GET/PUT  /v1/inputs/<binding_path>  - Get/set input component state\n"
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