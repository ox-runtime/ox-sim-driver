#include "common.hpp"

namespace ox_sim::tests {

TEST_F(InputApiTest, RoundTripsBooleanInput) {
    SetQuest2Profile();
    ASSERT_EQ(ox_sim_set_input_state_boolean("/user/hand/left", "/input/x/click", 1), OX_SIM_SUCCESS);
    ExpectBoolean("/user/hand/left", "/input/x/click", 1u);
}

TEST_F(InputApiTest, RoundTripsFloatInput) {
    SetQuest2Profile();
    ASSERT_EQ(ox_sim_set_input_state_float("/user/hand/left", "/input/trigger/value", 0.75f), OX_SIM_SUCCESS);
    ExpectFloat("/user/hand/left", "/input/trigger/value", 0.75f);
}

TEST_F(InputApiTest, RoundTripsVectorInput) {
    SetQuest2Profile();

    OxVector2f vec_value = {0.25f, -0.5f};
    ASSERT_EQ(ox_sim_set_input_state_vector2f("/user/hand/left", "/input/thumbstick", &vec_value), OX_SIM_SUCCESS);
    ExpectVec2("/user/hand/left", "/input/thumbstick", 0.25f, -0.5f);
}

TEST_F(InputApiTest, SyncsLinkedAxesFromVector) {
    SetQuest2Profile();

    OxVector2f vec_value = {0.25f, -0.5f};
    ASSERT_EQ(ox_sim_set_input_state_vector2f("/user/hand/left", "/input/thumbstick", &vec_value), OX_SIM_SUCCESS);
    ExpectFloat("/user/hand/left", "/input/thumbstick/x", 0.25f);
    ExpectFloat("/user/hand/left", "/input/thumbstick/y", -0.5f);
}

TEST_F(InputApiTest, SyncsVectorFromLinkedAxis) {
    SetQuest2Profile();

    OxVector2f vec_value = {0.25f, -0.5f};
    ASSERT_EQ(ox_sim_set_input_state_vector2f("/user/hand/left", "/input/thumbstick", &vec_value), OX_SIM_SUCCESS);
    ASSERT_EQ(ox_sim_set_input_state_float("/user/hand/left", "/input/thumbstick/x", -0.75f), OX_SIM_SUCCESS);
    ExpectVec2("/user/hand/left", "/input/thumbstick", -0.75f, -0.5f);
}

TEST_F(InputApiTest, RejectsUnknownComponentPath) {
    SetQuest2Profile();

    float axis_value = 0.0f;
    EXPECT_EQ(ox_sim_get_input_state_float("/user/hand/left", "/input/does_not_exist", &axis_value),
              OX_SIM_ERROR_COMPONENT_NOT_FOUND);
}

}  // namespace ox_sim::tests