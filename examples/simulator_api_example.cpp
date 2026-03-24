#include <ox_sim.h>

#include <iostream>

int main() {
    if (ox_sim_initialize() != OX_SIM_SUCCESS) {
        std::cerr << "Failed to initialize simulator" << std::endl;
        return 1;
    }

    char profile_name[64] = {};
    if (ox_sim_get_current_profile(profile_name, sizeof(profile_name)) == OX_SIM_SUCCESS) {
        std::cout << "Current profile: " << profile_name << std::endl;
    }

    uint32_t device_count = 0;
    if (ox_sim_get_device_count(&device_count) == OX_SIM_SUCCESS) {
        std::cout << "Tracked devices: " << device_count << std::endl;
    }

    XrPosef left_pose = {{0.0f, 0.0f, 0.0f, 1.0f}, {-0.25f, 1.35f, -0.4f}};
    ox_sim_set_device_pose("/user/hand/left", &left_pose, 1);
    ox_sim_set_input_state_float("/user/hand/left", "/input/trigger/value", 0.75f);

    float trigger_value = 0.0f;
    if (ox_sim_get_input_state_float("/user/hand/left", "/input/trigger/value", &trigger_value) == OX_SIM_SUCCESS) {
        std::cout << "Left trigger: " << trigger_value << std::endl;
    }

    XrSessionState session_state = XR_SESSION_STATE_UNKNOWN;
    if (ox_sim_get_session_state(&session_state) == OX_SIM_SUCCESS) {
        std::cout << "Session state: " << static_cast<uint32_t>(session_state) << std::endl;
    }

    ox_sim_shutdown();
    return 0;
}