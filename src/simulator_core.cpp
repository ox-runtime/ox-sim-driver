#include "simulator_core.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace ox_sim {

SimulatorCore::SimulatorCore() : profile_(nullptr), state_{} { state_.device_count = 0; }

SimulatorCore::~SimulatorCore() { Shutdown(); }

// Helper to find device index in state by user path
int SimulatorCore::FindDeviceIndexByUserPath(const char* user_path) const {
    for (uint32_t i = 0; i < state_.device_count && i < OX_MAX_DEVICES; i++) {
        if (std::strcmp(state_.devices[i].user_path, user_path) == 0) {
            return i;
        }
    }
    return -1;
}

// Helper to find device definition in profile by user path
const DeviceDef* SimulatorCore::FindDeviceDefByUserPath(const char* user_path) const {
    if (!profile_) {
        return nullptr;
    }

    for (const auto& dev : profile_->devices) {
        if (std::strcmp(dev.user_path, user_path) == 0) {
            return &dev;
        }
    }
    return nullptr;
}

// Helper to find component information in device definition
std::pair<int32_t, ComponentType> SimulatorCore::FindComponentInfo(const DeviceDef* device_def,
                                                                   const char* component_path) const {
    if (!device_def) {
        return {-1, ComponentType::FLOAT};
    }

    int32_t index = 0;
    for (const auto& comp : device_def->components) {
        if (std::strcmp(comp.path, component_path) == 0) {
            return {index, comp.type};
        }
        index++;
    }
    return {-1, ComponentType::FLOAT};
}

// Helper to validate device and component, returning input state pointer and component info
std::tuple<DeviceInputState*, int32_t, ComponentType> SimulatorCore::ValidateDeviceAndComponent(
    const char* user_path, const char* component_path) {
    int device_index = FindDeviceIndexByUserPath(user_path);
    const DeviceDef* device_def = FindDeviceDefByUserPath(user_path);
    if (device_index < 0 || !device_def) {
        return {nullptr, -1, ComponentType::FLOAT};
    }

    auto [comp_index, comp_type] = FindComponentInfo(device_def, component_path);
    if (comp_index == -1) {
        return {nullptr, -1, ComponentType::FLOAT};
    }

    return {&state_.device_inputs[device_index], comp_index, comp_type};
}

bool SimulatorCore::Initialize(const DeviceProfile* profile) {
    if (!profile) {
        return false;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    profile_ = profile;

    // Initialize devices from profile
    state_.device_count = static_cast<uint32_t>(std::min(static_cast<size_t>(OX_MAX_DEVICES), profile->devices.size()));

    for (uint32_t i = 0; i < state_.device_count; i++) {
        const DeviceDef& dev_def = profile->devices[i];

        // Set user path
        snprintf(state_.devices[i].user_path, sizeof(state_.devices[i].user_path), "%s", dev_def.user_path);

        // Set active state
        state_.devices[i].is_active = dev_def.always_active ? 1 : 0;

        // Set default pose from profile
        state_.devices[i].pose = dev_def.default_pose;

        // Initialize input state (all components to zero/false)
        size_t max_component_index = dev_def.components.size();
        state_.device_inputs[i].values.resize(max_component_index);

        // Pre-populate all components for this device
        for (size_t comp_index = 0; comp_index < dev_def.components.size(); ++comp_index) {
            const auto& component = dev_def.components[comp_index];
            switch (component.type) {
                case ComponentType::FLOAT:
                    state_.device_inputs[i].values[comp_index] = 0.0f;
                    break;
                case ComponentType::BOOLEAN:
                    state_.device_inputs[i].values[comp_index] = false;
                    break;
                case ComponentType::VEC2:
                    state_.device_inputs[i].values[comp_index] = OxVector2f{0.0f, 0.0f};
                    break;
            }
        }
    }

    return true;
}

bool SimulatorCore::SwitchDevice(const DeviceProfile* profile) {
    Shutdown();
    return Initialize(profile);
}

void SimulatorCore::Shutdown() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    profile_ = nullptr;
    state_.device_count = 0;
}

void SimulatorCore::UpdateAllDevices(OxDeviceState* out_states, uint32_t* out_count) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    *out_count = state_.device_count;
    for (uint32_t i = 0; i < state_.device_count && i < OX_MAX_DEVICES; i++) {
        out_states[i] = state_.devices[i];
    }
}

bool SimulatorCore::GetDevicePose(const char* user_path, OxPose* out_pose, bool* out_is_active) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    int device_index = FindDeviceIndexByUserPath(user_path);
    if (device_index < 0) {
        return false;
    }

    *out_pose = state_.devices[device_index].pose;
    *out_is_active = state_.devices[device_index].is_active != 0;
    return true;
}

void SimulatorCore::SetDevicePose(const char* user_path, const OxPose& pose, bool is_active) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Find the device by user path
    int device_index = FindDeviceIndexByUserPath(user_path);
    const DeviceDef* device_def = FindDeviceDefByUserPath(user_path);
    if (device_index < 0) {
        return;
    }

    state_.devices[device_index].pose = pose;
    bool device_always_active = device_def ? device_def->always_active : false;
    state_.devices[device_index].is_active = device_always_active ? 1 : (is_active ? 1 : 0);
}

template <ComponentType CT, typename T>
OxComponentResult SimulatorCore::GetInputState(const char* user_path, const char* component_path, T* out_value) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto [input, comp_index, comp_type] = ValidateDeviceAndComponent(user_path, component_path);
    if (!input || comp_index == -1) {
        return OX_COMPONENT_UNAVAILABLE;
    }

    if (comp_type == CT) {
        *out_value = std::get<T>(input->values[comp_index]);
        return OX_COMPONENT_AVAILABLE;
    } else if (CT == ComponentType::FLOAT && comp_type == ComponentType::BOOLEAN) {
        if constexpr (CT == ComponentType::FLOAT) {
            bool val = std::get<bool>(input->values[comp_index]);
            *out_value = val ? 1.0f : 0.0f;
        }
        return OX_COMPONENT_AVAILABLE;
    } else if (CT == ComponentType::BOOLEAN && comp_type == ComponentType::FLOAT) {
        if constexpr (CT == ComponentType::BOOLEAN) {
            float val = std::get<float>(input->values[comp_index]);
            *out_value = (val >= 0.5f) ? true : false;
        }
        return OX_COMPONENT_AVAILABLE;
    } else {
        return OX_COMPONENT_UNAVAILABLE;
    }
}

template <ComponentType CT, typename T>
void SimulatorCore::SetInputState(const char* user_path, const char* component_path, const T& value) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto [input, comp_index, comp_type] = ValidateDeviceAndComponent(user_path, component_path);
    if (!input || comp_index == -1) {
        return;
    }

    if (comp_type == CT) {
        input->values[comp_index] = value;
    } else if (CT == ComponentType::FLOAT && comp_type == ComponentType::BOOLEAN) {
        if constexpr (CT == ComponentType::FLOAT) {
            input->values[comp_index] = (value >= 0.5f) ? true : false;
        }
    } else if (CT == ComponentType::BOOLEAN && comp_type == ComponentType::FLOAT) {
        if constexpr (CT == ComponentType::BOOLEAN) {
            input->values[comp_index] = value ? 1.0f : 0.0f;
        }
    } else {
        return;
    }
}

OxComponentResult SimulatorCore::GetInputStateBoolean(const char* user_path, const char* component_path,
                                                      bool* out_value) {
    return GetInputState<ComponentType::BOOLEAN, bool>(user_path, component_path, out_value);
}

OxComponentResult SimulatorCore::GetInputStateFloat(const char* user_path, const char* component_path,
                                                    float* out_value) {
    return GetInputState<ComponentType::FLOAT, float>(user_path, component_path, out_value);
}

OxComponentResult SimulatorCore::GetInputStateVec2(const char* user_path, const char* component_path,
                                                   OxVector2f* out_value) {
    return GetInputState<ComponentType::VEC2, OxVector2f>(user_path, component_path, out_value);
}

void SimulatorCore::SetInputStateBoolean(const char* user_path, const char* component_path, bool value) {
    SetInputState<ComponentType::BOOLEAN, bool>(user_path, component_path, value);
}

void SimulatorCore::SetInputStateFloat(const char* user_path, const char* component_path, float value) {
    SetInputState<ComponentType::FLOAT, float>(user_path, component_path, value);
    SyncLinkedVec2FromFloat(user_path, component_path);
}

void SimulatorCore::SetInputStateVec2(const char* user_path, const char* component_path, const OxVector2f& value) {
    SetInputState<ComponentType::VEC2, OxVector2f>(user_path, component_path, value);
    SyncLinkedFloatsFromVec2(user_path, component_path);
}

// ---------------------------------------------------------------------------
// Linked-component sync helpers
// ---------------------------------------------------------------------------

// After a FLOAT axis component is set, propagate the new value into its parent
// VEC2 component (if one is declared via linked_vec2_path / linked_axis).
void SimulatorCore::SyncLinkedVec2FromFloat(const char* user_path, const char* component_path) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    const DeviceDef* dev_def = FindDeviceDefByUserPath(user_path);
    if (!dev_def) return;

    // Find source component definition
    const ComponentDef* src_def = nullptr;
    for (const auto& c : dev_def->components) {
        if (std::strcmp(c.path, component_path) == 0) {
            src_def = &c;
            break;
        }
    }
    if (!src_def || !src_def->linked_vec2_path || src_def->linked_axis == Vec2Axis::NONE) return;
    if (src_def->type != ComponentType::FLOAT) return;

    // Find device input state
    int device_index = FindDeviceIndexByUserPath(user_path);
    if (device_index < 0) return;
    DeviceInputState& inputs = state_.device_inputs[device_index];

    // Get updated float value
    auto [float_idx, float_type] = FindComponentInfo(dev_def, component_path);
    if (float_idx == -1 || float_type != ComponentType::FLOAT) return;
    float axis_val = std::get<float>(inputs.values[float_idx]);

    // Find the VEC2 component and patch the appropriate axis
    auto [vec2_idx, vec2_type] = FindComponentInfo(dev_def, src_def->linked_vec2_path);
    if (vec2_idx == -1 || vec2_type != ComponentType::VEC2) return;
    OxVector2f& vec2_val = std::get<OxVector2f>(inputs.values[vec2_idx]);
    if (src_def->linked_axis == Vec2Axis::X)
        vec2_val.x = axis_val;
    else
        vec2_val.y = axis_val;
}

// After a VEC2 component is set, propagate x / y into the FLOAT axis components
// that declare themselves as linked to this VEC2.
void SimulatorCore::SyncLinkedFloatsFromVec2(const char* user_path, const char* component_path) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    const DeviceDef* dev_def = FindDeviceDefByUserPath(user_path);
    if (!dev_def) return;

    int device_index = FindDeviceIndexByUserPath(user_path);
    if (device_index < 0) return;
    DeviceInputState& inputs = state_.device_inputs[device_index];

    // Get the new VEC2 value
    auto [vec2_idx, vec2_type] = FindComponentInfo(dev_def, component_path);
    if (vec2_idx == -1 || vec2_type != ComponentType::VEC2) return;
    const OxVector2f vec2_val = std::get<OxVector2f>(inputs.values[vec2_idx]);

    // Update every FLOAT component that links to this VEC2
    int32_t idx = 0;
    for (const auto& c : dev_def->components) {
        if (c.type == ComponentType::FLOAT && c.linked_vec2_path != nullptr &&
            std::strcmp(c.linked_vec2_path, component_path) == 0 && c.linked_axis != Vec2Axis::NONE) {
            float new_val = (c.linked_axis == Vec2Axis::X) ? vec2_val.x : vec2_val.y;
            inputs.values[idx] = new_val;
        }
        idx++;
    }
}
}  // namespace ox_sim