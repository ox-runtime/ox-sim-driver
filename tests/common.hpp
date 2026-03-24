#pragma once

#include <gtest/gtest.h>
#include <ox_sim.h>

#include <chrono>
#include <cstring>
#include <thread>

#include "device_profiles.hpp"

extern "C" void sim_submit_frame(XrTime frame_time, uint32_t eye, uint32_t w, uint32_t h, const void* data,
                                 uint32_t size);
extern "C" void sim_notify_session(XrSessionState state);

namespace ox_sim::tests {

inline void EnsureShutdown() {
    ox_sim_shutdown();
    ox_sim_shutdown();
}

class UninitializedApiTest : public ::testing::Test {
   protected:
    void SetUp() override { EnsureShutdown(); }
    void TearDown() override { EnsureShutdown(); }
};

class InitializedApiTest : public ::testing::Test {
   protected:
    void SetUp() override {
        EnsureShutdown();
        ASSERT_EQ(ox_sim_initialize(), OX_SIM_SUCCESS);
    }

    void TearDown() override { EnsureShutdown(); }
};

class ProfileApiTest : public InitializedApiTest {
   protected:
    void ExpectCurrentProfile(const char* expected_id) {
        char profile_name[64] = {};
        ASSERT_EQ(ox_sim_get_current_profile(profile_name, sizeof(profile_name)), OX_SIM_SUCCESS);
        EXPECT_STREQ(profile_name, expected_id);
    }
};

class DeviceApiTest : public InitializedApiTest {
   protected:
    const ox_sim::DeviceProfile* SetProfileAndGet(const char* profile_id) {
        const OxSimResult result = ox_sim_set_current_profile(profile_id);
        if (result != OX_SIM_SUCCESS) {
            ADD_FAILURE() << "ox_sim_set_current_profile failed for " << profile_id << ": " << result;
            return nullptr;
        }

        const ox_sim::DeviceProfile* profile = ox_sim::GetDeviceProfileByName(profile_id);
        EXPECT_NE(profile, nullptr);
        return profile;
    }
};

class InputApiTest : public InitializedApiTest {
   protected:
    void SetQuest2Profile() { ASSERT_EQ(ox_sim_set_current_profile("oculus_quest_2"), OX_SIM_SUCCESS); }

    void ExpectFloat(const char* user_path, const char* component_path, float expected) {
        float value = 0.0f;
        ASSERT_EQ(ox_sim_get_input_state_float(user_path, component_path, &value), OX_SIM_SUCCESS);
        EXPECT_FLOAT_EQ(value, expected);
    }

    void ExpectBoolean(const char* user_path, const char* component_path, uint32_t expected) {
        uint32_t value = 0;
        ASSERT_EQ(ox_sim_get_input_state_boolean(user_path, component_path, &value), OX_SIM_SUCCESS);
        EXPECT_EQ(value, expected);
    }

    void ExpectVec2(const char* user_path, const char* component_path, float expected_x, float expected_y) {
        XrVector2f value = {};
        ASSERT_EQ(ox_sim_get_input_state_vector2f(user_path, component_path, &value), OX_SIM_SUCCESS);
        EXPECT_FLOAT_EQ(value.x, expected_x);
        EXPECT_FLOAT_EQ(value.y, expected_y);
    }
};

class FrameApiTest : public InitializedApiTest {
   protected:
    void SubmitFramePair(const void* left_pixels, uint32_t left_size, const void* right_pixels, uint32_t right_size) {
        sim_submit_frame(1, 0, 2, 2, left_pixels, left_size);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        sim_submit_frame(2, 1, 2, 2, right_pixels, right_size);
    }
};

}  // namespace ox_sim::tests