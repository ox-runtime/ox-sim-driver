#pragma once

#include <ox_driver.h>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ox_sim {

enum class DeviceType {
    OCULUS_QUEST_2,
    OCULUS_QUEST_3,
    HTC_VIVE,
    VALVE_INDEX,
    HTC_VIVE_TRACKER,
};

enum class ComponentType {
    FLOAT,
    BOOLEAN,
    VEC2,
};

enum class Vec2Axis {
    NONE,
    X,
    Y,
};

struct ComponentDef {
    const char* path;
    ComponentType type;
    const char* description;
    const char* hand_restriction = nullptr;
    const char* linked_vec2_path = nullptr;
    Vec2Axis linked_axis = Vec2Axis::NONE;
};

struct DeviceDef {
    const char* user_path;
    const char* role;
    bool always_active;
    XrPosef default_pose;
    std::vector<ComponentDef> components;
};

struct DeviceProfile {
    DeviceType type;
    const char* name;
    const char* manufacturer;
    const char* serial_prefix;
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t display_width;
    uint32_t display_height;
    uint32_t recommended_width;
    uint32_t recommended_height;
    float refresh_rate;
    float fov_left;
    float fov_right;
    float fov_up;
    float fov_down;
    bool has_position_tracking;
    bool has_orientation_tracking;
    const char* interaction_profile;
    std::vector<DeviceDef> devices;
};

namespace detail {

inline const std::vector<ComponentDef>& OculusTouchComponents() {
    static const std::vector<ComponentDef> components = {
        {"/input/trigger/value", ComponentType::FLOAT, "Trigger"},
        {"/input/trigger/touch", ComponentType::BOOLEAN, "Trigger touch"},
        {"/input/squeeze/value", ComponentType::FLOAT, "Grip/squeeze"},
        {"/input/thumbstick", ComponentType::VEC2, "Thumbstick 2D position"},
        {"/input/thumbstick/x", ComponentType::FLOAT, "Thumbstick X axis", nullptr, "/input/thumbstick", Vec2Axis::X},
        {"/input/thumbstick/y", ComponentType::FLOAT, "Thumbstick Y axis", nullptr, "/input/thumbstick", Vec2Axis::Y},
        {"/input/thumbstick/click", ComponentType::BOOLEAN, "Thumbstick click"},
        {"/input/thumbstick/touch", ComponentType::BOOLEAN, "Thumbstick touch"},
        {"/input/x/click", ComponentType::BOOLEAN, "X button click", "/user/hand/left"},
        {"/input/x/touch", ComponentType::BOOLEAN, "X button touch", "/user/hand/left"},
        {"/input/y/click", ComponentType::BOOLEAN, "Y button click", "/user/hand/left"},
        {"/input/y/touch", ComponentType::BOOLEAN, "Y button touch", "/user/hand/left"},
        {"/input/a/click", ComponentType::BOOLEAN, "A button click", "/user/hand/right"},
        {"/input/a/touch", ComponentType::BOOLEAN, "A button touch", "/user/hand/right"},
        {"/input/b/click", ComponentType::BOOLEAN, "B button click", "/user/hand/right"},
        {"/input/b/touch", ComponentType::BOOLEAN, "B button touch", "/user/hand/right"},
        {"/input/menu/click", ComponentType::BOOLEAN, "Menu button click"},
    };
    return components;
}

inline const std::vector<ComponentDef>& ViveControllerComponents() {
    static const std::vector<ComponentDef> components = {
        {"/input/trigger/value", ComponentType::FLOAT, "Trigger"},
        {"/input/trigger/click", ComponentType::BOOLEAN, "Trigger click"},
        {"/input/squeeze/click", ComponentType::BOOLEAN, "Grip button click"},
        {"/input/trackpad", ComponentType::VEC2, "Trackpad 2D position"},
        {"/input/trackpad/x", ComponentType::FLOAT, "Trackpad X axis", nullptr, "/input/trackpad", Vec2Axis::X},
        {"/input/trackpad/y", ComponentType::FLOAT, "Trackpad Y axis", nullptr, "/input/trackpad", Vec2Axis::Y},
        {"/input/trackpad/click", ComponentType::BOOLEAN, "Trackpad click"},
        {"/input/trackpad/touch", ComponentType::BOOLEAN, "Trackpad touch"},
        {"/input/menu/click", ComponentType::BOOLEAN, "Menu button click"},
    };
    return components;
}

inline const std::vector<ComponentDef>& IndexControllerComponents() {
    static const std::vector<ComponentDef> components = {
        {"/input/trigger/value", ComponentType::FLOAT, "Trigger"},
        {"/input/trigger/click", ComponentType::BOOLEAN, "Trigger click"},
        {"/input/trigger/touch", ComponentType::BOOLEAN, "Trigger touch"},
        {"/input/squeeze/value", ComponentType::FLOAT, "Grip force"},
        {"/input/squeeze/force", ComponentType::FLOAT, "Grip force (alias)"},
        {"/input/thumbstick", ComponentType::VEC2, "Thumbstick 2D position"},
        {"/input/thumbstick/x", ComponentType::FLOAT, "Thumbstick X axis", nullptr, "/input/thumbstick", Vec2Axis::X},
        {"/input/thumbstick/y", ComponentType::FLOAT, "Thumbstick Y axis", nullptr, "/input/thumbstick", Vec2Axis::Y},
        {"/input/thumbstick/click", ComponentType::BOOLEAN, "Thumbstick click"},
        {"/input/thumbstick/touch", ComponentType::BOOLEAN, "Thumbstick touch"},
        {"/input/trackpad", ComponentType::VEC2, "Trackpad 2D position"},
        {"/input/trackpad/x", ComponentType::FLOAT, "Trackpad X axis", nullptr, "/input/trackpad", Vec2Axis::X},
        {"/input/trackpad/y", ComponentType::FLOAT, "Trackpad Y axis", nullptr, "/input/trackpad", Vec2Axis::Y},
        {"/input/trackpad/force", ComponentType::FLOAT, "Trackpad force"},
        {"/input/trackpad/touch", ComponentType::BOOLEAN, "Trackpad touch"},
        {"/input/a/click", ComponentType::BOOLEAN, "A button click"},
        {"/input/a/touch", ComponentType::BOOLEAN, "A button touch"},
        {"/input/b/click", ComponentType::BOOLEAN, "B button click"},
        {"/input/b/touch", ComponentType::BOOLEAN, "B button touch"},
        {"/input/system/click", ComponentType::BOOLEAN, "System button click"},
        {"/input/system/touch", ComponentType::BOOLEAN, "System button touch"},
    };
    return components;
}

inline const std::vector<ComponentDef>& ViveTrackerComponents() {
    static const std::vector<ComponentDef> components = {};
    return components;
}

inline const DeviceProfile& Quest2Profile() {
    static const DeviceProfile profile = {
        DeviceType::OCULUS_QUEST_2,
        "Meta Quest 2 (Simulated)",
        "Meta Platforms",
        "QUEST2-SIM",
        0x2833,
        0x0186,
        1832,
        1920,
        1832,
        1920,
        90.0f,
        -0.785398f,
        0.785398f,
        0.872665f,
        -0.872665f,
        true,
        true,
        "/interaction_profiles/oculus/touch_controller",
        {
            {"/user/head", "hmd", true, {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.6f, 0.0f}}, {}},
            {"/user/hand/left",
             "left_controller",
             false,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {-0.2f, 1.4f, -0.3f}},
             OculusTouchComponents()},
            {"/user/hand/right",
             "right_controller",
             false,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {0.2f, 1.4f, -0.3f}},
             OculusTouchComponents()},
        },
    };
    return profile;
}

inline const DeviceProfile& Quest3Profile() {
    static const DeviceProfile profile = {
        DeviceType::OCULUS_QUEST_3,
        "Meta Quest 3 (Simulated)",
        "Meta Platforms",
        "QUEST3-SIM",
        0x2833,
        0x0200,
        2064,
        2208,
        2064,
        2208,
        120.0f,
        -0.872665f,
        0.872665f,
        0.959931f,
        -0.959931f,
        true,
        true,
        "/interaction_profiles/oculus/touch_controller",
        {
            {"/user/head", "hmd", true, {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.6f, 0.0f}}, {}},
            {"/user/hand/left",
             "left_controller",
             false,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {-0.2f, 1.4f, -0.3f}},
             OculusTouchComponents()},
            {"/user/hand/right",
             "right_controller",
             false,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {0.2f, 1.4f, -0.3f}},
             OculusTouchComponents()},
        },
    };
    return profile;
}

inline const DeviceProfile& ViveProfile() {
    static const DeviceProfile profile = {
        DeviceType::HTC_VIVE,
        "HTC Vive (Simulated)",
        "HTC Corporation",
        "VIVE-SIM",
        0x0BB4,
        0x2C87,
        1080,
        1200,
        1080,
        1200,
        90.0f,
        -0.785398f,
        0.785398f,
        0.872665f,
        -0.872665f,
        true,
        true,
        "/interaction_profiles/htc/vive_controller",
        {
            {"/user/head", "hmd", true, {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.6f, 0.0f}}, {}},
            {"/user/hand/left",
             "left_controller",
             false,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {-0.2f, 1.4f, -0.3f}},
             ViveControllerComponents()},
            {"/user/hand/right",
             "right_controller",
             false,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {0.2f, 1.4f, -0.3f}},
             ViveControllerComponents()},
        },
    };
    return profile;
}

inline const DeviceProfile& IndexProfile() {
    static const DeviceProfile profile = {
        DeviceType::VALVE_INDEX,
        "Valve Index HMD (Simulated)",
        "Valve Corporation",
        "INDEX-SIM",
        0x28DE,
        0x2012,
        1440,
        1600,
        1440,
        1600,
        144.0f,
        -0.959931f,
        0.959931f,
        0.959931f,
        -0.959931f,
        true,
        true,
        "/interaction_profiles/valve/index_controller",
        {
            {"/user/head", "hmd", true, {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.6f, 0.0f}}, {}},
            {"/user/hand/left",
             "left_controller",
             false,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {-0.2f, 1.4f, -0.3f}},
             IndexControllerComponents()},
            {"/user/hand/right",
             "right_controller",
             false,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {0.2f, 1.4f, -0.3f}},
             IndexControllerComponents()},
        },
    };
    return profile;
}

inline const DeviceProfile& ViveTrackerProfile() {
    static const DeviceProfile profile = {
        DeviceType::HTC_VIVE_TRACKER,
        "HTC Vive Tracker (Simulated)",
        "HTC Corporation",
        "VIVETRK-SIM",
        0x0BB4,
        0x0000,
        0,
        0,
        0,
        0,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        false,
        false,
        "/interaction_profiles/htc/vive_tracker_htcx",
        {
            {"/user/vive_tracker_htcx/role/waist",
             "waist_tracker",
             true,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
             ViveTrackerComponents()},
            {"/user/vive_tracker_htcx/role/left_foot",
             "left_foot_tracker",
             true,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {-0.15f, 0.1f, 0.0f}},
             ViveTrackerComponents()},
            {"/user/vive_tracker_htcx/role/right_foot",
             "right_foot_tracker",
             true,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {0.15f, 0.1f, 0.0f}},
             ViveTrackerComponents()},
            {"/user/vive_tracker_htcx/role/left_shoulder",
             "left_shoulder_tracker",
             true,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {-0.2f, 1.5f, 0.0f}},
             ViveTrackerComponents()},
            {"/user/vive_tracker_htcx/role/right_shoulder",
             "right_shoulder_tracker",
             true,
             {{0.0f, 0.0f, 0.0f, 1.0f}, {0.2f, 1.5f, 0.0f}},
             ViveTrackerComponents()},
        },
    };
    return profile;
}

inline const std::map<std::string, DeviceType>& NameToType() {
    static const std::map<std::string, DeviceType> name_to_type = {
        {"oculus_quest_2", DeviceType::OCULUS_QUEST_2},
        {"oculus_quest_3", DeviceType::OCULUS_QUEST_3},
        {"htc_vive", DeviceType::HTC_VIVE},
        {"valve_index", DeviceType::VALVE_INDEX},
        {"htc_vive_tracker", DeviceType::HTC_VIVE_TRACKER},
    };
    return name_to_type;
}

}  // namespace detail

inline const DeviceProfile& GetDeviceProfile(DeviceType type) {
    switch (type) {
        case DeviceType::OCULUS_QUEST_2:
            return detail::Quest2Profile();
        case DeviceType::OCULUS_QUEST_3:
            return detail::Quest3Profile();
        case DeviceType::HTC_VIVE:
            return detail::ViveProfile();
        case DeviceType::VALVE_INDEX:
            return detail::IndexProfile();
        case DeviceType::HTC_VIVE_TRACKER:
            return detail::ViveTrackerProfile();
        default:
            throw std::runtime_error("Unknown device type");
    }
}

inline const char* GetDeviceProfileId(DeviceType type) {
    switch (type) {
        case DeviceType::OCULUS_QUEST_2:
            return "oculus_quest_2";
        case DeviceType::OCULUS_QUEST_3:
            return "oculus_quest_3";
        case DeviceType::HTC_VIVE:
            return "htc_vive";
        case DeviceType::VALVE_INDEX:
            return "valve_index";
        case DeviceType::HTC_VIVE_TRACKER:
            return "htc_vive_tracker";
        default:
            throw std::runtime_error("Unknown device type");
    }
}

inline const DeviceProfile* GetDeviceProfileByName(const std::string& name) {
    const auto& name_to_type = detail::NameToType();
    auto it = name_to_type.find(name);
    if (it == name_to_type.end()) {
        return nullptr;
    }

    return &GetDeviceProfile(it->second);
}

inline DeviceType GetDeviceTypeByName(const std::string& name) {
    const auto& name_to_type = detail::NameToType();
    auto it = name_to_type.find(name);
    if (it == name_to_type.end()) {
        throw std::runtime_error("Unknown device name: " + name);
    }

    return it->second;
}

}  // namespace ox_sim