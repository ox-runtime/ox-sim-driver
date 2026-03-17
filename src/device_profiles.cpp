#include "device_profiles.h"

#include <cmath>
#include <map>
#include <stdexcept>

namespace ox_sim {

// clang-format off
// ===== Oculus Touch Controller Components =====
static const std::vector<ComponentDef> OCULUS_TOUCH_COMPONENTS = {
    // Trigger
    {"/input/trigger/value", ComponentType::FLOAT, "Trigger"},
    {"/input/trigger/touch", ComponentType::BOOLEAN, "Trigger touch"},
    // Grip
    {"/input/squeeze/value", ComponentType::FLOAT, "Grip/squeeze"},
    // Thumbstick — VEC2 parent + linked FLOAT axes
    {"/input/thumbstick",    ComponentType::VEC2,    "Thumbstick 2D position"},
    {"/input/thumbstick/x",  ComponentType::FLOAT,   "Thumbstick X axis",  nullptr, "/input/thumbstick", Vec2Axis::X},
    {"/input/thumbstick/y",  ComponentType::FLOAT,   "Thumbstick Y axis",  nullptr, "/input/thumbstick", Vec2Axis::Y},
    {"/input/thumbstick/click", ComponentType::BOOLEAN, "Thumbstick click"},
    {"/input/thumbstick/touch", ComponentType::BOOLEAN, "Thumbstick touch"},
    // Buttons - X/Y on left, A/B on right
    {"/input/x/click", ComponentType::BOOLEAN, "X button click", "/user/hand/left"},
    {"/input/x/touch", ComponentType::BOOLEAN, "X button touch", "/user/hand/left"},
    {"/input/y/click", ComponentType::BOOLEAN, "Y button click", "/user/hand/left"},
    {"/input/y/touch", ComponentType::BOOLEAN, "Y button touch", "/user/hand/left"},
    {"/input/a/click", ComponentType::BOOLEAN, "A button click", "/user/hand/right"},
    {"/input/a/touch", ComponentType::BOOLEAN, "A button touch", "/user/hand/right"},
    {"/input/b/click", ComponentType::BOOLEAN, "B button click", "/user/hand/right"},
    {"/input/b/touch", ComponentType::BOOLEAN, "B button touch", "/user/hand/right"},
    // Menu button
    {"/input/menu/click", ComponentType::BOOLEAN, "Menu button click"},
};

// ===== Vive Controller Components =====
static const std::vector<ComponentDef> VIVE_CONTROLLER_COMPONENTS = {
    // Trigger
    {"/input/trigger/value", ComponentType::FLOAT, "Trigger"},
    {"/input/trigger/click", ComponentType::BOOLEAN, "Trigger click"},
    // Grip
    {"/input/squeeze/click", ComponentType::BOOLEAN, "Grip button click"},
    // Trackpad — VEC2 parent + linked FLOAT axes
    {"/input/trackpad",   ComponentType::VEC2,  "Trackpad 2D position"},
    {"/input/trackpad/x", ComponentType::FLOAT, "Trackpad X axis", nullptr, "/input/trackpad", Vec2Axis::X},
    {"/input/trackpad/y", ComponentType::FLOAT, "Trackpad Y axis", nullptr, "/input/trackpad", Vec2Axis::Y},
    {"/input/trackpad/click", ComponentType::BOOLEAN, "Trackpad click"},
    {"/input/trackpad/touch", ComponentType::BOOLEAN, "Trackpad touch"},
    // Menu button
    {"/input/menu/click", ComponentType::BOOLEAN, "Menu button click"},
};

// ===== Valve Index Controller Components =====
static const std::vector<ComponentDef> INDEX_CONTROLLER_COMPONENTS = {
    // Trigger
    {"/input/trigger/value", ComponentType::FLOAT, "Trigger"},
    {"/input/trigger/click", ComponentType::BOOLEAN, "Trigger click"},
    {"/input/trigger/touch", ComponentType::BOOLEAN, "Trigger touch"},
    // Grip (force sensor)
    {"/input/squeeze/value", ComponentType::FLOAT, "Grip force"},
    {"/input/squeeze/force", ComponentType::FLOAT, "Grip force (alias)"},
    // Thumbstick — VEC2 parent + linked FLOAT axes
    {"/input/thumbstick",    ComponentType::VEC2,    "Thumbstick 2D position"},
    {"/input/thumbstick/x",  ComponentType::FLOAT,   "Thumbstick X axis",  nullptr, "/input/thumbstick", Vec2Axis::X},
    {"/input/thumbstick/y",  ComponentType::FLOAT,   "Thumbstick Y axis",  nullptr, "/input/thumbstick", Vec2Axis::Y},
    {"/input/thumbstick/click", ComponentType::BOOLEAN, "Thumbstick click"},
    {"/input/thumbstick/touch", ComponentType::BOOLEAN, "Thumbstick touch"},
    // Trackpad — VEC2 parent + linked FLOAT axes
    {"/input/trackpad",      ComponentType::VEC2,    "Trackpad 2D position"},
    {"/input/trackpad/x",    ComponentType::FLOAT,   "Trackpad X axis",    nullptr, "/input/trackpad", Vec2Axis::X},
    {"/input/trackpad/y",    ComponentType::FLOAT,   "Trackpad Y axis",    nullptr, "/input/trackpad", Vec2Axis::Y},
    {"/input/trackpad/force", ComponentType::FLOAT, "Trackpad force"},
    {"/input/trackpad/touch", ComponentType::BOOLEAN, "Trackpad touch"},
    // Buttons
    {"/input/a/click", ComponentType::BOOLEAN, "A button click"},
    {"/input/a/touch", ComponentType::BOOLEAN, "A button touch"},
    {"/input/b/click", ComponentType::BOOLEAN, "B button click"},
    {"/input/b/touch", ComponentType::BOOLEAN, "B button touch"},
    // System button
    {"/input/system/click", ComponentType::BOOLEAN, "System button click"},
    {"/input/system/touch", ComponentType::BOOLEAN, "System button touch"},
};

// ===== Vive Tracker (no input components, pose-only) =====
static const std::vector<ComponentDef> VIVE_TRACKER_COMPONENTS = {
    // Trackers typically have no input components, only pose tracking
};

// ===== Device Profiles =====

// Define device profiles
static const DeviceProfile QUEST_2_PROFILE = {
    DeviceType::OCULUS_QUEST_2,

    // Device info
    "Meta Quest 2 (Simulated)",
    "Meta Platforms",
    "QUEST2-SIM",
    0x2833, // Meta VID
    0x0186, // Quest 2 PID

    // Display: 1832x1920 per eye
    1832,
    1920,
    1832,
    1920,
    90.0f, // 90Hz (also supports 120Hz)

    // FOV (approximate Quest 2 values in radians)
    -0.785398f, // left: ~45 degrees
    0.785398f,  // right: ~45 degrees
    0.872665f,  // up: ~50 degrees
    -0.872665f, // down: ~50 degrees

    // Tracking
    true, // position tracking
    true, // orientation tracking

    // Interaction profile
    "/interaction_profiles/oculus/touch_controller",

    // Devices
    {
        {"/user/head", "hmd", true, {{0.0f, 1.6f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, {}}, // HMD - always active, no input components
        {"/user/hand/left", "left_controller", false, {{-0.2f, 1.4f, -0.3f}, {0.0f, 0.0f, 0.0f, 1.0f}}, OCULUS_TOUCH_COMPONENTS},
        {"/user/hand/right", "right_controller", false, {{0.2f, 1.4f, -0.3f}, {0.0f, 0.0f, 0.0f, 1.0f}}, OCULUS_TOUCH_COMPONENTS},
    },
};

static const DeviceProfile QUEST_3_PROFILE = {
    DeviceType::OCULUS_QUEST_3,

    // Device info
    "Meta Quest 3 (Simulated)",
    "Meta Platforms",
    "QUEST3-SIM",
    0x2833,
    0x0200,

    // Display: 2064x2208 per eye
    2064,
    2208,
    2064,
    2208,
    120.0f, // 120Hz

    // FOV (Quest 3 has slightly wider FOV)
    -0.872665f, // left: ~50 degrees
    0.872665f,  // right: ~50 degrees
    0.959931f,  // up: ~55 degrees
    -0.959931f, // down: ~55 degrees

    true,
    true,

    "/interaction_profiles/oculus/touch_controller",

    // Devices
    {
        {"/user/head", "hmd", true, {{0.0f, 1.6f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, {}},
        {"/user/hand/left", "left_controller", false, {{-0.2f, 1.4f, -0.3f}, {0.0f, 0.0f, 0.0f, 1.0f}}, OCULUS_TOUCH_COMPONENTS},
        {"/user/hand/right", "right_controller", false, {{0.2f, 1.4f, -0.3f}, {0.0f, 0.0f, 0.0f, 1.0f}}, OCULUS_TOUCH_COMPONENTS},
    },
};

static const DeviceProfile VIVE_PROFILE = {
    DeviceType::HTC_VIVE,

    "HTC Vive (Simulated)",
    "HTC Corporation",
    "VIVE-SIM",
    0x0BB4, // HTC VID
    0x2C87,

    // Display: 1080x1200 per eye
    1080,
    1200,
    1080,
    1200,
    90.0f,

    // FOV
    -0.785398f,
    0.785398f,
    0.872665f,
    -0.872665f,

    true,
    true,

    "/interaction_profiles/htc/vive_controller",

    // Devices
    {
        {"/user/head", "hmd", true, {{0.0f, 1.6f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, {}},
        {"/user/hand/left", "left_controller", false, {{-0.2f, 1.4f, -0.3f}, {0.0f, 0.0f, 0.0f, 1.0f}}, VIVE_CONTROLLER_COMPONENTS},
        {"/user/hand/right", "right_controller", false, {{0.2f, 1.4f, -0.3f}, {0.0f, 0.0f, 0.0f, 1.0f}}, VIVE_CONTROLLER_COMPONENTS},
    },
};

static const DeviceProfile INDEX_PROFILE = {
    DeviceType::VALVE_INDEX,

    "Valve Index HMD (Simulated)",
    "Valve Corporation",
    "INDEX-SIM",
    0x28DE, // Valve VID
    0x2012,

    // Display: 1440x1600 per eye
    1440,
    1600,
    1440,
    1600,
    144.0f, // 144Hz

    // FOV (Index has wide FOV)
    -0.959931f, // ~55 degrees
    0.959931f,
    0.959931f,
    -0.959931f,

    true,
    true,

    "/interaction_profiles/valve/index_controller",

    // Devices
    {
        {"/user/head", "hmd", true, {{0.0f, 1.6f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, {}},
        {"/user/hand/left", "left_controller", false, {{-0.2f, 1.4f, -0.3f}, {0.0f, 0.0f, 0.0f, 1.0f}}, INDEX_CONTROLLER_COMPONENTS},
        {"/user/hand/right", "right_controller", false, {{0.2f, 1.4f, -0.3f}, {0.0f, 0.0f, 0.0f, 1.0f}}, INDEX_CONTROLLER_COMPONENTS},
    },
};

// Vive Tracker profile (can have multiple trackers)
static const DeviceProfile VIVE_TRACKER_PROFILE = {
    DeviceType::HTC_VIVE_TRACKER,

    "HTC Vive Tracker (Simulated)",
    "HTC Corporation",
    "VIVETRK-SIM",
    0x0BB4, // HTC VID
    0x0000, // Generic tracker PID

    // Display properties (not applicable for trackers, but required by structure)
    0,
    0,
    0,
    0,
    0.0f,

    // FOV (not applicable)
    0.0f,
    0.0f,
    0.0f,
    0.0f,

    false, // No position tracking for HMD (this is tracker-only)
    false, // No orientation tracking for HMD

    "/interaction_profiles/htc/vive_tracker_htcx",

    // Devices - Trackers only (no HMD)
    {
        // Trackers with common roles - set to active by default
        {"/user/vive_tracker_htcx/role/waist", "waist_tracker", true, {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, VIVE_TRACKER_COMPONENTS},
        {"/user/vive_tracker_htcx/role/left_foot", "left_foot_tracker", true, {{-0.15f, 0.1f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, VIVE_TRACKER_COMPONENTS},
        {"/user/vive_tracker_htcx/role/right_foot", "right_foot_tracker", true, {{0.15f, 0.1f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, VIVE_TRACKER_COMPONENTS},
        {"/user/vive_tracker_htcx/role/left_shoulder", "left_shoulder_tracker", true, {{-0.2f, 1.5f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, VIVE_TRACKER_COMPONENTS},
        {"/user/vive_tracker_htcx/role/right_shoulder", "right_shoulder_tracker", true, {{0.2f, 1.5f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, VIVE_TRACKER_COMPONENTS},
    },
};

// clang-format on

// Map for easy lookup
static const std::map<std::string, DeviceType> NAME_TO_TYPE = {
    {"oculus_quest_2", DeviceType::OCULUS_QUEST_2},
    {"oculus_quest_3", DeviceType::OCULUS_QUEST_3},
    {"htc_vive", DeviceType::HTC_VIVE},
    {"valve_index", DeviceType::VALVE_INDEX},
    {"htc_vive_tracker", DeviceType::HTC_VIVE_TRACKER},
};
// clang-format on

const DeviceProfile& GetDeviceProfile(DeviceType type) {
    switch (type) {
        case DeviceType::OCULUS_QUEST_2:
            return QUEST_2_PROFILE;
        case DeviceType::OCULUS_QUEST_3:
            return QUEST_3_PROFILE;
        case DeviceType::HTC_VIVE:
            return VIVE_PROFILE;
        case DeviceType::VALVE_INDEX:
            return INDEX_PROFILE;
        case DeviceType::HTC_VIVE_TRACKER:
            return VIVE_TRACKER_PROFILE;
        default:
            throw std::runtime_error("Unknown device type");
    }
}

const char* GetDeviceProfileId(DeviceType type) {
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

const DeviceProfile* GetDeviceProfileByName(const std::string& name) {
    auto it = NAME_TO_TYPE.find(name);
    if (it != NAME_TO_TYPE.end()) {
        return &GetDeviceProfile(it->second);
    }
    return nullptr;
}

DeviceType GetDeviceTypeByName(const std::string& name) {
    auto it = NAME_TO_TYPE.find(name);
    if (it != NAME_TO_TYPE.end()) {
        return it->second;
    }
    throw std::runtime_error("Unknown device name: " + name);
}

}  // namespace ox_sim
