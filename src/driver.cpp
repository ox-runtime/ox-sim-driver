#include <ox_driver.h>
#include <ox_sim.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "device_profiles.hpp"
#include "gui/gui_window.h"
#include "rest_api/http_server.h"

using namespace ox_sim;

static OxVector3f rotate_vector_by_quat(const OxQuaternion& q, const OxVector3f& v);

extern "C" void sim_submit_frame(uint32_t eye, uint32_t w, uint32_t h, const void* data, uint32_t size);
extern "C" void sim_notify_session(OxSessionState state);
extern "C" void sim_copy_devices(OxDeviceState* out, uint32_t max, uint32_t* out_count);

namespace {

GuiWindow g_gui;

const DeviceProfile* current_profile() {
    char profile_name[64] = {};
    if (ox_sim_get_current_profile(profile_name, sizeof(profile_name)) != OX_SIM_SUCCESS) {
        return nullptr;
    }

    return GetDeviceProfileByName(profile_name);
}

OxComponentResult result_to_component_result(OxSimResult result) {
    return result == OX_SIM_SUCCESS ? OX_COMPONENT_AVAILABLE : OX_COMPONENT_UNAVAILABLE;
}

}  // namespace

static int simulator_initialize(void) {
    spdlog::info("=== ox Simulator Driver ===");

    if (ox_sim_initialize() != OX_SIM_SUCCESS) {
        spdlog::error("Failed to initialize simulator state");
        return 0;
    }

    if (!GetHttpServer().Start(kHttpServerPort)) {
        spdlog::warn("Failed to start HTTP API server on port {}", kHttpServerPort);
    }

    if (!g_gui.Start()) {
        spdlog::error("Failed to start GUI window");
        GetHttpServer().Stop();
        ox_sim_shutdown();
        return 0;
    }

    spdlog::info("Simulator driver initialized successfully");
    return 1;
}

static void simulator_shutdown(void) {
    spdlog::info("Shutting down simulator driver...");
    g_gui.Stop();
    GetHttpServer().Stop();
    ox_sim_shutdown();
    spdlog::info("Simulator driver shut down");
}

static int simulator_is_device_connected(void) { return 1; }

static int simulator_is_driver_running(void) {
    if (!g_gui.IsRunning()) {
        spdlog::info("Simulator GUI window is closed.");
        return 0;
    }
    return 1;
}

static void simulator_get_device_info(OxDeviceInfo* info) {
    const DeviceProfile* device_profile = current_profile();
    if (!info || !device_profile) {
        return;
    }

    std::snprintf(info->name, sizeof(info->name), "%s", device_profile->name);
    std::snprintf(info->manufacturer, sizeof(info->manufacturer), "%s", device_profile->manufacturer);
    std::string serial = std::string(device_profile->serial_prefix) + "-12345";
    std::snprintf(info->serial, sizeof(info->serial), "%s", serial.c_str());
    info->vendor_id = device_profile->vendor_id;
    info->product_id = device_profile->product_id;
}

static void simulator_get_display_properties(OxDisplayProperties* props) {
    const DeviceProfile* device_profile = current_profile();
    if (!props || !device_profile) {
        return;
    }

    props->display_width = device_profile->display_width;
    props->display_height = device_profile->display_height;
    props->recommended_width = device_profile->recommended_width;
    props->recommended_height = device_profile->recommended_height;
    props->refresh_rate = device_profile->refresh_rate;
    props->fov.angle_left = device_profile->fov_left;
    props->fov.angle_right = device_profile->fov_right;
    props->fov.angle_up = device_profile->fov_up;
    props->fov.angle_down = device_profile->fov_down;
}

static void simulator_get_tracking_capabilities(OxTrackingCapabilities* caps) {
    const DeviceProfile* device_profile = current_profile();
    if (!caps || !device_profile) {
        return;
    }

    caps->has_position_tracking = device_profile->has_position_tracking ? 1u : 0u;
    caps->has_orientation_tracking = device_profile->has_orientation_tracking ? 1u : 0u;
}

static void simulator_update_view_pose(int64_t predicted_time, uint32_t eye_index, OxPose* out_pose) {
    (void)predicted_time;

    OxPose hmd_pose = {{0.0f, 1.6f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};
    uint32_t is_active = 0;
    ox_sim_get_device_pose("/user/head", &hmd_pose, &is_active);

    const float ipd = 0.063f;
    const float eye_offset = eye_index == 0 ? -ipd / 2.0f : ipd / 2.0f;
    const OxVector3f eye_local = {eye_offset, 0.0f, 0.0f};
    const OxVector3f rotated_offset = rotate_vector_by_quat(hmd_pose.orientation, eye_local);

    *out_pose = hmd_pose;
    out_pose->position.x += rotated_offset.x;
    out_pose->position.y += rotated_offset.y;
    out_pose->position.z += rotated_offset.z;
}

static void simulator_update_devices(int64_t predicted_time, OxDeviceState* out_states, uint32_t* out_count) {
    (void)predicted_time;
    sim_copy_devices(out_states, OX_MAX_DEVICES, out_count);
}

static OxComponentResult simulator_get_input_state_boolean(int64_t predicted_time, const char* user_path,
                                                           const char* component_path, uint32_t* out_value) {
    (void)predicted_time;
    return result_to_component_result(ox_sim_get_input_state_boolean(user_path, component_path, out_value));
}

static OxComponentResult simulator_get_input_state_float(int64_t predicted_time, const char* user_path,
                                                         const char* component_path, float* out_value) {
    (void)predicted_time;
    return result_to_component_result(ox_sim_get_input_state_float(user_path, component_path, out_value));
}

static OxComponentResult simulator_get_input_state_vector2f(int64_t predicted_time, const char* user_path,
                                                            const char* component_path, OxVector2f* out_value) {
    (void)predicted_time;
    return result_to_component_result(ox_sim_get_input_state_vector2f(user_path, component_path, out_value));
}

static uint32_t simulator_get_interaction_profiles(const char** out_profiles, uint32_t max_count) {
    const DeviceProfile* device_profile = current_profile();
    if (!out_profiles || max_count == 0 || !device_profile) {
        return 0;
    }

    out_profiles[0] = device_profile->interaction_profile;
    return 1;
}

static void simulator_on_session_state_changed(OxSessionState new_state) { sim_notify_session(new_state); }

static void simulator_submit_frame_pixels(uint32_t eye_index, uint32_t width, uint32_t height, uint32_t format,
                                          const void* pixel_data, uint32_t data_size) {
    (void)format;
    sim_submit_frame(eye_index, width, height, pixel_data, data_size);
}

// ===== Driver Registration =====

extern "C" OX_DRIVER_EXPORT int ox_driver_register(OxDriverCallbacks* callbacks) {
    if (!callbacks) {
        return 0;
    }

    callbacks->initialize = simulator_initialize;
    callbacks->shutdown = simulator_shutdown;
    callbacks->is_driver_running = simulator_is_driver_running;
    callbacks->is_device_connected = simulator_is_device_connected;
    callbacks->get_device_info = simulator_get_device_info;
    callbacks->get_display_properties = simulator_get_display_properties;
    callbacks->get_tracking_capabilities = simulator_get_tracking_capabilities;
    callbacks->update_view_pose = simulator_update_view_pose;
    callbacks->update_devices = simulator_update_devices;
    callbacks->get_input_state_boolean = simulator_get_input_state_boolean;
    callbacks->get_input_state_float = simulator_get_input_state_float;
    callbacks->get_input_state_vector2f = simulator_get_input_state_vector2f;
    callbacks->get_interaction_profiles = simulator_get_interaction_profiles;
    callbacks->on_session_state_changed = simulator_on_session_state_changed;
    callbacks->submit_frame_pixels = simulator_submit_frame_pixels;

    return 1;
}

static OxVector3f rotate_vector_by_quat(const OxQuaternion& q, const OxVector3f& v) {
    // t = 2 * cross(q.xyz, v)
    OxVector3f t;
    t.x = 2.0f * (q.y * v.z - q.z * v.y);
    t.y = 2.0f * (q.z * v.x - q.x * v.z);
    t.z = 2.0f * (q.x * v.y - q.y * v.x);

    // result = v + q.w * t + cross(q.xyz, t)
    OxVector3f cross_q_t;
    cross_q_t.x = q.y * t.z - q.z * t.y;
    cross_q_t.y = q.z * t.x - q.x * t.z;
    cross_q_t.z = q.x * t.y - q.y * t.x;

    OxVector3f res;
    res.x = v.x + q.w * t.x + cross_q_t.x;
    res.y = v.y + q.w * t.y + cross_q_t.y;
    res.z = v.z + q.w * t.z + cross_q_t.z;

    return res;
}
