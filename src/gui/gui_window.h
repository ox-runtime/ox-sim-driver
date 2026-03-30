#pragma once

#include <ox_sim.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "device_profiles.hpp"
#include "vog.h"

namespace ox_sim {

class GuiWindow {
   public:
    GuiWindow();
    ~GuiWindow();

    bool Start();

    // Signal the window to close and wait for it to finish.
    void Stop();

    bool IsRunning() const { return window_.IsRunning(); }

   private:
    struct PreviewImageRect {
        int eye_index = -1;
        bool visible = false;
        ImVec2 min = ImVec2(0.0f, 0.0f);
        ImVec2 max = ImVec2(0.0f, 0.0f);
    };

    // ImGui widget callback — called once per frame by vog::Window.
    void RenderFrame();

    void RenderDevicePanel(const DeviceDef& device, int device_index, float panel_width);
    void RenderComponentControl(const DeviceDef& device, const ComponentDef& component, int device_index,
                                float label_col_w, float content_start_x);
    void RenderRotationControl(const DeviceDef& device, int device_index, XrPosef& pose, XrBool32 is_active);
    void RenderFramePreview();
    void UpdateFrameTextures();
    void HandlePreviewInteraction(const ImVec2& preview_min, const ImVec2& preview_max,
                                  const std::vector<PreviewImageRect>& image_rects, const ImVec2& composite_min,
                                  const ImVec2& composite_max, bool has_image);
    bool UpdatePreviewHoverText(const std::vector<PreviewImageRect>& image_rects);
    void HandlePreviewNavigation(bool allow_navigation, bool block_navigation);
    bool CopyCurrentPreviewToClipboard();
    bool CopyPreviewPixelsToClipboard(const std::vector<uint8_t>& pixels, uint32_t width, uint32_t height);
    bool GetHeadPose(XrPosef& pose, XrBool32& is_active) const;

    // Euler cache for rotation UI
    struct EulerCache {
        XrVector3f euler;
        XrQuaternionf quat;
    };
    std::unordered_map<std::string, EulerCache> euler_cache_;

    vog::Window window_;

    // UI state
    int selected_device_type_ = 0;
    int preview_eye_selection_ = 0;
    std::string status_message_{"Ready"};
    std::string preview_hover_text_;
    float sidebar_w_{360.0f};           // resizable via splitter drag
    bool last_splitter_active_{false};  // true if splitter was being dragged last frame
    bool preview_has_focus_{false};
    bool preview_drag_active_{false};

    // Frame preview textures (OpenGL texture IDs)
    uint32_t preview_textures_[2] = {0, 0};
    uint32_t preview_width_ = 0;
    uint32_t preview_height_ = 0;
    bool preview_textures_valid_ = false;
    XrTime last_preview_frame_time_ = 0;
    std::vector<uint8_t> preview_pixels_[2];
};

}  // namespace ox_sim
