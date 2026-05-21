#include <gtest/gtest.h>

#include "gui/clickable_slider.hpp"

namespace ox_sim::tests {

TEST(ClickableSliderTest, ZeroDeltaDoesNotChangeAnIdleValue) {
    ClickableSlider slider;
    float value = 0.25f;

    EXPECT_FALSE(slider.UpdateAnimation(1, &value, false, 0.0f));
    EXPECT_NEAR(value, 0.25f, 0.0001f);
}

TEST(ClickableSliderTest, PressHoldAndReleaseFollowConfiguredDurations) {
    ClickableSlider slider;
    float value = 0.2f;

    EXPECT_TRUE(slider.UpdateAnimation(1, &value, true, 0.15f));
    EXPECT_NEAR(value, 0.6f, 0.0001f);

    EXPECT_TRUE(slider.UpdateAnimation(1, &value, true, 0.15f));
    EXPECT_NEAR(value, 1.0f, 0.0001f);

    EXPECT_FALSE(slider.UpdateAnimation(1, &value, true, 0.05f));
    EXPECT_NEAR(value, 1.0f, 0.0001f);

    EXPECT_TRUE(slider.UpdateAnimation(1, &value, false, 0.05f));
    EXPECT_NEAR(value, 0.5f, 0.0001f);

    EXPECT_TRUE(slider.UpdateAnimation(1, &value, false, 0.05f));
    EXPECT_NEAR(value, 0.0f, 0.0001f);

    EXPECT_FALSE(slider.UpdateAnimation(1, &value, false, 0.05f));
    EXPECT_NEAR(value, 0.0f, 0.0001f);
}

TEST(ClickableSliderTest, ManualValueCancelsPendingAnimation) {
    ClickableSlider slider;
    float value = 0.0f;

    ASSERT_TRUE(slider.UpdateAnimation(7, &value, true, 0.1f));
    EXPECT_NEAR(value, 0.33333334f, 0.0001f);

    value = 0.4f;
    slider.StopAnimation(7);
    EXPECT_FALSE(slider.UpdateAnimation(7, &value, false, 0.05f));
    EXPECT_NEAR(value, 0.4f, 0.0001f);
}

}  // namespace ox_sim::tests