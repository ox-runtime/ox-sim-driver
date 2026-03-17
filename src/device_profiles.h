#pragma once

#include <ox_driver.h>

#include <string>
#include <vector>

namespace ox_sim {

// Device type enumeration - extensible for future devices
enum class DeviceType {
    OCULUS_QUEST_2,
    OCULUS_QUEST_3,
    HTC_VIVE,
    VALVE_INDEX,
    HTC_VIVE_TRACKER,
    // Add more device types here
};

// Input component type
enum class ComponentType {
    FLOAT,    // Analog values: triggers, grips (0.0 to 1.0)
    BOOLEAN,  // Digital values: clicks, touches (0 or 1)
    VEC2      // 2D vectors: thumbsticks, trackpads (-1.0 to 1.0)
};

// Which axis a FLOAT component represents in a linked VEC2 component
enum class Vec2Axis {
    NONE,  // This component is not a linked axis
    X,     // This component is the X axis of its linked VEC2
    Y,     // This component is the Y axis of its linked VEC2
};

// Component definition for a device
struct ComponentDef {
    const char* path;  // e.g., "/input/trigger/value"
    ComponentType type;
    const char* description;  // Human-readable description

    // Optional: restrict this component to a specific user path (e.g. "/user/hand/left").
    // nullptr means no restriction — the component is visible/active for any device path.
    const char* hand_restriction = nullptr;

    // Optional VEC2 linkage: for a FLOAT component that is the X or Y axis of a VEC2
    // component, set linked_vec2_path to the VEC2's path and linked_axis to X or Y.
    // The simulator will keep the two in sync automatically.  VEC2 components that have
    // FLOAT children linked to them are hidden from the UI (edit via their sub-axes).
    const char* linked_vec2_path = nullptr;
    Vec2Axis linked_axis = Vec2Axis::NONE;
};

// Device definition (HMD, controller, tracker, etc.)
struct DeviceDef {
    const char* user_path;                 // e.g., "/user/head", "/user/hand/left"
    const char* role;                      // e.g., "hmd", "left_controller", "right_controller"
    bool always_active;                    // Whether device is always active (e.g., HMD)
    OxPose default_pose;                   // Default pose for this device
    std::vector<ComponentDef> components;  // Input components for this device
};

// Device profile containing all static properties
struct DeviceProfile {
    DeviceType type;

    // Device info
    const char* name;
    const char* manufacturer;
    const char* serial_prefix;
    uint32_t vendor_id;
    uint32_t product_id;

    // Display properties
    uint32_t display_width;
    uint32_t display_height;
    uint32_t recommended_width;
    uint32_t recommended_height;
    float refresh_rate;

    // Field of view (radians)
    float fov_left;
    float fov_right;
    float fov_up;
    float fov_down;

    // Tracking capabilities
    bool has_position_tracking;
    bool has_orientation_tracking;

    // Interaction profile
    const char* interaction_profile;  // e.g., "/interaction_profiles/oculus/touch_controller"

    // Devices that make up this system (HMD, controllers, trackers, etc.)
    std::vector<DeviceDef> devices;
};

// Get device profile by type
const DeviceProfile& GetDeviceProfile(DeviceType type);

// Get the stable external identifier for a device type
const char* GetDeviceProfileId(DeviceType type);

// Get device profile by name (for config file)
const DeviceProfile* GetDeviceProfileByName(const std::string& name);

// Get device type by name
DeviceType GetDeviceTypeByName(const std::string& name);

}  // namespace ox_sim
