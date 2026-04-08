#include "common.hpp"

namespace ox_sim::tests {

void ExpectPreviewPixels(const void* actual_pixels, const unsigned char* expected_pixels, size_t size) {
    ASSERT_NE(actual_pixels, nullptr);
    EXPECT_EQ(std::memcmp(actual_pixels, expected_pixels, size), 0);
}

std::vector<uint8_t> ReadViewPixels(uint32_t eye_index, const OxSimViewInfo& view_info) {
    std::vector<uint8_t> pixels(view_info.data_size);
    EXPECT_EQ(ox_sim_get_view(eye_index, pixels.data(), static_cast<uint32_t>(pixels.size())), OX_SIM_SUCCESS);
    return pixels;
}

TEST_F(FrameApiTest, StartsWithEmptyPreviewAndUnknownSession) {
    OxSimStatus status = {};
    OxSimViewInfo view = {};
    uint32_t view_count = 99;

    ASSERT_EQ(ox_sim_get_status(&status), OX_SIM_SUCCESS);
    EXPECT_EQ(status.session_state, XR_SESSION_STATE_UNKNOWN);
    EXPECT_EQ(status.session_active, XR_FALSE);
    EXPECT_EQ(status.fps, 0u);

    ASSERT_EQ(ox_sim_get_view_count(&view_count), OX_SIM_SUCCESS);
    EXPECT_EQ(view_count, 2u);

    EXPECT_EQ(ox_sim_get_view_info(0, &view), OX_SIM_ERROR_NOT_INITIALIZED);
}

TEST_F(FrameApiTest, ExposesSubmittedFramePreview) {
    unsigned char left_pixels[16] = {1, 2, 3, 255, 5, 6, 7, 255, 9, 10, 11, 255, 13, 14, 15, 255};
    unsigned char right_pixels[16] = {21, 22, 23, 255, 25, 26, 27, 255, 29, 30, 31, 255, 33, 34, 35, 255};
    SubmitFramePair(left_pixels, sizeof(left_pixels), right_pixels, sizeof(right_pixels));
    sim_notify_session(XR_SESSION_STATE_VISIBLE);

    OxSimStatus status = {};
    ASSERT_EQ(ox_sim_get_status(&status), OX_SIM_SUCCESS);
    EXPECT_EQ(status.session_state, XR_SESSION_STATE_VISIBLE);

    OxSimViewInfo left_view = {};
    OxSimViewInfo right_view = {};
    ASSERT_EQ(ox_sim_get_view_info(0, &left_view), OX_SIM_SUCCESS);
    ASSERT_EQ(ox_sim_get_view_info(1, &right_view), OX_SIM_SUCCESS);
    EXPECT_EQ(left_view.width, 2u);
    EXPECT_EQ(left_view.height, 2u);
    EXPECT_EQ(left_view.data_size, sizeof(left_pixels));
    EXPECT_EQ(right_view.data_size, sizeof(right_pixels));
    std::vector<uint8_t> left_pixels_copy = ReadViewPixels(0, left_view);
    std::vector<uint8_t> right_pixels_copy = ReadViewPixels(1, right_view);
    ExpectPreviewPixels(left_pixels_copy.data(), left_pixels, sizeof(left_pixels));
    ExpectPreviewPixels(right_pixels_copy.data(), right_pixels, sizeof(right_pixels));
    EXPECT_GT(left_view.frame_time, 0);
}

TEST_F(FrameApiTest, ReturnsBufferedFrameWithStableTimestampUntilNextSubmit) {
    unsigned char left_pixels[16] = {41, 42, 43, 255, 45, 46, 47, 255, 49, 50, 51, 255, 53, 54, 55, 255};
    unsigned char right_pixels[16] = {61, 62, 63, 255, 65, 66, 67, 255, 69, 70, 71, 255, 73, 74, 75, 255};
    SubmitFramePair(left_pixels, sizeof(left_pixels), right_pixels, sizeof(right_pixels));

    OxSimViewInfo first_left = {};
    ASSERT_EQ(ox_sim_get_view_info(0, &first_left), OX_SIM_SUCCESS);

    OxSimViewInfo second_left = {};
    ASSERT_EQ(ox_sim_get_view_info(0, &second_left), OX_SIM_SUCCESS);
    EXPECT_EQ(second_left.frame_time, first_left.frame_time);
    std::vector<uint8_t> first_pixels = ReadViewPixels(0, first_left);
    std::vector<uint8_t> second_pixels = ReadViewPixels(0, second_left);
    ExpectPreviewPixels(first_pixels.data(), left_pixels, sizeof(left_pixels));
    ExpectPreviewPixels(second_pixels.data(), left_pixels, sizeof(left_pixels));
}

TEST_F(FrameApiTest, CallerOwnedViewBufferRemainsReadableAfterNextSubmit) {
    unsigned char first_left_pixels[16] = {81, 82, 83, 0, 85, 86, 87, 0, 89, 90, 91, 0, 93, 94, 95, 0};
    unsigned char first_right_pixels[16] = {101, 102, 103, 0, 105, 106, 107, 0, 109, 110, 111, 0, 113, 114, 115, 0};
    unsigned char second_left_pixels[16] = {121, 122, 123, 0, 125, 126, 127, 0, 129, 130, 131, 0, 133, 134, 135, 0};
    unsigned char second_right_pixels[16] = {141, 142, 143, 0, 145, 146, 147, 0, 149, 150, 151, 0, 153, 154, 155, 0};
    unsigned char expected_first_left_pixels[16] = {81, 82, 83, 255, 85, 86, 87, 255, 89, 90, 91, 255, 93, 94, 95, 255};
    unsigned char expected_second_left_pixels[16] = {121, 122, 123, 255, 125, 126, 127, 255, 129, 130, 131, 255,
                                                     133, 134, 135, 255};

    sim_submit_frame(10, 0, 2, 2, first_left_pixels, sizeof(first_left_pixels));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sim_submit_frame(11, 1, 2, 2, first_right_pixels, sizeof(first_right_pixels));

    OxSimViewInfo first_left = {};
    ASSERT_EQ(ox_sim_get_view_info(0, &first_left), OX_SIM_SUCCESS);
    std::vector<uint8_t> first_pixels = ReadViewPixels(0, first_left);

    sim_submit_frame(20, 0, 2, 2, second_left_pixels, sizeof(second_left_pixels));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sim_submit_frame(21, 1, 2, 2, second_right_pixels, sizeof(second_right_pixels));

    ExpectPreviewPixels(first_pixels.data(), expected_first_left_pixels, sizeof(expected_first_left_pixels));

    OxSimViewInfo second_left = {};
    ASSERT_EQ(ox_sim_get_view_info(0, &second_left), OX_SIM_SUCCESS);
    std::vector<uint8_t> second_pixels = ReadViewPixels(0, second_left);
    ExpectPreviewPixels(second_pixels.data(), expected_second_left_pixels, sizeof(expected_second_left_pixels));
    EXPECT_NE(second_left.frame_time, first_left.frame_time);
}

TEST_F(FrameApiTest, RejectsTooSmallViewBuffer) {
    unsigned char left_pixels[16] = {1, 2, 3, 0, 5, 6, 7, 0, 9, 10, 11, 0, 13, 14, 15, 0};
    unsigned char right_pixels[16] = {21, 22, 23, 0, 25, 26, 27, 0, 29, 30, 31, 0, 33, 34, 35, 0};
    SubmitFramePair(left_pixels, sizeof(left_pixels), right_pixels, sizeof(right_pixels));

    OxSimViewInfo left_view = {};
    ASSERT_EQ(ox_sim_get_view_info(0, &left_view), OX_SIM_SUCCESS);

    std::vector<uint8_t> too_small(left_view.data_size - 1);
    EXPECT_EQ(ox_sim_get_view(0, too_small.data(), static_cast<uint32_t>(too_small.size())),
              OX_SIM_ERROR_BUFFER_TOO_SMALL);
}

TEST_F(FrameApiTest, TrackerProfilesExposeNoViews) {
    ASSERT_EQ(ox_sim_set_profile("htc_vive_tracker"), OX_SIM_SUCCESS);

    uint32_t view_count = 99;
    ASSERT_EQ(ox_sim_get_view_count(&view_count), OX_SIM_SUCCESS);
    EXPECT_EQ(view_count, 0u);
}

}  // namespace ox_sim::tests