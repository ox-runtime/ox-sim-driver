#pragma once

// ox_sim.h - OxDriver simulator control API
//
// Provides programmatic access to the simulator's state for automation,
// testing, and GUI tooling. Both the REST server and the simulator GUI
// are built on top of this API, so all state is always in sync.
//
// Mirrors the REST API resource model:
//   Status     -> GET /v1/status
//   Views      -> GET /v1/views/{eye}
//   Profile    -> GET/PUT /v1/profile
//   Devices    -> GET/PUT /v1/devices/{userPath}
//   Components -> GET /v1/profile (components array)
//   Inputs     -> GET/PUT /v1/inputs/{bindingPath}
//
// Threading: all functions are safe to call from any thread.
// All output string buffers must be null-terminated on success.
// All input string parameters are expected to be null-terminated.

#include <ox_driver.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OX_SIM_SUCCESS = 0,
    OX_SIM_ERROR_INVALID_ARGUMENT = 1,
    OX_SIM_ERROR_NOT_INITIALIZED = 2,
    OX_SIM_ERROR_BUFFER_TOO_SMALL = 3,
    OX_SIM_ERROR_PROFILE_NOT_FOUND = 4,
    OX_SIM_ERROR_DEVICE_NOT_FOUND = 5,
    OX_SIM_ERROR_COMPONENT_NOT_FOUND = 6,
} OxSimResult;

typedef enum {
    OX_SIM_COMPONENT_TYPE_BOOLEAN = 0,
    OX_SIM_COMPONENT_TYPE_FLOAT   = 1,
    OX_SIM_COMPONENT_TYPE_VEC2    = 2,
} OxSimComponentType;

// Display metadata for the active profile.
typedef struct {
    char name[128];                // e.g. "Meta Quest 2 (Simulated)"
    char manufacturer[128];        // e.g. "Meta Platforms"
    char interaction_profile[256]; // e.g. "/interaction_profiles/oculus/touch_controller"
} OxSimProfileInfo;

// Static description of a device in the active profile.
// Use ox_sim_get_device() to read live pose and active state.
typedef struct {
    char     user_path[256]; // e.g. "/user/hand/left"
    char     role[64];       // e.g. "left_controller", "hmd"
    XrBool32 always_active;  // XR_TRUE if the device is always considered active (e.g. HMD)
} OxSimDeviceInfo;

// Static description of an input component on a device.
typedef struct {
    char               path[256];        // e.g. "/input/trigger/value"
    OxSimComponentType type;
    char               description[128]; // human-readable label, e.g. "Trigger"
} OxSimComponentInfo;

// Current simulator session status.
typedef struct {
    XrSessionState session_state;  // current XrSessionState value
    XrBool32       session_active; // XR_TRUE when state is synchronized, visible, or focused
    uint32_t       fps;            // application frame rate; 0 when session is inactive
} OxSimStatus;

// Metadata for the most recently submitted eye texture.
typedef struct {
    uint32_t    data_size;
    uint32_t    width;
    uint32_t    height;
    XrTime      frame_time;
} OxSimViewInfo;

// ========== Lifecycle ==========

// Initialize the simulator. Must be called before any other ox_sim_* function.
// Safe to call multiple times; subsequent calls are no-ops.
OX_DRIVER_EXPORT OxSimResult ox_sim_initialize(void);

// Shut down the simulator and release all resources.
OX_DRIVER_EXPORT void ox_sim_shutdown(void);

// ========== Status ==========

// Get current session state and application frame rate.
OX_DRIVER_EXPORT OxSimResult ox_sim_get_status(OxSimStatus* out_status);

// ========== Views ==========

// Get the number of eyes in the active profile (typically 2; 0 for tracker-only profiles).
OX_DRIVER_EXPORT OxSimResult ox_sim_get_view_count(uint32_t* out_count);

// Get the metadata for the most recently submitted frame texture for the given eye index.
// Returns OX_SIM_ERROR_NOT_INITIALIZED if no frame has been submitted yet.
OX_DRIVER_EXPORT OxSimResult ox_sim_get_view_info(uint32_t eye_index, OxSimViewInfo* out_view);

// Copy the most recently submitted frame texture for the given eye index into
// caller-owned RGBA8 storage. out_pixels_capacity must be at least the
// data_size reported by ox_sim_get_view_info().
OX_DRIVER_EXPORT OxSimResult ox_sim_get_view(uint32_t eye_index, void* out_pixels, uint32_t out_pixels_capacity);

// ========== Profile ==========

// Get the active profile ID (e.g. "oculus_quest_2").
// out_id_capacity must include space for the null terminator.
// Returns OX_SIM_ERROR_BUFFER_TOO_SMALL if the buffer is insufficient.
OX_DRIVER_EXPORT OxSimResult ox_sim_get_profile(char* out_id, uint32_t out_id_capacity);

// Switch the active profile by ID. Valid IDs:
//   "oculus_quest_2", "oculus_quest_3", "htc_vive", "valve_index", "htc_vive_tracker"
// Resets all device poses and input states to profile defaults.
OX_DRIVER_EXPORT OxSimResult ox_sim_set_profile(const char* profile_id);

// Get display metadata for the active profile.
OX_DRIVER_EXPORT OxSimResult ox_sim_get_profile_info(OxSimProfileInfo* out_info);

// ========== Devices ==========

// Get the number of devices in the active profile.
OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_count(uint32_t* out_count);

// Get static profile metadata for a device by index.
// Use ox_sim_get_device() to read live pose and active state.
OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_info(uint32_t index, OxSimDeviceInfo* out_info);

// Get live pose and active state for a device by user_path.
OX_DRIVER_EXPORT OxSimResult ox_sim_get_device(const char* user_path, OxDeviceState* out_state);

// Set pose and active state for a device by user_path.
// state->user_path is ignored; the user_path parameter is used.
OX_DRIVER_EXPORT OxSimResult ox_sim_set_device(const char* user_path, const OxDeviceState* state);

// ========== Components ==========

// Get the number of input components on the device at user_path.
OX_DRIVER_EXPORT OxSimResult ox_sim_get_component_count(const char* user_path, uint32_t* out_count);

// Get the static description of an input component by index.
OX_DRIVER_EXPORT OxSimResult ox_sim_get_component_info(const char* user_path, uint32_t index,
                                                       OxSimComponentInfo* out_info);

// ========== Inputs ==========

// Get and set input state by user_path and component_path.
// component_path examples: "/input/trigger/value", "/input/a/click", "/input/thumbstick"
// Returns OX_SIM_ERROR_COMPONENT_NOT_FOUND if the path is not valid for the device.

OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_boolean(const char* user_path, const char* component_path,
                                                      XrBool32* out_value);
OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_boolean(const char* user_path, const char* component_path,
                                                      XrBool32 value);
OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_float(const char* user_path, const char* component_path,
                                                    float* out_value);
OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_float(const char* user_path, const char* component_path, float value);
OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_vector2f(const char* user_path, const char* component_path,
                                                       XrVector2f* out_value);
OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_vector2f(const char* user_path, const char* component_path,
                                                       const XrVector2f* value);

#ifdef __cplusplus
}
#endif