#include "common.hpp"

namespace ox_sim::tests {

void ExpectPreviewPixels(const void* actual_pixels, const unsigned char* expected_pixels, size_t size) {
    ASSERT_NE(actual_pixels, nullptr);
    EXPECT_EQ(std::memcmp(actual_pixels, expected_pixels, size), 0);
}

TEST_F(FrameApiTest, StartsWithEmptyPreviewAndUnknownSession) {
    XrSessionState session_state = XR_SESSION_STATE_UNKNOWN;
    uint32_t app_fps = 123;
    OxSimFramePreview preview = {};

    ASSERT_EQ(ox_sim_get_session_state(&session_state), OX_SIM_SUCCESS);
    EXPECT_EQ(session_state, XR_SESSION_STATE_UNKNOWN);

    ASSERT_EQ(ox_sim_get_app_fps(&app_fps), OX_SIM_SUCCESS);
    EXPECT_EQ(app_fps, 0u);

    ASSERT_EQ(ox_sim_get_frame_preview(&preview), OX_SIM_SUCCESS);
    EXPECT_EQ(preview.width, 0u);
    EXPECT_EQ(preview.height, 0u);
    EXPECT_EQ(preview.frame_time, 0);
}

TEST_F(FrameApiTest, ExposesSubmittedFramePreview) {
    unsigned char left_pixels[16] = {1, 2, 3, 255, 5, 6, 7, 255, 9, 10, 11, 255, 13, 14, 15, 255};
    unsigned char right_pixels[16] = {21, 22, 23, 255, 25, 26, 27, 255, 29, 30, 31, 255, 33, 34, 35, 255};
    SubmitFramePair(left_pixels, sizeof(left_pixels), right_pixels, sizeof(right_pixels));
    sim_notify_session(XR_SESSION_STATE_VISIBLE);

    OxSimFramePreview preview = {};
    ASSERT_EQ(ox_sim_get_frame_preview(&preview), OX_SIM_SUCCESS);
    EXPECT_EQ(preview.width, 2u);
    EXPECT_EQ(preview.height, 2u);
    EXPECT_EQ(preview.data_size[0], sizeof(left_pixels));
    EXPECT_EQ(preview.data_size[1], sizeof(right_pixels));
    ExpectPreviewPixels(preview.pixel_data[0], left_pixels, sizeof(left_pixels));
    ExpectPreviewPixels(preview.pixel_data[1], right_pixels, sizeof(right_pixels));
    EXPECT_EQ(preview.session_state, XR_SESSION_STATE_VISIBLE);
    EXPECT_GT(preview.frame_time, 0);
}

TEST_F(FrameApiTest, ReturnsBufferedFrameWithStableTimestampUntilNextSubmit) {
    unsigned char left_pixels[16] = {41, 42, 43, 255, 45, 46, 47, 255, 49, 50, 51, 255, 53, 54, 55, 255};
    unsigned char right_pixels[16] = {61, 62, 63, 255, 65, 66, 67, 255, 69, 70, 71, 255, 73, 74, 75, 255};
    SubmitFramePair(left_pixels, sizeof(left_pixels), right_pixels, sizeof(right_pixels));

    OxSimFramePreview first_preview = {};
    ASSERT_EQ(ox_sim_get_frame_preview(&first_preview), OX_SIM_SUCCESS);

    OxSimFramePreview second_preview = {};
    ASSERT_EQ(ox_sim_get_frame_preview(&second_preview), OX_SIM_SUCCESS);
    EXPECT_EQ(second_preview.frame_time, first_preview.frame_time);
    ExpectPreviewPixels(first_preview.pixel_data[0], left_pixels, sizeof(left_pixels));
    ExpectPreviewPixels(first_preview.pixel_data[1], right_pixels, sizeof(right_pixels));
    ExpectPreviewPixels(second_preview.pixel_data[0], left_pixels, sizeof(left_pixels));
    ExpectPreviewPixels(second_preview.pixel_data[1], right_pixels, sizeof(right_pixels));
}

}  // namespace ox_sim::tests