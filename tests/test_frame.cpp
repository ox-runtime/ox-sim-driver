#include "common.hpp"

namespace ox_sim::tests {

TEST_F(FrameApiTest, StartsWithEmptyPreviewAndUnknownSession) {
    OxSessionState session_state = OX_SESSION_STATE_UNKNOWN;
    uint32_t app_fps = 123;
    OxSimFramePreview preview = {};

    ASSERT_EQ(ox_sim_get_session_state(&session_state), OX_SIM_SUCCESS);
    EXPECT_EQ(session_state, OX_SESSION_STATE_UNKNOWN);

    ASSERT_EQ(ox_sim_get_app_fps(&app_fps), OX_SIM_SUCCESS);
    EXPECT_EQ(app_fps, 0u);

    ASSERT_EQ(ox_sim_get_frame_preview(&preview), OX_SIM_SUCCESS);
    EXPECT_EQ(preview.width, 0u);
    EXPECT_EQ(preview.height, 0u);
    EXPECT_EQ(preview.frame_timestamp_ns, 0u);
}

TEST_F(FrameApiTest, ExposesSubmittedFramePreview) {
    unsigned char left_pixels[16] = {};
    unsigned char right_pixels[16] = {};
    SubmitFramePair(left_pixels, sizeof(left_pixels), right_pixels, sizeof(right_pixels));
    sim_notify_session(OX_SESSION_STATE_VISIBLE);

    OxSimFramePreview preview = {};
    ASSERT_EQ(ox_sim_get_frame_preview(&preview), OX_SIM_SUCCESS);
    EXPECT_EQ(preview.width, 2u);
    EXPECT_EQ(preview.height, 2u);
    EXPECT_EQ(preview.data_size[0], sizeof(left_pixels));
    EXPECT_EQ(preview.data_size[1], sizeof(right_pixels));
    EXPECT_EQ(preview.pixel_data[0], left_pixels);
    EXPECT_EQ(preview.pixel_data[1], right_pixels);
    EXPECT_EQ(preview.session_state, OX_SESSION_STATE_VISIBLE);
    EXPECT_GT(preview.frame_timestamp_ns, 0u);
}

TEST_F(FrameApiTest, ReturnsBufferedFrameWithStableTimestampUntilNextSubmit) {
    unsigned char left_pixels[16] = {};
    unsigned char right_pixels[16] = {};
    SubmitFramePair(left_pixels, sizeof(left_pixels), right_pixels, sizeof(right_pixels));

    OxSimFramePreview first_preview = {};
    ASSERT_EQ(ox_sim_get_frame_preview(&first_preview), OX_SIM_SUCCESS);

    OxSimFramePreview second_preview = {};
    ASSERT_EQ(ox_sim_get_frame_preview(&second_preview), OX_SIM_SUCCESS);
    EXPECT_EQ(second_preview.frame_timestamp_ns, first_preview.frame_timestamp_ns);
    EXPECT_EQ(second_preview.pixel_data[0], left_pixels);
    EXPECT_EQ(second_preview.pixel_data[1], right_pixels);
}

}  // namespace ox_sim::tests