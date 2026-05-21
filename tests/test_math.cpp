#include <gtest/gtest.h>

#include "math.hpp"

namespace ox_sim::tests {

namespace {

void ExpectQuaternionNearIdentity(const XrQuaternionf& orientation) {
    constexpr float kTolerance = 0.0001f;
    EXPECT_NEAR(orientation.x, 0.0f, kTolerance);
    EXPECT_NEAR(orientation.y, 0.0f, kTolerance);
    EXPECT_NEAR(orientation.z, 0.0f, kTolerance);
    EXPECT_NEAR(orientation.w, 1.0f, kTolerance);
}

}  // namespace

TEST(RotationMathTest, AbsoluteEulerEditingReturnsToIdentityAfterResettingAxes) {
    XrQuaternionf orientation = math::QuaternionFromEulerDegrees({90.0f, 0.0f, 0.0f});
    orientation = math::QuaternionFromEulerDegrees({90.0f, 45.0f, 0.0f});
    orientation = math::QuaternionFromEulerDegrees({0.0f, 45.0f, 0.0f});
    orientation = math::QuaternionFromEulerDegrees({0.0f, 0.0f, 0.0f});

    ExpectQuaternionNearIdentity(orientation);
}

TEST(RotationMathTest, EulerRoundTripPreservesDisplayedAngles) {
    constexpr float kTolerance = 0.001f;
    const XrVector3f euler_degrees{20.0f, -35.0f, 15.0f};

    const XrQuaternionf orientation = math::QuaternionFromEulerDegrees(euler_degrees);
    const XrVector3f round_trip = math::EulerDegrees(orientation);

    EXPECT_NEAR(round_trip.x, euler_degrees.x, kTolerance);
    EXPECT_NEAR(round_trip.y, euler_degrees.y, kTolerance);
    EXPECT_NEAR(round_trip.z, euler_degrees.z, kTolerance);
}

}  // namespace ox_sim::tests