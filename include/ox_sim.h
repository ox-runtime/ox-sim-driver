#pragma once

#include <ox_driver.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OX_SIM_API_VERSION 1

typedef enum {
    OX_SIM_SUCCESS = 0,
    OX_SIM_ERROR_INVALID_ARGUMENT = -1,
    OX_SIM_ERROR_INITIALIZATION_FAILED = -2,
    OX_SIM_ERROR_NOT_INITIALIZED = -3,
    OX_SIM_ERROR_PROFILE_NOT_FOUND = -4,
    OX_SIM_ERROR_DEVICE_NOT_FOUND = -5,
    OX_SIM_ERROR_COMPONENT_NOT_FOUND = -6,
    OX_SIM_ERROR_BUFFER_TOO_SMALL = -7,
} OxSimResult;

typedef struct {
    const void* pixel_data[2];
    uint32_t data_size[2];
    uint32_t width;
    uint32_t height;
    uint32_t has_new_frame;
    uint32_t app_fps;
    OxSessionState session_state;
} OxSimFramePreview;

OX_DRIVER_EXPORT OxSimResult ox_sim_initialize(void);
OX_DRIVER_EXPORT void ox_sim_shutdown(void);

OX_DRIVER_EXPORT OxSimResult ox_sim_get_current_profile(char* out_name, uint32_t out_name_capacity);
OX_DRIVER_EXPORT OxSimResult ox_sim_set_current_profile(const char* profile_name);

OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_count(uint32_t* out_count);
OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_state(uint32_t device_index, OxDeviceState* out_state);
OX_DRIVER_EXPORT OxSimResult ox_sim_get_device_pose(const char* user_path, OxPose* out_pose, uint32_t* out_is_active);
OX_DRIVER_EXPORT OxSimResult ox_sim_set_device_pose(const char* user_path, const OxPose* pose, uint32_t is_active);

OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_state_boolean(const char* user_path, const char* component_path,
                                                            uint32_t* out_value);
OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_state_boolean(const char* user_path, const char* component_path,
                                                            uint32_t value);
OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_state_float(const char* user_path, const char* component_path,
                                                          float* out_value);
OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_state_float(const char* user_path, const char* component_path,
                                                          float value);
OX_DRIVER_EXPORT OxSimResult ox_sim_get_input_state_vector2f(const char* user_path, const char* component_path,
                                                             OxVector2f* out_value);
OX_DRIVER_EXPORT OxSimResult ox_sim_set_input_state_vector2f(const char* user_path, const char* component_path,
                                                             const OxVector2f* value);

OX_DRIVER_EXPORT OxSimResult ox_sim_get_session_state(OxSessionState* out_state);
OX_DRIVER_EXPORT OxSimResult ox_sim_get_app_fps(uint32_t* out_fps);
OX_DRIVER_EXPORT OxSimResult ox_sim_get_frame_preview(OxSimFramePreview* out_preview);

#ifdef __cplusplus
}
#endif