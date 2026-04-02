#include <ox_sim.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <variant>
#include <vector>

#include "device_profiles.hpp"

namespace {

using InputValue = std::variant<bool, float, XrVector2f>;

struct DeviceInputState {
    std::vector<InputValue> values;
};

std::mutex g_mutex;
uint32_t g_init_count = 0;
const ox_sim::DeviceProfile* g_profile = nullptr;
OxDeviceState g_devices[OX_MAX_DEVICES] = {};
uint32_t g_device_count = 0;
DeviceInputState g_inputs[OX_MAX_DEVICES];

std::mutex g_frame_mutex;
std::vector<uint8_t> g_frame_pixels[2];
uint32_t g_frame_sizes[2] = {};
uint32_t g_frame_w = 0;
uint32_t g_frame_h = 0;
std::atomic<XrSessionState> g_session_state{XR_SESSION_STATE_UNKNOWN};
std::atomic<uint32_t> g_app_fps{0};
std::atomic<XrTime> g_frame_time{0};
int64_t g_last_frame_ms = 0;
std::deque<int64_t> g_dt_history;

const ox_sim::DeviceProfile* default_profile() { return &ox_sim::GetDeviceProfile(ox_sim::DeviceType::OCULUS_QUEST_2); }

bool is_initialized_locked() { return g_init_count > 0 && g_profile != nullptr; }

OxSimResult require_initialized();

int find_device_index(const char* user_path) {
    if (!user_path) {
        return -1;
    }

    for (uint32_t index = 0; index < g_device_count; ++index) {
        if (std::strcmp(g_devices[index].user_path, user_path) == 0) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

const ox_sim::DeviceDef* find_device_def(const char* user_path) {
    if (!g_profile || !user_path) {
        return nullptr;
    }

    for (const ox_sim::DeviceDef& device : g_profile->devices) {
        if (std::strcmp(device.user_path, user_path) == 0) {
            return &device;
        }
    }

    return nullptr;
}

const ox_sim::ComponentDef* find_component_def(const ox_sim::DeviceDef* device_def, const char* component_path,
                                               int* out_index = nullptr) {
    if (!device_def || !component_path) {
        return nullptr;
    }

    for (size_t index = 0; index < device_def->components.size(); ++index) {
        const ox_sim::ComponentDef& component = device_def->components[index];
        if (std::strcmp(component.path, component_path) == 0) {
            if (out_index) {
                *out_index = static_cast<int>(index);
            }
            return &component;
        }
    }

    return nullptr;
}

int find_component_index(const ox_sim::DeviceDef* device_def, const char* component_path) {
    int component_index = -1;
    return find_component_def(device_def, component_path, &component_index) ? component_index : -1;
}

void init_devices(const ox_sim::DeviceProfile* profile) {
    g_profile = profile;
    g_device_count = 0;

    for (uint32_t index = 0; index < OX_MAX_DEVICES; ++index) {
        g_devices[index] = {};
        g_inputs[index].values.clear();
    }

    if (!profile) {
        return;
    }

    g_device_count = static_cast<uint32_t>(std::min(profile->devices.size(), static_cast<size_t>(OX_MAX_DEVICES)));
    for (uint32_t index = 0; index < g_device_count; ++index) {
        const ox_sim::DeviceDef& device_def = profile->devices[index];
        std::snprintf(g_devices[index].user_path, sizeof(g_devices[index].user_path), "%s", device_def.user_path);
        g_devices[index].pose = device_def.default_pose;
        g_devices[index].is_active = (device_def.always_active || device_def.default_active) ? XR_TRUE : XR_FALSE;

        g_inputs[index].values.resize(device_def.components.size());
        for (size_t component_index = 0; component_index < device_def.components.size(); ++component_index) {
            switch (device_def.components[component_index].type) {
                case ox_sim::ComponentType::BOOLEAN:
                    g_inputs[index].values[component_index] = false;
                    break;
                case ox_sim::ComponentType::FLOAT:
                    g_inputs[index].values[component_index] = 0.0f;
                    break;
                case ox_sim::ComponentType::VEC2:
                    g_inputs[index].values[component_index] = XrVector2f{0.0f, 0.0f};
                    break;
            }
        }
    }
}

void reset_frame_state() {
    std::lock_guard<std::mutex> frame_lock(g_frame_mutex);
    g_frame_pixels[0].clear();
    g_frame_pixels[1].clear();
    g_frame_sizes[0] = 0;
    g_frame_sizes[1] = 0;
    g_frame_w = 0;
    g_frame_h = 0;
    g_session_state.store(XR_SESSION_STATE_UNKNOWN, std::memory_order_relaxed);
    g_app_fps.store(0, std::memory_order_relaxed);
    g_frame_time.store(0, std::memory_order_relaxed);
    g_last_frame_ms = 0;
    g_dt_history.clear();
}

void update_fps() {
    using namespace std::chrono;

    const int64_t now_ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    if (g_last_frame_ms > 0) {
        const int64_t dt_ms = now_ms - g_last_frame_ms;
        if (dt_ms > 0) {
            g_dt_history.push_back(dt_ms);
            if (g_dt_history.size() > 10) {
                g_dt_history.pop_front();
            }

            int64_t avg_dt_ms = 0;
            for (int64_t dt : g_dt_history) {
                avg_dt_ms += dt;
            }
            avg_dt_ms /= static_cast<int64_t>(g_dt_history.size());
            if (avg_dt_ms > 0) {
                g_app_fps.store(static_cast<uint32_t>(1000.0 / static_cast<double>(avg_dt_ms)),
                                std::memory_order_relaxed);
            }
        }
    }

    g_last_frame_ms = now_ms;
}

// Store a normalized owned RGBA copy for preview consumers.
bool normalize_frame_preview_rgba(const void* data, uint32_t width, uint32_t height, uint32_t size,
                                  std::vector<uint8_t>* out_pixels) {
    if (!data || !out_pixels || width == 0 || height == 0) {
        return false;
    }

    const size_t expected_size = static_cast<size_t>(width) * height * 4;
    if (size != expected_size) {
        return false;
    }

    out_pixels->resize(expected_size);
    const uint8_t* src = static_cast<const uint8_t*>(data);
    std::memcpy(out_pixels->data(), src, expected_size);
    for (size_t offset = 3; offset < expected_size; offset += 4) {
        (*out_pixels)[offset] = 255;
    }

    return true;
}

void sync_float_to_vec2(const char* user_path, const char* component_path) {
    const int device_index = find_device_index(user_path);
    const ox_sim::DeviceDef* device_def = find_device_def(user_path);
    if (device_index < 0 || !device_def) {
        return;
    }

    int source_index = -1;
    const ox_sim::ComponentDef* source = find_component_def(device_def, component_path, &source_index);
    if (!source || source->type != ox_sim::ComponentType::FLOAT || !source->linked_vec2_path ||
        source->linked_axis == ox_sim::Vec2Axis::NONE) {
        return;
    }

    const int vec2_index = find_component_index(device_def, source->linked_vec2_path);
    if (vec2_index < 0) {
        return;
    }

    float axis_value = std::get<float>(g_inputs[device_index].values[source_index]);
    XrVector2f& vec2_value = std::get<XrVector2f>(g_inputs[device_index].values[vec2_index]);
    if (source->linked_axis == ox_sim::Vec2Axis::X) {
        vec2_value.x = axis_value;
    } else {
        vec2_value.y = axis_value;
    }
}

void sync_vec2_to_floats(const char* user_path, const char* component_path) {
    const int device_index = find_device_index(user_path);
    const ox_sim::DeviceDef* device_def = find_device_def(user_path);
    if (device_index < 0 || !device_def) {
        return;
    }

    const int vec2_index = find_component_index(device_def, component_path);
    if (vec2_index < 0) {
        return;
    }

    const XrVector2f vec2_value = std::get<XrVector2f>(g_inputs[device_index].values[vec2_index]);
    for (size_t component_index = 0; component_index < device_def->components.size(); ++component_index) {
        const ox_sim::ComponentDef& component = device_def->components[component_index];
        if (component.type != ox_sim::ComponentType::FLOAT || !component.linked_vec2_path ||
            std::strcmp(component.linked_vec2_path, component_path) != 0) {
            continue;
        }

        g_inputs[device_index].values[component_index] =
            component.linked_axis == ox_sim::Vec2Axis::X ? vec2_value.x : vec2_value.y;
    }
}

OxSimResult require_initialized() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return is_initialized_locked() ? OX_SIM_SUCCESS : OX_SIM_ERROR_NOT_INITIALIZED;
}

OxSimResult validate_component_paths_locked(const char* user_path, const char* component_path,
                                            const ox_sim::DeviceDef** out_device_def = nullptr,
                                            const ox_sim::ComponentDef** out_component_def = nullptr,
                                            int* out_device_index = nullptr, int* out_component_index = nullptr) {
    const int device_index = find_device_index(user_path);
    const ox_sim::DeviceDef* device_def = find_device_def(user_path);
    if (device_index < 0 || !device_def) {
        return OX_SIM_ERROR_DEVICE_NOT_FOUND;
    }

    int component_index = -1;
    const ox_sim::ComponentDef* component_def = find_component_def(device_def, component_path, &component_index);
    if (!component_def) {
        return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
    }

    if (out_device_def) {
        *out_device_def = device_def;
    }
    if (out_component_def) {
        *out_component_def = component_def;
    }
    if (out_device_index) {
        *out_device_index = device_index;
    }
    if (out_component_index) {
        *out_component_index = component_index;
    }
    return OX_SIM_SUCCESS;
}

}  // namespace

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_initialize(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_init_count == 0) {
        if (!g_profile) {
            g_profile = default_profile();
        }
        init_devices(g_profile);
        reset_frame_state();
    }

    ++g_init_count;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT void ox_sim_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_init_count == 0) {
        return;
    }

    --g_init_count;
    if (g_init_count > 0) {
        return;
    }

    g_device_count = 0;
    for (uint32_t index = 0; index < OX_MAX_DEVICES; ++index) {
        g_devices[index] = {};
        g_inputs[index].values.clear();
    }
    reset_frame_state();
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_current_profile(char* out_name, uint32_t out_name_capacity) {
    if (!out_name || out_name_capacity == 0) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const char* profile_name = ox_sim::GetDeviceProfileId(g_profile->type);
    const size_t name_len = std::strlen(profile_name);
    if (name_len + 1 > out_name_capacity) {
        return OX_SIM_ERROR_BUFFER_TOO_SMALL;
    }

    std::memcpy(out_name, profile_name, name_len + 1);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_current_profile(const char* profile_name) {
    if (!profile_name) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const ox_sim::DeviceProfile* profile = ox_sim::GetDeviceProfileByName(profile_name);
    if (!profile) {
        return OX_SIM_ERROR_PROFILE_NOT_FOUND;
    }

    init_devices(profile);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_count(uint32_t* out_count) {
    if (!out_count) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    *out_count = g_device_count;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_state(uint32_t device_index, OxDeviceState* out_state) {
    if (!out_state) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }
    if (device_index >= g_device_count) {
        return OX_SIM_ERROR_DEVICE_NOT_FOUND;
    }

    *out_state = g_devices[device_index];
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_pose(const char* user_path, XrPosef* out_pose,
                                                               XrBool32* out_is_active) {
    if (!user_path || !out_pose || !out_is_active) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const int device_index = find_device_index(user_path);
    if (device_index < 0) {
        return OX_SIM_ERROR_DEVICE_NOT_FOUND;
    }

    *out_pose = g_devices[device_index].pose;
    *out_is_active = g_devices[device_index].is_active;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_device_pose(const char* user_path, const XrPosef* pose,
                                                               XrBool32 is_active) {
    if (!user_path || !pose) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const int device_index = find_device_index(user_path);
    const ox_sim::DeviceDef* device_def = find_device_def(user_path);
    if (device_index < 0 || !device_def) {
        return OX_SIM_ERROR_DEVICE_NOT_FOUND;
    }

    g_devices[device_index].pose = *pose;
    g_devices[device_index].is_active = device_def->always_active ? XR_TRUE : (is_active ? XR_TRUE : XR_FALSE);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_state_boolean(const char* user_path,
                                                                       const char* component_path,
                                                                       uint32_t* out_value) {
    if (!user_path || !component_path || !out_value) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const ox_sim::ComponentDef* component_def = nullptr;
    int device_index = -1;
    int component_index = -1;
    OxSimResult result = validate_component_paths_locked(user_path, component_path, nullptr, &component_def,
                                                         &device_index, &component_index);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }

    if (component_def->type == ox_sim::ComponentType::BOOLEAN) {
        *out_value = std::get<bool>(g_inputs[device_index].values[component_index]) ? 1u : 0u;
        return OX_SIM_SUCCESS;
    }
    if (component_def->type == ox_sim::ComponentType::FLOAT) {
        *out_value = std::get<float>(g_inputs[device_index].values[component_index]) >= 0.5f ? 1u : 0u;
        return OX_SIM_SUCCESS;
    }

    return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_state_boolean(const char* user_path,
                                                                       const char* component_path, uint32_t value) {
    if (!user_path || !component_path) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const ox_sim::ComponentDef* component_def = nullptr;
    int device_index = -1;
    int component_index = -1;
    OxSimResult result = validate_component_paths_locked(user_path, component_path, nullptr, &component_def,
                                                         &device_index, &component_index);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }

    if (component_def->type == ox_sim::ComponentType::BOOLEAN) {
        g_inputs[device_index].values[component_index] = value != 0;
        return OX_SIM_SUCCESS;
    }
    if (component_def->type == ox_sim::ComponentType::FLOAT) {
        g_inputs[device_index].values[component_index] = value != 0 ? 1.0f : 0.0f;
        sync_float_to_vec2(user_path, component_path);
        return OX_SIM_SUCCESS;
    }

    return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_state_float(const char* user_path, const char* component_path,
                                                                     float* out_value) {
    if (!user_path || !component_path || !out_value) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const ox_sim::ComponentDef* component_def = nullptr;
    int device_index = -1;
    int component_index = -1;
    OxSimResult result = validate_component_paths_locked(user_path, component_path, nullptr, &component_def,
                                                         &device_index, &component_index);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }

    if (component_def->type == ox_sim::ComponentType::FLOAT) {
        *out_value = std::get<float>(g_inputs[device_index].values[component_index]);
        return OX_SIM_SUCCESS;
    }
    if (component_def->type == ox_sim::ComponentType::BOOLEAN) {
        *out_value = std::get<bool>(g_inputs[device_index].values[component_index]) ? 1.0f : 0.0f;
        return OX_SIM_SUCCESS;
    }

    return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_state_float(const char* user_path, const char* component_path,
                                                                     float value) {
    if (!user_path || !component_path) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const ox_sim::ComponentDef* component_def = nullptr;
    int device_index = -1;
    int component_index = -1;
    OxSimResult result = validate_component_paths_locked(user_path, component_path, nullptr, &component_def,
                                                         &device_index, &component_index);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }

    if (component_def->type == ox_sim::ComponentType::FLOAT) {
        g_inputs[device_index].values[component_index] = value;
        sync_float_to_vec2(user_path, component_path);
        return OX_SIM_SUCCESS;
    }
    if (component_def->type == ox_sim::ComponentType::BOOLEAN) {
        g_inputs[device_index].values[component_index] = value >= 0.5f;
        return OX_SIM_SUCCESS;
    }

    return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_state_vector2f(const char* user_path,
                                                                        const char* component_path,
                                                                        XrVector2f* out_value) {
    if (!user_path || !component_path || !out_value) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const ox_sim::ComponentDef* component_def = nullptr;
    int device_index = -1;
    int component_index = -1;
    OxSimResult result = validate_component_paths_locked(user_path, component_path, nullptr, &component_def,
                                                         &device_index, &component_index);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (component_def->type != ox_sim::ComponentType::VEC2) {
        return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
    }

    *out_value = std::get<XrVector2f>(g_inputs[device_index].values[component_index]);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_state_vector2f(const char* user_path,
                                                                        const char* component_path,
                                                                        const XrVector2f* value) {
    if (!user_path || !component_path || !value) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const ox_sim::ComponentDef* component_def = nullptr;
    int device_index = -1;
    int component_index = -1;
    OxSimResult result = validate_component_paths_locked(user_path, component_path, nullptr, &component_def,
                                                         &device_index, &component_index);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (component_def->type != ox_sim::ComponentType::VEC2) {
        return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
    }

    g_inputs[device_index].values[component_index] = *value;
    sync_vec2_to_floats(user_path, component_path);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_session_state(XrSessionState* out_state) {
    if (!out_state) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }
    if (require_initialized() != OX_SIM_SUCCESS) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    *out_state = g_session_state.load(std::memory_order_relaxed);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_app_fps(uint32_t* out_fps) {
    if (!out_fps) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }
    if (require_initialized() != OX_SIM_SUCCESS) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    *out_fps = g_app_fps.load(std::memory_order_relaxed);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_frame_preview(OxSimFramePreview* out_preview) {
    if (!out_preview) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }
    if (require_initialized() != OX_SIM_SUCCESS) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    // Thread-local snapshots: the deep copy happens under g_frame_mutex so the
    // returned pointers are owned by the calling thread and remain valid after
    // the lock is released, even while sim_submit_frame overwrites g_frame_pixels.
    thread_local std::vector<uint8_t> tl_pixels[2];

    std::lock_guard<std::mutex> lock(g_frame_mutex);
    for (int i = 0; i < 2; ++i) {
        tl_pixels[i] = g_frame_pixels[i];
    }
    out_preview->pixel_data[0] = tl_pixels[0].empty() ? nullptr : tl_pixels[0].data();
    out_preview->pixel_data[1] = tl_pixels[1].empty() ? nullptr : tl_pixels[1].data();
    out_preview->data_size[0] = g_frame_sizes[0];
    out_preview->data_size[1] = g_frame_sizes[1];
    out_preview->width = g_frame_w;
    out_preview->height = g_frame_h;
    out_preview->app_fps = g_app_fps.load(std::memory_order_relaxed);
    out_preview->session_state = g_session_state.load(std::memory_order_relaxed);
    out_preview->frame_time = g_frame_time.load(std::memory_order_relaxed);
    return OX_SIM_SUCCESS;
}

extern "C" void sim_submit_frame(XrTime frame_time, uint32_t eye, uint32_t w, uint32_t h, const void* data,
                                 uint32_t size) {
    if (eye >= 2 || w == 0 || h == 0 || !data || size == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_frame_mutex);
    if (!normalize_frame_preview_rgba(data, w, h, size, &g_frame_pixels[eye])) {
        return;
    }

    g_frame_w = w;
    g_frame_h = h;
    g_frame_sizes[eye] = static_cast<uint32_t>(g_frame_pixels[eye].size());
    if (eye == 0) {
        update_fps();
    }
    g_frame_time.store(frame_time, std::memory_order_relaxed);
}

extern "C" void sim_notify_session(XrSessionState state) { g_session_state.store(state, std::memory_order_relaxed); }

extern "C" void sim_copy_devices(OxDeviceState* out, uint32_t max, uint32_t* out_count) {
    if (!out_count) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        *out_count = 0;
        return;
    }

    const uint32_t copy_count = std::min(g_device_count, max);
    if (out) {
        for (uint32_t index = 0; index < copy_count; ++index) {
            out[index] = g_devices[index];
        }
    }
    *out_count = copy_count;
}