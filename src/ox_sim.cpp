#include <ox_sim.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <variant>
#include <vector>

#include "device_profiles.hpp"

namespace {

using InputValue = std::variant<bool, float, XrVector2f>;

struct DeviceInputState {
    std::vector<InputValue> values;
};

struct StoredView {
    OxSimViewInfo info = {};
    std::vector<uint8_t> pixels;
};

std::mutex g_mutex;
uint32_t g_init_count = 0;
const ox_sim::DeviceProfile* g_profile = nullptr;
OxDeviceState g_devices[OX_MAX_DEVICES] = {};
uint32_t g_device_count = 0;
DeviceInputState g_inputs[OX_MAX_DEVICES];

std::mutex g_frame_mutex;
std::vector<StoredView> g_frame_views;
std::atomic<XrSessionState> g_session_state{XR_SESSION_STATE_UNKNOWN};
std::atomic<uint32_t> g_fps{0};
int64_t g_last_frame_ms = 0;
std::deque<int64_t> g_dt_history;

const ox_sim::DeviceProfile* default_profile() { return &ox_sim::GetDeviceProfile(ox_sim::DeviceType::OCULUS_QUEST_2); }

bool is_initialized_locked() { return g_init_count > 0 && g_profile != nullptr; }

OxSimResult require_initialized();

bool is_session_active(XrSessionState state) {
    return state == XR_SESSION_STATE_SYNCHRONIZED || state == XR_SESSION_STATE_VISIBLE ||
           state == XR_SESSION_STATE_FOCUSED;
}

uint32_t view_count_for_profile(const ox_sim::DeviceProfile* profile) {
    if (!profile) {
        return 0;
    }
    return profile->view_count;
}

template <size_t N>
void copy_string(char (&dst)[N], const char* src) {
    std::snprintf(dst, N, "%s", src ? src : "");
}

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

void resize_frame_views(const ox_sim::DeviceProfile* profile) {
    std::lock_guard<std::mutex> frame_lock(g_frame_mutex);
    g_frame_views.assign(view_count_for_profile(profile), {});
}

void init_devices(const ox_sim::DeviceProfile* profile) {
    g_profile = profile;
    g_device_count = 0;
    resize_frame_views(profile);

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
    g_frame_views.assign(view_count_for_profile(g_profile), {});
    g_session_state.store(XR_SESSION_STATE_UNKNOWN, std::memory_order_relaxed);
    g_fps.store(0, std::memory_order_relaxed);
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
                g_fps.store(static_cast<uint32_t>(1000.0 / static_cast<double>(avg_dt_ms)), std::memory_order_relaxed);
            }
        }
    }

    g_last_frame_ms = now_ms;
}

bool copy_frame_preview_rgba(const void* data, uint32_t width, uint32_t height, uint32_t size, XrTime frame_time,
                             StoredView* out_view) {
    if (!data || !out_view || width == 0 || height == 0) {
        return false;
    }

    const size_t expected_size = static_cast<size_t>(width) * height * 4;
    if (size != expected_size) {
        return false;
    }

    const uint8_t* src = static_cast<const uint8_t*>(data);
    out_view->pixels.resize(expected_size);
    std::memcpy(out_view->pixels.data(), src, expected_size);
    for (size_t offset = 3; offset < expected_size; offset += 4) {
        out_view->pixels[offset] = 255;
    }

    out_view->info.data_size = size;
    out_view->info.width = width;
    out_view->info.height = height;
    out_view->info.frame_time = frame_time;

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

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_status(OxSimStatus* out_status) {
    if (!out_status) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    if (require_initialized() != OX_SIM_SUCCESS) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const XrSessionState session_state = g_session_state.load(std::memory_order_relaxed);
    out_status->session_state = session_state;
    out_status->session_active = is_session_active(session_state) ? XR_TRUE : XR_FALSE;
    out_status->fps = out_status->session_active ? g_fps.load(std::memory_order_relaxed) : 0;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_view_count(uint32_t* out_count) {
    if (!out_count) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    *out_count = view_count_for_profile(g_profile);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_view_info(uint32_t eye_index, OxSimViewInfo* out_view) {
    if (!out_view) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    uint32_t view_count = 0;
    OxSimResult result = ox_sim_get_view_count(&view_count);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (eye_index >= view_count) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_frame_mutex);
    if (eye_index >= g_frame_views.size()) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    const StoredView& view = g_frame_views[eye_index];
    if (view.pixels.empty() || view.info.data_size == 0) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    *out_view = view.info;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_view(uint32_t eye_index, void* out_pixels,
                                                        uint32_t out_pixels_capacity) {
    if (!out_pixels) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    uint32_t view_count = 0;
    OxSimResult result = ox_sim_get_view_count(&view_count);
    if (result != OX_SIM_SUCCESS) {
        return result;
    }
    if (eye_index >= view_count) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_frame_mutex);
    if (eye_index >= g_frame_views.size()) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    const StoredView& view = g_frame_views[eye_index];
    if (view.pixels.empty() || view.info.data_size == 0) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }
    if (out_pixels_capacity < view.info.data_size) {
        return OX_SIM_ERROR_BUFFER_TOO_SMALL;
    }

    std::memcpy(out_pixels, view.pixels.data(), view.info.data_size);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_profile(char* out_id, uint32_t out_id_capacity) {
    if (!out_id || out_id_capacity == 0) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const char* profile_id = ox_sim::GetDeviceProfileId(g_profile->type);
    const size_t profile_id_len = std::strlen(profile_id);
    if (profile_id_len + 1 > out_id_capacity) {
        return OX_SIM_ERROR_BUFFER_TOO_SMALL;
    }

    std::memcpy(out_id, profile_id, profile_id_len + 1);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_profile(const char* profile_id) {
    if (!profile_id) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const ox_sim::DeviceProfile* profile = ox_sim::GetDeviceProfileByName(profile_id);
    if (!profile) {
        return OX_SIM_ERROR_PROFILE_NOT_FOUND;
    }

    init_devices(profile);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_profile_info(OxSimProfileInfo* out_info) {
    if (!out_info) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    copy_string(out_info->name, g_profile->name);
    copy_string(out_info->manufacturer, g_profile->manufacturer);
    copy_string(out_info->interaction_profile, g_profile->interaction_profile);
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

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_info(uint32_t index, OxSimDeviceInfo* out_info) {
    if (!out_info) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }
    if (index >= g_device_count) {
        return OX_SIM_ERROR_DEVICE_NOT_FOUND;
    }

    const ox_sim::DeviceDef& device_def = g_profile->devices[index];
    copy_string(out_info->user_path, device_def.user_path);
    copy_string(out_info->role, device_def.role);
    out_info->always_active = device_def.always_active ? XR_TRUE : XR_FALSE;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_device(const char* user_path, OxDeviceState* out_state) {
    if (!user_path || !out_state) {
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

    *out_state = g_devices[device_index];
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_device(const char* user_path, const OxDeviceState* state) {
    if (!user_path || !state) {
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

    g_devices[device_index].pose = state->pose;
    g_devices[device_index].is_active = device_def->always_active ? XR_TRUE : (state->is_active ? XR_TRUE : XR_FALSE);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_component_count(const char* user_path, uint32_t* out_count) {
    if (!user_path || !out_count) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const ox_sim::DeviceDef* device_def = find_device_def(user_path);
    if (!device_def) {
        return OX_SIM_ERROR_DEVICE_NOT_FOUND;
    }

    *out_count = static_cast<uint32_t>(device_def->components.size());
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_component_info(const char* user_path, uint32_t index,
                                                                  OxSimComponentInfo* out_info) {
    if (!user_path || !out_info) {
        return OX_SIM_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!is_initialized_locked()) {
        return OX_SIM_ERROR_NOT_INITIALIZED;
    }

    const ox_sim::DeviceDef* device_def = find_device_def(user_path);
    if (!device_def) {
        return OX_SIM_ERROR_DEVICE_NOT_FOUND;
    }
    if (index >= device_def->components.size()) {
        return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
    }

    const ox_sim::ComponentDef& component = device_def->components[index];
    copy_string(out_info->path, component.path);
    copy_string(out_info->description, component.description);
    out_info->type = ox_sim::ToPublicComponentType(component.type);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_boolean(const char* user_path, const char* component_path,
                                                                 XrBool32* out_value) {
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

    if (component_def->type != ox_sim::ComponentType::BOOLEAN) {
        return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
    }

    *out_value = std::get<bool>(g_inputs[device_index].values[component_index]) ? XR_TRUE : XR_FALSE;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_boolean(const char* user_path, const char* component_path,
                                                                 XrBool32 value) {
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

    if (component_def->type != ox_sim::ComponentType::BOOLEAN) {
        return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
    }

    g_inputs[device_index].values[component_index] = value != XR_FALSE;
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_float(const char* user_path, const char* component_path,
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

    if (component_def->type != ox_sim::ComponentType::FLOAT) {
        return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
    }

    *out_value = std::get<float>(g_inputs[device_index].values[component_index]);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_float(const char* user_path, const char* component_path,
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

    if (component_def->type != ox_sim::ComponentType::FLOAT) {
        return OX_SIM_ERROR_COMPONENT_NOT_FOUND;
    }

    g_inputs[device_index].values[component_index] = value;
    sync_float_to_vec2(user_path, component_path);
    return OX_SIM_SUCCESS;
}

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_vector2f(const char* user_path, const char* component_path,
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

extern "C" OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_vector2f(const char* user_path, const char* component_path,
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

extern "C" void sim_submit_frame(XrTime frame_time, uint32_t eye, uint32_t w, uint32_t h, const void* data,
                                 uint32_t size) {
    if (w == 0 || h == 0 || !data || size == 0) {
        return;
    }

    const size_t expected_size = static_cast<size_t>(w) * h * 4;
    if (size != expected_size) {
        return;
    }

    StoredView view = {};
    if (!copy_frame_preview_rgba(data, w, h, size, frame_time, &view)) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_frame_mutex);
    if (eye >= g_frame_views.size()) {
        return;
    }
    g_frame_views[eye] = std::move(view);
    if (eye == 0) {
        update_fps();
    }
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