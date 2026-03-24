#include "common.hpp"

namespace ox_sim::tests {

TEST_F(DeviceApiTest, ReportsProfileDeviceCount) {
    const ox_sim::DeviceProfile* profile = SetProfileAndGet("valve_index");
    uint32_t count = 0;

    ASSERT_EQ(ox_sim_get_device_count(&count), OX_SIM_SUCCESS);
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(count, profile->devices.size());
}

TEST_F(DeviceApiTest, ReturnsIndexedDeviceState) {
    SetProfileAndGet("valve_index");

    OxDeviceState state = {};
    ASSERT_EQ(ox_sim_get_device_state(0, &state), OX_SIM_SUCCESS);
    EXPECT_STREQ(state.user_path, "/user/head");

    uint32_t count = 0;
    ASSERT_EQ(ox_sim_get_device_count(&count), OX_SIM_SUCCESS);
    EXPECT_EQ(ox_sim_get_device_state(count, &state), OX_SIM_ERROR_DEVICE_NOT_FOUND);
}

TEST_F(DeviceApiTest, RoundTripsDevicePose) {
    SetProfileAndGet("valve_index");

    XrPosef pose = {{0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 2.0f, 3.0f}};
    ASSERT_EQ(ox_sim_set_device_pose("/user/hand/left", &pose, 1), OX_SIM_SUCCESS);

    XrPosef roundtrip = {};
    uint32_t is_active = 0;
    ASSERT_EQ(ox_sim_get_device_pose("/user/hand/left", &roundtrip, &is_active), OX_SIM_SUCCESS);
    EXPECT_FLOAT_EQ(roundtrip.position.x, 1.0f);
    EXPECT_FLOAT_EQ(roundtrip.position.y, 2.0f);
    EXPECT_FLOAT_EQ(roundtrip.position.z, 3.0f);
    EXPECT_EQ(is_active, 1u);
}

TEST_F(DeviceApiTest, RejectsUnknownDevicePosePath) {
    XrPosef pose = {};
    uint32_t is_active = 0;
    EXPECT_EQ(ox_sim_get_device_pose("/user/invalid", &pose, &is_active), OX_SIM_ERROR_DEVICE_NOT_FOUND);
}

}  // namespace ox_sim::tests