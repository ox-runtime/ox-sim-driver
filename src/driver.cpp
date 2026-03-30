#include <ox_driver.h>
#include <ox_sim.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "device_profiles.hpp"
#include "gui/gui_window.h"
#include "math.hpp"
#include "rest_api/http_server.h"

using namespace ox_sim;

extern "C" void sim_submit_frame(XrTime frame_time, uint32_t eye, uint32_t w, uint32_t h, const void* data,
                                 uint32_t size);
extern "C" void sim_notify_session(XrSessionState state);
extern "C" void sim_copy_devices(OxDeviceState* out, uint32_t max, uint32_t* out_count);

namespace {

namespace sim_math = ox_sim::math;

GuiWindow g_gui;

const DeviceProfile* current_profile() {
    char profile_name[64] = {};
    if (ox_sim_get_current_profile(profile_name, sizeof(profile_name)) != OX_SIM_SUCCESS) {
        return nullptr;
    }

    return GetDeviceProfileByName(profile_name);
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

static void simulator_get_system_properties(XrSystemProperties* props) {
    const DeviceProfile* device_profile = current_profile();
    if (!props || !device_profile) {
        return;
    }

    std::snprintf(props->systemName, XR_MAX_SYSTEM_NAME_SIZE, "%s", device_profile->name);
    props->vendorId = device_profile->vendor_id;
    props->graphicsProperties.maxSwapchainImageWidth = device_profile->display_width;
    props->graphicsProperties.maxSwapchainImageHeight = device_profile->display_height;
    props->trackingProperties.orientationTracking = device_profile->has_orientation_tracking ? XR_TRUE : XR_FALSE;
    props->trackingProperties.positionTracking = device_profile->has_position_tracking ? XR_TRUE : XR_FALSE;
}

static void simulator_update_view(XrTime predicted_time, uint32_t eye_index, XrView* out_view) {
    (void)predicted_time;

    XrPosef hmd_pose = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.6f, 0.0f}};
    XrBool32 is_active = XR_FALSE;
    ox_sim_get_device_pose("/user/head", &hmd_pose, &is_active);

    const float ipd = 0.063f;
    const float eye_offset = eye_index == 0 ? -ipd / 2.0f : ipd / 2.0f;
    const glm::vec3 rotated_offset = sim_math::RotateVector(hmd_pose.orientation, glm::vec3(eye_offset, 0.0f, 0.0f));

    out_view->pose = hmd_pose;
    out_view->pose.position.x += rotated_offset.x;
    out_view->pose.position.y += rotated_offset.y;
    out_view->pose.position.z += rotated_offset.z;

    const DeviceProfile* profile = current_profile();
    if (profile) {
        out_view->fov = {profile->fov_left, profile->fov_right, profile->fov_up, profile->fov_down};
    } else {
        out_view->fov = {-0.785398f, 0.785398f, 0.785398f, -0.785398f};
    }
}

static void simulator_update_devices(XrTime predicted_time, OxDeviceState* out_states, uint32_t* out_count) {
    (void)predicted_time;
    sim_copy_devices(out_states, OX_MAX_DEVICES, out_count);
}

static XrResult simulator_get_input_state_boolean(XrTime predicted_time, const char* user_path,
                                                  const char* component_path, XrBool32* out_value) {
    (void)predicted_time;
    uint32_t raw_value = 0;
    const OxSimResult result = ox_sim_get_input_state_boolean(user_path, component_path, &raw_value);
    if (result != OX_SIM_SUCCESS) {
        return XR_ERROR_PATH_UNSUPPORTED;
    }
    if (out_value) {
        *out_value = raw_value ? XR_TRUE : XR_FALSE;
    }
    return XR_SUCCESS;
}

static XrResult simulator_get_input_state_float(XrTime predicted_time, const char* user_path,
                                                const char* component_path, float* out_value) {
    (void)predicted_time;
    return ox_sim_get_input_state_float(user_path, component_path, out_value) == OX_SIM_SUCCESS
               ? XR_SUCCESS
               : XR_ERROR_PATH_UNSUPPORTED;
}

static XrResult simulator_get_input_state_vector2f(XrTime predicted_time, const char* user_path,
                                                   const char* component_path, XrVector2f* out_value) {
    (void)predicted_time;
    return ox_sim_get_input_state_vector2f(user_path, component_path, out_value) == OX_SIM_SUCCESS
               ? XR_SUCCESS
               : XR_ERROR_PATH_UNSUPPORTED;
}

static uint32_t simulator_get_interaction_profiles(const char** out_profiles, uint32_t max_count) {
    const DeviceProfile* device_profile = current_profile();
    if (!out_profiles || max_count == 0 || !device_profile) {
        return 0;
    }

    out_profiles[0] = device_profile->interaction_profile;
    return 1;
}

static void simulator_on_session_state_changed(XrSessionState new_state) { sim_notify_session(new_state); }

static void simulator_submit_frame_pixels(XrTime frame_time, uint32_t eye_index, uint32_t width, uint32_t height,
                                          uint32_t format, const void* pixel_data, uint32_t data_size) {
    (void)format;
    sim_submit_frame(frame_time, eye_index, width, height, pixel_data, data_size);
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
    callbacks->get_system_properties = simulator_get_system_properties;
    callbacks->update_view = simulator_update_view;
    callbacks->update_devices = simulator_update_devices;
    callbacks->get_input_state_boolean = simulator_get_input_state_boolean;
    callbacks->get_input_state_float = simulator_get_input_state_float;
    callbacks->get_input_state_vector2f = simulator_get_input_state_vector2f;
    callbacks->get_interaction_profiles = simulator_get_interaction_profiles;
    callbacks->on_session_state_changed = simulator_on_session_state_changed;
    callbacks->submit_frame_pixels = simulator_submit_frame_pixels;

    return 1;
}
