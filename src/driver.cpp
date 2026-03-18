#include <ox_driver.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include "device_profiles.h"
#include "simulator_core.h"
#include "simulator_runtime.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace ox_sim;

static OxVector3f rotate_vector_by_quat(const OxQuaternion& q, const OxVector3f& v);

// ===== Driver Callbacks =====

static int simulator_initialize(void) {
    spdlog::info("=== ox Simulator Driver ===");

    if (!AcquireSimulatorRuntime()) {
        spdlog::error("Failed to initialize simulator core");
        return 0;
    }

    if (!StartSimulatorGui()) {
        spdlog::error("Failed to start GUI window");
        ReleaseSimulatorRuntime();
        return 0;
    }

    spdlog::info("Simulator driver initialized successfully");
    return 1;
}

static void simulator_shutdown(void) {
    spdlog::info("Shutting down simulator driver...");

    StopSimulatorGui();
    ReleaseSimulatorRuntime();

    spdlog::info("Simulator driver shut down");
}

static int simulator_is_device_connected(void) {
    // Simulator is always "connected"
    return 1;
}

static int simulator_is_driver_running(void) {
    // Check if the GUI window is still running
    if (!IsSimulatorGuiRunning()) {
        spdlog::info("Simulator GUI window is closed.");
        return 0;
    }
    return 1;
}

static void simulator_get_device_info(OxDeviceInfo* info) {
    const DeviceProfile* device_profile = GetCurrentDeviceProfile();
    if (!device_profile) {
        return;
    }

    snprintf(info->name, sizeof(info->name), "%s", device_profile->name);

    snprintf(info->manufacturer, sizeof(info->manufacturer), "%s", device_profile->manufacturer);

    // Generate unique serial number
    std::string serial = std::string(device_profile->serial_prefix) + "-12345";
    snprintf(info->serial, sizeof(info->serial), "%s", serial.c_str());

    info->vendor_id = device_profile->vendor_id;
    info->product_id = device_profile->product_id;
}

static void simulator_get_display_properties(OxDisplayProperties* props) {
    const DeviceProfile* device_profile = GetCurrentDeviceProfile();
    if (!device_profile) {
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
    const DeviceProfile* device_profile = GetCurrentDeviceProfile();
    if (!device_profile) {
        return;
    }

    caps->has_position_tracking = device_profile->has_position_tracking ? 1 : 0;
    caps->has_orientation_tracking = device_profile->has_orientation_tracking ? 1 : 0;
}

static void simulator_update_view_pose(int64_t predicted_time, uint32_t eye_index, OxPose* out_pose) {
    // Get HMD pose from device list (HMD is at /user/head)
    OxDeviceState devices[OX_MAX_DEVICES];
    uint32_t device_count;
    GetSimulatorCore().UpdateAllDevices(devices, &device_count);

    // Find HMD device
    OxPose hmd_pose = {{0.0f, 1.6f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};  // Default origin at eye level
    for (uint32_t i = 0; i < device_count; i++) {
        if (std::strcmp(devices[i].user_path, "/user/head") == 0) {
            hmd_pose = devices[i].pose;
            break;
        }
    }
    // If no HMD found (e.g., vive tracker profile), just use origin
    // Most tracker applications don't render, so view pose doesn't matter

    // Apply IPD offset (typical IPD is ~63mm = 0.063m)
    float ipd = 0.063f;
    float eye_offset = (eye_index == 0) ? -ipd / 2.0f : ipd / 2.0f;

    // Apply the IPD offset in head-local space by rotating the local X offset
    OxVector3f eye_local = {eye_offset, 0.0f, 0.0f};
    OxVector3f rotated_offset = rotate_vector_by_quat(hmd_pose.orientation, eye_local);

    *out_pose = hmd_pose;
    out_pose->position.x += rotated_offset.x;
    out_pose->position.y += rotated_offset.y;
    out_pose->position.z += rotated_offset.z;
}

static void simulator_update_devices(int64_t predicted_time, OxDeviceState* out_states, uint32_t* out_count) {
    if (!GetCurrentDeviceProfile()) {
        *out_count = 0;
        return;
    }

    GetSimulatorCore().UpdateAllDevices(out_states, out_count);
}

static OxComponentResult simulator_get_input_state_boolean(int64_t predicted_time, const char* user_path,
                                                           const char* component_path, uint32_t* out_value) {
    if (!GetCurrentDeviceProfile()) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    bool value;
    OxComponentResult result = GetSimulatorCore().GetInputStateBoolean(user_path, component_path, &value);
    *out_value = value ? 1 : 0;
    return result;
}

static OxComponentResult simulator_get_input_state_float(int64_t predicted_time, const char* user_path,
                                                         const char* component_path, float* out_value) {
    if (!GetCurrentDeviceProfile()) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    return GetSimulatorCore().GetInputStateFloat(user_path, component_path, out_value);
}

static OxComponentResult simulator_get_input_state_vector2f(int64_t predicted_time, const char* user_path,
                                                            const char* component_path, OxVector2f* out_value) {
    if (!GetCurrentDeviceProfile()) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    OxVector2f vec;
    OxComponentResult result = GetSimulatorCore().GetInputStateVec2(user_path, component_path, &vec);
    *out_value = vec;
    return result;
}

static uint32_t simulator_get_interaction_profiles(const char** out_profiles, uint32_t max_count) {
    const DeviceProfile* device_profile = GetCurrentDeviceProfile();
    if (!device_profile || max_count == 0) {
        return 0;
    }

    out_profiles[0] = device_profile->interaction_profile;
    return 1;
}

static void simulator_on_session_state_changed(OxSessionState new_state) {
    GetFrameData()->session_state.store(static_cast<uint32_t>(new_state), std::memory_order_relaxed);
}

static void simulator_submit_frame_pixels(uint32_t eye_index, uint32_t width, uint32_t height, uint32_t format,
                                          const void* pixel_data, uint32_t data_size) {
    if (eye_index >= 2 || width == 0 || height == 0 || !pixel_data || data_size == 0) {
        spdlog::error("[Driver] submit_frame_pixels: Invalid parameters (eye={} size={}x{} data_size={})", eye_index,
                      width, height, data_size);
        return;
    }

    // Store frame data pointers for GUI preview (zero-copy - use shared memory directly)
    {
        FrameData* frame_data = GetFrameData();
        std::lock_guard<std::mutex> lock(frame_data->mutex);

        // Update dimensions on first frame or size change
        if (frame_data->width != width || frame_data->height != height) {
            frame_data->width = width;
            frame_data->height = height;
            spdlog::info("[Driver] Frame dimensions set to {}x{}", width, height);
        }

        // Store the shared memory pointer
        frame_data->pixel_data[eye_index] = pixel_data;
        frame_data->data_size[eye_index] = data_size;

        // App FPS computation
        if (eye_index == 0) {
            frame_data->UpdateFps();
        }

        // Mark that new frame is available
        frame_data->has_new_frame.store(true, std::memory_order_release);
    }
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

// Rotate a vector `v` by quaternion `q` (assumes q is a unit quaternion).
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
