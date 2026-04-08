#include "common.hpp"

namespace ox_sim::tests {

TEST_F(DeviceApiTest, ReportsProfileDeviceCount) {
    const ox_sim::DeviceProfile* profile = SetProfileAndGet("valve_index");
    uint32_t count = 0;

    ASSERT_EQ(ox_sim_get_device_count(&count), OX_SIM_SUCCESS);
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(count, profile->devices.size());
}

TEST_F(DeviceApiTest, ReturnsIndexedDeviceInfo) {
    SetProfileAndGet("valve_index");

    OxSimDeviceInfo info = {};
    ASSERT_EQ(ox_sim_get_device_info(0, &info), OX_SIM_SUCCESS);
    EXPECT_STREQ(info.user_path, "/user/head");
    EXPECT_STREQ(info.role, "hmd");
    EXPECT_EQ(info.always_active, XR_TRUE);

    uint32_t count = 0;
    ASSERT_EQ(ox_sim_get_device_count(&count), OX_SIM_SUCCESS);
    EXPECT_EQ(ox_sim_get_device_info(count, &info), OX_SIM_ERROR_DEVICE_NOT_FOUND);
}

TEST_F(DeviceApiTest, ControllersAndTrackersStartActiveByDefault) {
    auto expect_active = [](const char* user_path) {
        OxDeviceState state = {};
        ASSERT_EQ(ox_sim_get_device(user_path, &state), OX_SIM_SUCCESS);
        EXPECT_EQ(state.is_active, XR_TRUE);
    };

    SetProfileAndGet("oculus_quest_2");
    expect_active("/user/hand/left");
    expect_active("/user/hand/right");

    SetProfileAndGet("oculus_quest_3");
    expect_active("/user/hand/left");
    expect_active("/user/hand/right");

    SetProfileAndGet("htc_vive");
    expect_active("/user/hand/left");
    expect_active("/user/hand/right");

    SetProfileAndGet("valve_index");
    expect_active("/user/hand/left");
    expect_active("/user/hand/right");

    SetProfileAndGet("htc_vive_tracker");
    expect_active("/user/vive_tracker_htcx/role/waist");
}

TEST_F(DeviceApiTest, RoundTripsDevicePose) {
    SetProfileAndGet("valve_index");

    OxDeviceState state = {};
    state.pose = {{0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 2.0f, 3.0f}};
    state.is_active = XR_TRUE;
    ASSERT_EQ(ox_sim_set_device("/user/hand/left", &state), OX_SIM_SUCCESS);

    OxDeviceState roundtrip = {};
    ASSERT_EQ(ox_sim_get_device("/user/hand/left", &roundtrip), OX_SIM_SUCCESS);
    EXPECT_STREQ(roundtrip.user_path, "/user/hand/left");
    EXPECT_FLOAT_EQ(roundtrip.pose.position.x, 1.0f);
    EXPECT_FLOAT_EQ(roundtrip.pose.position.y, 2.0f);
    EXPECT_FLOAT_EQ(roundtrip.pose.position.z, 3.0f);
    EXPECT_EQ(roundtrip.is_active, XR_TRUE);
}

TEST_F(DeviceApiTest, RejectsUnknownDevicePosePath) {
    OxDeviceState state = {};
    EXPECT_EQ(ox_sim_get_device("/user/invalid", &state), OX_SIM_ERROR_DEVICE_NOT_FOUND);
}

TEST_F(DeviceApiTest, ExposesComponentMetadata) {
    SetProfileAndGet("oculus_quest_2");

    uint32_t component_count = 0;
    ASSERT_EQ(ox_sim_get_component_count("/user/hand/left", &component_count), OX_SIM_SUCCESS);
    EXPECT_GT(component_count, 0u);

    OxSimComponentInfo component = {};
    ASSERT_EQ(ox_sim_get_component_info("/user/hand/left", 0, &component), OX_SIM_SUCCESS);
    EXPECT_STREQ(component.path, "/input/trigger/value");
    EXPECT_EQ(component.type, OX_SIM_COMPONENT_TYPE_FLOAT);
    EXPECT_STREQ(component.description, "Trigger");
}

}  // namespace ox_sim::tests