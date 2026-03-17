#include "simulator_api.h"

#include <cstring>
#include <mutex>
#include <new>
#include <string>

#include "device_profiles.h"
#include "simulator_runtime.h"

struct OxSimContext {
    bool initialized = false;
};

namespace {

OxSimResult RequireContext(OxSimContext* context) { return context ? OX_SIM_SUCCESS : OX_SIM_ERROR_INVALID_ARGUMENT; }

OxSimResult RequireInitialized(OxSimContext* context) {
    if (!context) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }
    if (!context->initialized || !ox_sim::IsSimulatorRuntimeInitialized()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }
    return OX_SIM_SUCCESS;
}

OxSimResult ValidateComponent(const char* user_path, const char* component_path) {
    const ox_sim::DeviceDef* device = ox_sim::GetSimulatorCore().FindDeviceDefByUserPath(user_path);
    if (!device) {
        return OX_SIM_ERROR_DEVICE_NOT_FOUND;
    }

    auto [component_index, component_type] = ox_sim::GetSimulatorCore().FindComponentInfo(device, component_path);
    (void)component_type;
    return component_index >= 0 ? OX_SIM_SUCCESS : OX_SIM_ERROR_COMPONENT_NOT_FOUND;
}

}  // namespace

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_create_context(OxSimContext** out_context) {
    if (!out_context) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    OxSimContext* context = new (std::nothrow) OxSimContext();
    if (!context) {
        return OX_SIM_ERROR_INITIALIZATION_FAILED;
    }

    *out_context = context;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT void ox_sim_destroy_context(OxSimContext* context) {
    if (!context) {
        return;
    }

    ox_sim_shutdown(context);
    delete context;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_initialize(OxSimContext* context) {
    OxSimResult result = RequireContext(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }

    if (context->initialized) {
        return OX_SIM_SUCCESS;
    }

    if (!ox_sim::AcquireSimulatorRuntime()) {
        return OX_SIM_ERROR_INITIALIZATION_FAILED;
    }

    context->initialized = true;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT void ox_sim_shutdown(OxSimContext* context) {
    if (!context || !context->initialized) {
        return;
    }

    ox_sim::ReleaseSimulatorRuntime();
    context->initialized = false;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_current_profile(OxSimContext* context, char* out_name,
                                                                   uint32_t out_name_capacity) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!out_name || out_name_capacity == 0) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    const ox_sim::DeviceProfile* profile = ox_sim::GetCurrentDeviceProfile();
    if (!profile) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const char* profile_name = ox_sim::GetDeviceProfileId(profile->type);
    size_t profile_name_len = std::strlen(profile_name);
    if (profile_name_len + 1 > out_name_capacity) {
        return OX_SIM_ERROR_BUFFER_TOO_SMALL;
    }

    std::memcpy(out_name, profile_name, profile_name_len + 1);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_current_profile(OxSimContext* context, const char* profile_name) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!profile_name) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    const ox_sim::DeviceProfile* profile = ox_sim::GetDeviceProfileByName(profile_name);
    if (!profile) {
        return OX_SIM_ERROR_PROFILE_NOT_FOUND;
    }
    if (!ox_sim::SetCurrentDeviceProfile(profile)) {
        return OX_SIM_ERROR_INITIALIZATION_FAILED;
    }

    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_count(OxSimContext* context, uint32_t* out_count) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!out_count) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    OxDeviceState devices[OX_MAX_DEVICES];
    ox_sim::GetSimulatorCore().UpdateAllDevices(devices, out_count);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_state(OxSimContext* context, uint32_t device_index,
                                                                OxDeviceState* out_state) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!out_state) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    OxDeviceState devices[OX_MAX_DEVICES];
    uint32_t device_count = 0;
    ox_sim::GetSimulatorCore().UpdateAllDevices(devices, &device_count);
    if (device_index >= device_count) {
        return OX_SIM_ERROR_DEVICE_NOT_FOUND;
    }

    *out_state = devices[device_index];
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_pose(OxSimContext* context, const char* user_path,
                                                               OxPose* out_pose, uint32_t* out_is_active) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!user_path || !out_pose || !out_is_active) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    bool is_active = false;
    if (!ox_sim::GetSimulatorCore().GetDevicePose(user_path, out_pose, &is_active)) {
        return OX_SIM_ERROR_DEVICE_NOT_FOUND;
    }

    *out_is_active = is_active ? 1u : 0u;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_device_pose(OxSimContext* context, const char* user_path,
                                                               const OxPose* pose, uint32_t is_active) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!user_path || !pose) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }
    if (!ox_sim::GetSimulatorCore().FindDeviceDefByUserPath(user_path)) {
        return OX_SIM_ERROR_DEVICE_NOT_FOUND;
    }

    ox_sim::GetSimulatorCore().SetDevicePose(user_path, *pose, is_active != 0);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_state_boolean(OxSimContext* context, const char* user_path,
                                                                       const char* component_path,
                                                                       uint32_t* out_value) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!user_path || !component_path || !out_value) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    bool value = false;
    if (ox_sim::GetSimulatorCore().GetInputStateBoolean(user_path, component_path, &value) != OX_COMPONENT_AVAILABLE) {
        return ValidateComponent(user_path, component_path);
    }

    *out_value = value ? 1u : 0u;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_state_boolean(OxSimContext* context, const char* user_path,
                                                                       const char* component_path, uint32_t value) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!user_path || !component_path) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    result = ValidateComponent(user_path, component_path);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }

    ox_sim::GetSimulatorCore().SetInputStateBoolean(user_path, component_path, value != 0);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_state_float(OxSimContext* context, const char* user_path,
                                                                     const char* component_path, float* out_value) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!user_path || !component_path || !out_value) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    if (ox_sim::GetSimulatorCore().GetInputStateFloat(user_path, component_path, out_value) != OX_COMPONENT_AVAILABLE) {
        return ValidateComponent(user_path, component_path);
    }

    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_state_float(OxSimContext* context, const char* user_path,
                                                                     const char* component_path, float value) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!user_path || !component_path) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    result = ValidateComponent(user_path, component_path);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }

    ox_sim::GetSimulatorCore().SetInputStateFloat(user_path, component_path, value);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_state_vector2f(OxSimContext* context, const char* user_path,
                                                                        const char* component_path,
                                                                        OxVector2f* out_value) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!user_path || !component_path || !out_value) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    if (ox_sim::GetSimulatorCore().GetInputStateVec2(user_path, component_path, out_value) != OX_COMPONENT_AVAILABLE) {
        return ValidateComponent(user_path, component_path);
    }

    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_state_vector2f(OxSimContext* context, const char* user_path,
                                                                        const char* component_path,
                                                                        const OxVector2f* value) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!user_path || !component_path || !value) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    result = ValidateComponent(user_path, component_path);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }

    ox_sim::GetSimulatorCore().SetInputStateVec2(user_path, component_path, *value);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_session_state(OxSimContext* context, OxSessionState* out_state) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!out_state) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    *out_state = static_cast<OxSessionState>(ox_sim::GetFrameData()->session_state.load(std::memory_order_relaxed));
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_app_fps(OxSimContext* context, uint32_t* out_fps) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!out_fps) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    *out_fps = ox_sim::GetFrameData()->app_fps.load(std::memory_order_relaxed);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_frame_preview(OxSimContext* context,
                                                                 OxSimFramePreview* out_preview) {
    OxSimResult result = RequireInitialized(context);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (!out_preview) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    ox_sim::FrameData* frame_data = ox_sim::GetFrameData();
    {
        std::lock_guard<std::mutex> lock(frame_data->mutex);
        out_preview->pixel_data[0] = frame_data->pixel_data[0];
        out_preview->pixel_data[1] = frame_data->pixel_data[1];
        out_preview->data_size[0] = frame_data->data_size[0];
        out_preview->data_size[1] = frame_data->data_size[1];
        out_preview->width = frame_data->width;
        out_preview->height = frame_data->height;
    }

    out_preview->has_new_frame = frame_data->has_new_frame.load(std::memory_order_relaxed) ? 1u : 0u;
    out_preview->app_fps = frame_data->app_fps.load(std::memory_order_relaxed);
    out_preview->session_state = static_cast<OxSessionState>(frame_data->session_state.load(std::memory_order_relaxed));
    return OX_SIM_SUCCESS;
}