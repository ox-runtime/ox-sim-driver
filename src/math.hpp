#pragma once

#include <openxr/openxr.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ox_sim::math {

inline constexpr float kPitchLimitDeg = 89.0f;
inline const glm::vec3 kLocalRight{1.0f, 0.0f, 0.0f};
inline const glm::vec3 kLocalUp{0.0f, 1.0f, 0.0f};
inline const glm::vec3 kLocalForward{0.0f, 0.0f, -1.0f};
inline const glm::vec3 kWorldUp{0.0f, 1.0f, 0.0f};
inline const glm::vec3 kWorldZ{0.0f, 0.0f, 1.0f};

inline glm::quat ToGlm(const XrQuaternionf& quaternion) {
    return glm::quat(quaternion.w, quaternion.x, quaternion.y, quaternion.z);
}

inline XrQuaternionf ToXr(const glm::quat& quaternion) {
    const glm::quat normalized = glm::normalize(quaternion);
    return {normalized.x, normalized.y, normalized.z, normalized.w};
}

inline glm::vec3 ToGlm(const XrVector3f& vector) { return {vector.x, vector.y, vector.z}; }

inline XrVector3f ToXr(const glm::vec3& vector) { return {vector.x, vector.y, vector.z}; }

inline glm::vec3 Forward(const glm::quat& orientation) { return orientation * kLocalForward; }

inline glm::quat RotateWorld(const glm::quat& orientation, const glm::vec3& axis, float angle_radians) {
    if (angle_radians == 0.0f) {
        return orientation;
    }

    return glm::normalize(glm::angleAxis(angle_radians, glm::normalize(axis)) * orientation);
}

inline XrVector3f EulerDegrees(const XrQuaternionf& quaternion) {
    const glm::vec3 euler = glm::degrees(glm::eulerAngles(ToGlm(quaternion)));
    return {euler.x, euler.y, euler.z};
}

inline float ForwardElevationDegrees(const glm::quat& orientation) {
    const glm::vec3 forward = Forward(orientation);
    return glm::degrees(std::atan2(forward.y, glm::length(glm::vec2(forward.x, forward.z))));
}

inline glm::vec3 HorizontalForwardAxis(const glm::quat& orientation) {
    glm::vec3 horizontal_forward = Forward(orientation);
    horizontal_forward.y = 0.0f;
    const float length = glm::length(horizontal_forward);
    if (length <= 0.0001f) {
        return kLocalForward;
    }

    return horizontal_forward / length;
}

inline glm::vec3 HorizontalRightAxis(const glm::quat& orientation) {
    return glm::normalize(glm::cross(HorizontalForwardAxis(orientation), kWorldUp));
}

inline glm::vec3 RotateVector(const XrQuaternionf& orientation, const glm::vec3& vector) {
    return ToGlm(orientation) * vector;
}

}  // namespace ox_sim::math