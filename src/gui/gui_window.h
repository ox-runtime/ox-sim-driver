#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "simulator_core.h"
#include "vog.h"

namespace ox_sim {

class GuiWindow {
   public:
    GuiWindow();
    ~GuiWindow();

    // Start the GUI window. device_profile_ptr points at the current device profile
    // pointer so that device switching is reflected immediately.
    bool Start(SimulatorCore* simulator, std::atomic<const DeviceProfile*>* device_profile_ptr);

    // Signal the window to close and wait for it to finish.
    void Stop();

    bool IsRunning() const { return window_.IsRunning(); }

   private:
    // ImGui widget callback — called once per frame by vog::Window.
    void RenderFrame();

    void RenderDevicePanel(const DeviceDef& device, int device_index, float panel_width);
    void RenderComponentControl(const DeviceDef& device, const ComponentDef& component, int device_index,
                                float label_col_w, float content_start_x);
    void RenderRotationControl(const DeviceDef& device, int device_index, OxPose& pose, bool is_active);
    void RenderFramePreview();
    void UpdateFrameTextures();

    // Utility functions for rotation handling
    static void QuatToEuler(const OxQuaternion& q, OxVector3f& euler);
    static void ApplyRotation(OxQuaternion& q, const OxVector3f& axis, float angle);

    // Euler cache for rotation UI
    struct EulerCache {
        OxVector3f euler;  // x=roll, y=pitch, z=yaw in degrees
        OxQuaternion quat;
    };
    std::unordered_map<std::string, EulerCache> euler_cache_;

    vog::Window window_;

    SimulatorCore* simulator_ = nullptr;
    std::atomic<const DeviceProfile*>* device_profile_ptr_ = nullptr;

    // UI state
    int selected_device_type_ = 0;
    int preview_eye_selection_ = 0;
    std::string status_message_{"Ready"};
    float sidebar_w_{360.0f};           // resizable via splitter drag
    bool last_splitter_active_{false};  // true if splitter was being dragged last frame

    // Frame preview textures (OpenGL texture IDs)
    uint32_t preview_textures_[2] = {0, 0};
    uint32_t preview_width_ = 0;
    uint32_t preview_height_ = 0;
    bool preview_textures_valid_ = false;
};

}  // namespace ox_sim
