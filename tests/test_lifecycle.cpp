#include "common.hpp"

namespace ox_sim::tests {

TEST_F(UninitializedApiTest, ReportsNotInitializedBeforeInitialize) {
    uint32_t count = 0;
    EXPECT_EQ(ox_sim_get_device_count(&count), OX_SIM_ERROR_NOT_INITIALIZED);
}

TEST_F(UninitializedApiTest, RejectsInvalidArguments) {
    EXPECT_EQ(ox_sim_get_current_profile(nullptr, 1), OX_SIM_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ox_sim_get_device_count(nullptr), OX_SIM_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ox_sim_get_device_state(0, nullptr), OX_SIM_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ox_sim_get_device_pose(nullptr, nullptr, nullptr), OX_SIM_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ox_sim_set_device_pose(nullptr, nullptr, 0), OX_SIM_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ox_sim_get_input_state_boolean(nullptr, nullptr, nullptr), OX_SIM_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ox_sim_set_input_state_float(nullptr, nullptr, 0.0f), OX_SIM_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ox_sim_get_input_state_vector2f(nullptr, nullptr, nullptr), OX_SIM_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ox_sim_get_session_state(nullptr), OX_SIM_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ox_sim_get_app_fps(nullptr), OX_SIM_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ox_sim_get_frame_preview(nullptr), OX_SIM_ERROR_INVALID_ARGUMENT);
}

TEST_F(UninitializedApiTest, TracksInitializationReferenceCount) {
    uint32_t count = 0;

    ASSERT_EQ(ox_sim_initialize(), OX_SIM_SUCCESS);
    ASSERT_EQ(ox_sim_initialize(), OX_SIM_SUCCESS);
    EXPECT_EQ(ox_sim_get_device_count(&count), OX_SIM_SUCCESS);

    ox_sim_shutdown();
    EXPECT_EQ(ox_sim_get_device_count(&count), OX_SIM_SUCCESS);

    ox_sim_shutdown();
    EXPECT_EQ(ox_sim_get_device_count(&count), OX_SIM_ERROR_NOT_INITIALIZED);
}

}  // namespace ox_sim::tests