#include <ox_sim.h>

#include <iostream>

int main() {
    if (ox_sim_initialize() != OX_SIM_SUCCESS) {
        std::cerr << "Failed to initialize simulator" << std::endl;
        return 1;
    }

    char profile_name[64] = {};
    if (ox_sim_get_profile(profile_name, sizeof(profile_name)) == OX_SIM_SUCCESS) {
        std::cout << "Current profile: " << profile_name << std::endl;
    }

    uint32_t device_count = 0;
    if (ox_sim_get_device_count(&device_count) == OX_SIM_SUCCESS) {
        std::cout << "Tracked devices: " << device_count << std::endl;
    }

    OxDeviceState left_state = {};
    left_state.pose = {{0.0f, 0.0f, 0.0f, 1.0f}, {-0.25f, 1.35f, -0.4f}};
    left_state.is_active = XR_TRUE;
    ox_sim_set_device("/user/hand/left", &left_state);
    ox_sim_set_input_float("/user/hand/left", "/input/trigger/value", 0.75f);

    float trigger_value = 0.0f;
    if (ox_sim_get_input_float("/user/hand/left", "/input/trigger/value", &trigger_value) == OX_SIM_SUCCESS) {
        std::cout << "Left trigger: " << trigger_value << std::endl;
    }

    OxSimStatus status = {};
    if (ox_sim_get_status(&status) == OX_SIM_SUCCESS) {
        std::cout << "Session state: " << static_cast<uint32_t>(status.session_state) << std::endl;
    }

    ox_sim_shutdown();
    return 0;
}