#include "common.hpp"

namespace ox_sim::tests {

TEST_F(ProfileApiTest, StartsWithQuest2Profile) { ExpectCurrentProfile("oculus_quest_2"); }

TEST_F(ProfileApiTest, RejectsTooSmallProfileBuffer) {
    char tiny_buffer[4] = {};
    EXPECT_EQ(ox_sim_get_profile(tiny_buffer, sizeof(tiny_buffer)), OX_SIM_ERROR_BUFFER_TOO_SMALL);
}

TEST_F(ProfileApiTest, RejectsUnknownProfile) {
    EXPECT_EQ(ox_sim_set_profile("missing_profile"), OX_SIM_ERROR_PROFILE_NOT_FOUND);
}

TEST_F(ProfileApiTest, SwitchesAcrossAllProfiles) {
    const char* profile_ids[] = {
        "oculus_quest_2", "oculus_quest_3", "htc_vive", "valve_index", "htc_vive_tracker",
    };

    for (const char* profile_id : profile_ids) {
        ASSERT_EQ(ox_sim_set_profile(profile_id), OX_SIM_SUCCESS);
        ExpectCurrentProfile(profile_id);
    }
}

TEST_F(ProfileApiTest, ExposesProfileMetadata) {
    ASSERT_EQ(ox_sim_set_profile("htc_vive"), OX_SIM_SUCCESS);

    OxSimProfileInfo info = {};
    ASSERT_EQ(ox_sim_get_profile_info(&info), OX_SIM_SUCCESS);
    EXPECT_STREQ(info.name, "HTC Vive (Simulated)");
    EXPECT_STREQ(info.manufacturer, "HTC Corporation");
    EXPECT_STREQ(info.interaction_profile, "/interaction_profiles/htc/vive_controller");
}

}  // namespace ox_sim::tests