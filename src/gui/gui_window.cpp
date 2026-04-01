#include "gui_window.h"

#include <GLFW/glfw3.h>
#ifndef CLIP_ENABLE_IMAGE
#define CLIP_ENABLE_IMAGE 1
#endif
#include <clip.h>
#include <ox_sim.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "device_profiles.hpp"
#include "imgui_impl_opengl3.h"
#include "math.hpp"
#include "rest_api/http_server.h"
#include "utils.hpp"
#include "vog.h"

namespace ox_sim {

namespace {

namespace sim_math = ox_sim::math;

std::string WindowTitle() {
    std::string title = "ox simulator";
#if defined(OX_VERSION_MAJOR) && defined(OX_VERSION_MINOR) && defined(OX_VERSION_PATCH)
    title += " v" + std::to_string(OX_VERSION_MAJOR) + "." + std::to_string(OX_VERSION_MINOR) + "." +
             std::to_string(OX_VERSION_PATCH);
#endif
    return title;
}

const DeviceProfile* current_profile() {
    char profile_name[64] = {};
    if (ox_sim_get_current_profile(profile_name, sizeof(profile_name)) != OX_SIM_SUCCESS) {
        return nullptr;
    }

    return GetDeviceProfileByName(profile_name);
}

bool is_session_active() {
    XrSessionState state = XR_SESSION_STATE_UNKNOWN;
    if (ox_sim_get_session_state(&state) != OX_SIM_SUCCESS) {
        return false;
    }

    return state == XR_SESSION_STATE_SYNCHRONIZED || state == XR_SESSION_STATE_VISIBLE ||
           state == XR_SESSION_STATE_FOCUSED;
}

clip::image_spec make_clip_rgba_image_spec(uint32_t width, uint32_t height) {
    clip::image_spec spec;
    spec.width = width;
    spec.height = height;
    spec.bits_per_pixel = 32;
    spec.bytes_per_row = width * 4;
    spec.red_mask = 0x000000ff;
    spec.green_mask = 0x0000ff00;
    spec.blue_mask = 0x00ff0000;
    spec.alpha_mask = 0xff000000;
    spec.red_shift = 0;
    spec.green_shift = 8;
    spec.blue_shift = 16;
    spec.alpha_shift = 24;
    return spec;
}

std::vector<uint8_t> ComposeSideBySidePreview(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right,
                                              uint32_t width, uint32_t height) {
    std::vector<uint8_t> composite(static_cast<size_t>(width) * 2 * height * 4, 0);
    const size_t row_bytes = static_cast<size_t>(width) * 4;
    const size_t composite_row_bytes = row_bytes * 2;

    for (uint32_t y = 0; y < height; ++y) {
        uint8_t* dst_row = composite.data() + static_cast<size_t>(y) * composite_row_bytes;
        if (!left.empty()) {
            std::memcpy(dst_row, left.data() + static_cast<size_t>(y) * row_bytes, row_bytes);
        }
        if (!right.empty()) {
            std::memcpy(dst_row + row_bytes, right.data() + static_cast<size_t>(y) * row_bytes, row_bytes);
        }
    }

    return composite;
}

}  // namespace

GuiWindow::GuiWindow() = default;
GuiWindow::~GuiWindow() { Stop(); }

bool GuiWindow::Start() {
    if (window_.IsRunning()) {
        spdlog::error("GuiWindow::Start: GUI already running");
        return false;
    }

    if (const DeviceProfile* profile = current_profile()) {
        selected_device_type_ = static_cast<int>(profile->type);
    }

    spdlog::info("Initializing GUI window...");
    vog::WindowConfig cfg{WindowTitle(), 1280, 720};

    // No window padding
    vog::Theme theme;
    theme.vars.window_padding = ImVec2(0, 0);
    cfg.theme = theme;

    return window_.Start(cfg, [this]() { RenderFrame(); });
}

void GuiWindow::Stop() { window_.Stop(); }

// ---------------------------------------------------------------------------
// RenderFrame — pure ImGui widget calls, invoked once per frame by
// vog::Window between NewFrame() and Render().
// ---------------------------------------------------------------------------

void GuiWindow::RenderFrame() {
    const vog::ThemeColors& tc = vog::Window::GetTheme().colors;

    ImVec2 content_size = ImGui::GetContentRegionAvail();
    const ImGuiStyle& style = ImGui::GetStyle();

    // ========== TOP TOOLBAR STRIP ==========
    const float top_toolbar_h = 48.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, tc.bg.value());
    ImGui::BeginChild("TopToolbar", ImVec2(0, top_toolbar_h), false, ImGuiWindowFlags_NoScrollbar);
    {
        const float btn_runtime_w = 190.0f;
        const float btn_icon_w = ImGui::GetFrameHeight();
        const float btn_api_w = 160.0f;
        const float lbl_device_w = ImGui::CalcTextSize("Device:").x + style.ItemSpacing.x;
        const float combo_device_w = 190.0f;
        const float spacing = style.ItemSpacing.x * 3.0f;
        const float total_w =
            btn_runtime_w + btn_icon_w + spacing + lbl_device_w + combo_device_w + spacing + btn_api_w + btn_icon_w;
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        float start_x = (avail.x - total_w) * 0.5f;
        if (start_x < 0.0f) start_x = 0.0f;
        float center_y = (avail.y - ImGui::GetFrameHeight()) * 0.5f;
        if (center_y < 0.0f) center_y = 0.0f;
        ImGui::SetCursorPos(ImVec2(start_x, center_y));

        if (ImGui::Button("Set as OpenXR Runtime", ImVec2(btn_runtime_w, 0))) {
            ox_sim::utils::SetAsOpenXRRuntime(status_message_);
        }
        vog::widgets::ShowItemTooltip("Register ox simulator as the active OpenXR runtime on this system");

        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_COPY "##copy_runtime_path")) {
            glfwSetClipboardString(window_.GetNativeWindow(), ox_sim::utils::GetRuntimeJsonPath().string().c_str());
            status_message_ = "Copied runtime path to clipboard";
        }
        vog::widgets::ShowItemTooltip(
            "Copy the path to the OpenXR runtime JSON file to clipboard. Set this as the XR_RUNTIME_JSON environment "
            "variable.");

        ImGui::SameLine(0, spacing);

        HttpServer& http_server = GetHttpServer();
        bool api_on = http_server.IsRunning();
        if (vog::widgets::ToggleButton("API Server:", &api_on, false)) {
            if (api_on) {
                if (http_server.Start(kHttpServerPort)) {
                    status_message_ = std::string("API Server enabled (port ") + std::to_string(kHttpServerPort) + ")";
                } else {
                    status_message_ =
                        std::string("Failed to start API server on port ") + std::to_string(kHttpServerPort);
                }
            } else {
                http_server.Stop();
                status_message_ = "API Server disabled";
            }
        }
        vog::widgets::ShowItemTooltip("Toggle the local HTTP API server on port 8765");

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_GLOBE "##copy_api_url")) {
            const std::string api_url = std::string("http://127.0.0.1:") + std::to_string(kHttpServerPort);
            glfwSetClipboardString(window_.GetNativeWindow(), api_url.c_str());
            status_message_ = "Copied API URL to clipboard";
        }
        vog::widgets::ShowItemTooltip("Copy the local HTTP API base URL to the clipboard");

        ImGui::SameLine(0, spacing);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Simulated Device:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(combo_device_w);
        const char* device_names[] = {"Meta Quest 2", "Meta Quest 3", "HTC Vive", "Valve Index", "HTC Vive Tracker"};
        int current_device = selected_device_type_;
        if (vog::widgets::Combo("##DeviceSelect", &current_device, device_names, IM_ARRAYSIZE(device_names))) {
            DeviceType new_type = static_cast<DeviceType>(current_device);
            const DeviceProfile& new_profile = GetDeviceProfile(new_type);
            if (ox_sim_set_current_profile(GetDeviceProfileId(new_type)) == OX_SIM_SUCCESS) {
                selected_device_type_ = current_device;
                status_message_ = std::string("Switched to ") + new_profile.name;
            } else {
                status_message_ = "Failed to switch device profile";
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // ========== MAIN AREA: Preview (left) + Splitter + Sidebar (right) ==========
    const float splitter_w = 5.0f;
    const float status_bar_h = 30.0f;
    const float main_area_h = content_size.y - top_toolbar_h - status_bar_h - style.ItemSpacing.y;

    // Apply the splitter drag delta BEFORE computing layout so both panels
    // use the same width within a single frame (eliminates one-frame lag).
    if (last_splitter_active_) {
        sidebar_w_ -= ImGui::GetIO().MouseDelta.x;
    }
    sidebar_w_ = std::clamp(sidebar_w_, 200.0f, content_size.x - 200.0f - splitter_w);

    const float preview_w = content_size.x - sidebar_w_ - splitter_w;

    // ---- Preview ----
    const float preview_padding = 5.0f;
    ImGui::SetCursorPos(ImVec2(preview_padding, top_toolbar_h));
    ImGui::BeginChild("PreviewArea", ImVec2(preview_w - preview_padding, main_area_h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        RenderFramePreview();
    }
    ImGui::EndChild();

    // ---- Splitter handle ----
    ImGui::SetCursorPos(ImVec2(preview_w, top_toolbar_h));
    ImGui::InvisibleButton("##splitter", ImVec2(splitter_w, main_area_h));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    last_splitter_active_ = ImGui::IsItemActive();  // consumed next frame, before layout
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 tl = ImGui::GetItemRectMin();
        ImVec2 br = ImGui::GetItemRectMax();
        float line_x = std::round((tl.x + br.x) * 0.5f);
        ImU32 col = ImGui::IsItemHovered() || ImGui::IsItemActive() ? ImGui::GetColorU32(tc.accent.value())
                                                                    : ImGui::GetColorU32(tc.bg.value());
        dl->AddLine(ImVec2(line_x, tl.y), ImVec2(line_x, br.y), col, 3.0f);
    }

    // ---- Sidebar ----
    ImGui::SetCursorPos(ImVec2(preview_w + splitter_w, top_toolbar_h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, tc.panel1.value());
    ImGui::BeginChild("Sidebar", ImVec2(sidebar_w_, main_area_h), false);
    {
        if (const DeviceProfile* profile = current_profile()) {
            // Use the actual usable width so the panel border always fills edge-to-edge.
            const float inner_w = ImGui::GetContentRegionAvail().x;
            for (size_t i = 0; i < profile->devices.size(); i++) {
                if (i > 0) ImGui::Spacing();
                RenderDevicePanel(profile->devices[i], static_cast<int>(i), inner_w);
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // ---- Status bar ----
    ImGui::BeginChild("StatusBar", ImVec2(0, status_bar_h), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImGui::Separator();
        ImGui::Indent(5.0f);
        if (const DeviceProfile* p = current_profile()) {
            uint32_t app_fps = 0;
            if (is_session_active()) {
                ox_sim_get_app_fps(&app_fps);
            }
            ImGui::Text("Display: %dx%d @ %u fps", p->display_width, p->display_height, app_fps);
            if (!preview_hover_text_.empty()) {
                ImGui::SameLine();
                ImGui::Text("| %s", preview_hover_text_.c_str());
            }
            ImGui::SameLine();
            ImGui::Text("| %s", status_message_.c_str());
        } else {
            ImGui::Text("%s", status_message_.c_str());
        }
        ImGui::Unindent(5.0f);
    }
    ImGui::EndChild();
}

void GuiWindow::RenderFramePreview() {
    using vog::widgets::Combo;
    const vog::ThemeColors& tc = vog::Window::GetTheme().colors;
    preview_hover_text_.clear();

    UpdateFrameTextures();

    const ImVec2 region = ImGui::GetContentRegionAvail();
    const float toolbar_h = 38.0f;
    const float content_h = region.y - toolbar_h - ImGui::GetStyle().ItemSpacing.y;
    const bool has_image = preview_textures_valid_ && preview_width_ > 0 && preview_height_ > 0;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, tc.panel2.value());
    ImGui::BeginChild("PreviewToolbar", ImVec2(0, toolbar_h), false, ImGuiWindowFlags_NoScrollbar);
    {
        const float right_padding = 8.0f;
        const float combo_w = 80.0f;
        const float label_w = ImGui::CalcTextSize("View:").x + ImGui::GetStyle().ItemSpacing.x;
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        float center_y = (avail.y - ImGui::GetFrameHeight()) * 0.5f;
        if (center_y < 0.0f) center_y = 0.0f;

        // Left: session active indicator with filled circle
        ImGui::SetCursorPos(ImVec2(8.0f, center_y));
        ImGui::AlignTextToFramePadding();
        const bool session_active = is_session_active();
        if (session_active) {
            ImGui::TextColored(tc.positive.value(), ICON_FA_CIRCLE "  Session: Active");
        } else {
            ImGui::TextColored(tc.text_muted.value(), ICON_FA_CIRCLE "  Session: Inactive");
        }

        // Right: view select combo
        float cursor_x = avail.x - combo_w - label_w - right_padding;
        if (cursor_x < 0.0f) cursor_x = 0.0f;
        ImGui::SetCursorPos(ImVec2(cursor_x, center_y));
        ImGui::AlignTextToFramePadding();
        ImGui::Text("View:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(combo_w);
        const char* eye_names[] = {"Left", "Right", "Both"};
        Combo("##EyeSelect", &preview_eye_selection_, eye_names, 3);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, tc.panel1.value());
    ImGui::BeginChild("PreviewContent", ImVec2(0, content_h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs);
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        std::vector<PreviewImageRect> image_rects;
        ImVec2 composite_min(0.0f, 0.0f);
        ImVec2 composite_max(0.0f, 0.0f);
        auto capture_image_rect = [&](int eye_index) {
            PreviewImageRect rect;
            rect.eye_index = eye_index;
            rect.visible = true;
            rect.min = ImGui::GetItemRectMin();
            rect.max = ImGui::GetItemRectMax();
            image_rects.push_back(rect);

            if (image_rects.size() == 1) {
                composite_min = rect.min;
                composite_max = rect.max;
            } else {
                composite_min.x = std::min(composite_min.x, rect.min.x);
                composite_min.y = std::min(composite_min.y, rect.min.y);
                composite_max.x = std::max(composite_max.x, rect.max.x);
                composite_max.y = std::max(composite_max.y, rect.max.y);
            }
        };

        auto draw_preview_texture = [&](int eye, const ImVec2& cursor_pos, const ImVec2& image_size) {
            ImGui::SetCursorPos(cursor_pos);
            if (!preview_textures_[eye]) {
                ImGui::Dummy(image_size);
                return;
            }

            ImGui::Image((ImTextureID)(intptr_t)preview_textures_[eye], image_size, ImVec2(0, 0), ImVec2(1, 1),
                         ImVec4(1, 1, 1, 1), tc.border.value());
            capture_image_rect(eye);
        };

        if (!has_image) {
            const char* msg = "No image received";
            ImVec2 ts = ImGui::CalcTextSize(msg);
            ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
            ImGui::Text("%s", msg);
        } else if (preview_eye_selection_ == 2) {
            const float aspect = (float)preview_width_ / (float)preview_height_;
            float w_each = avail.x * 0.5f;
            float h_each = w_each / aspect;
            if (h_each > avail.y) {
                h_each = avail.y;
                w_each = h_each * aspect;
            }
            const float y_off = (avail.y - h_each) * 0.5f;
            const float left_x = std::max(0.0f, avail.x * 0.5f - w_each);
            const float right_x = left_x + w_each;
            draw_preview_texture(0, ImVec2(left_x, y_off), ImVec2(w_each, h_each));
            draw_preview_texture(1, ImVec2(right_x, y_off), ImVec2(w_each, h_each));
        } else {
            const int eye = (preview_eye_selection_ == 1) ? 1 : 0;
            const char* no_msg = (eye == 1) ? "No image received (right eye)" : "No image received (left eye)";
            if (preview_textures_[eye]) {
                const float aspect = (float)preview_width_ / (float)preview_height_;
                float img_w = avail.x;
                float img_h = img_w / aspect;
                if (img_h > avail.y) {
                    img_h = avail.y;
                    img_w = img_h * aspect;
                }
                const float x_off = (avail.x - img_w) * 0.5f;
                const float y_off = (avail.y - img_h) * 0.5f;
                draw_preview_texture(eye, ImVec2(x_off, y_off), ImVec2(img_w, img_h));
            } else {
                ImVec2 ts = ImGui::CalcTextSize(no_msg);
                ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
                ImGui::Text("%s", no_msg);
            }
        }

        const ImVec2 preview_min = ImGui::GetWindowPos();
        const ImVec2 preview_max(preview_min.x + ImGui::GetWindowSize().x, preview_min.y + ImGui::GetWindowSize().y);
        HandlePreviewInteraction(preview_min, preview_max, image_rects, composite_min, composite_max, has_image);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void GuiWindow::UpdateFrameTextures() {
    OxSimFramePreview frame_preview = {};
    if (ox_sim_get_frame_preview(&frame_preview) != OX_SIM_SUCCESS) return;
    if (frame_preview.frame_time == 0 || frame_preview.frame_time == last_preview_frame_time_) {
        return;
    }

    uint32_t w = frame_preview.width;
    uint32_t h = frame_preview.height;
    last_preview_frame_time_ = frame_preview.frame_time;
    if (w == 0 || h == 0) return;

    for (int eye = 0; eye < 2; ++eye) {
        if (!frame_preview.pixel_data[eye]) continue;
        size_t expected_size = w * h * 4;
        if (frame_preview.data_size[eye] != expected_size) continue;

        const uint8_t* pixels = static_cast<const uint8_t*>(frame_preview.pixel_data[eye]);
        preview_pixels_[eye].assign(pixels, pixels + expected_size);

        if (!preview_textures_[eye]) {
            glGenTextures(1, &preview_textures_[eye]);
            glBindTexture(GL_TEXTURE_2D, preview_textures_[eye]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            spdlog::info("[GUI] Created OpenGL texture {} for eye {}", preview_textures_[eye], eye);
        }
        glBindTexture(GL_TEXTURE_2D, preview_textures_[eye]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    preview_width_ = w;
    preview_height_ = h;
    preview_textures_valid_ = preview_textures_[0] != 0 || preview_textures_[1] != 0;
}

void GuiWindow::RenderDevicePanel(const DeviceDef& device, int device_index, float panel_width) {
    using vog::widgets::ShowItemTooltip;
    const vog::ThemeColors& tc = vog::Window::GetTheme().colors;
    ImGui::PushID(device_index);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float pad = 8.0f;
    const float rounding = 4.0f;
    const ImVec2 panel_tl = ImGui::GetCursorScreenPos();

    ImGui::SetCursorScreenPos(ImVec2(panel_tl.x + pad, panel_tl.y + pad));
    ImGui::BeginGroup();
    ImGui::PushItemWidth(panel_width - pad * 2.0f);

    // Capture window-relative X so all columns can be rooted consistently.
    const float content_start_x = ImGui::GetCursorPosX();

    std::string device_label = std::string(device.role);
    ImGui::TextColored(tc.accent.value(), "%s", device_label.c_str());
    ImGui::SameLine();
    ImGui::TextColored(tc.text_muted.value(), "(%s)", device.user_path);
    ImGui::Separator();

    XrBool32 is_active = XR_FALSE;
    XrPosef pose = {{0, 0, 0, 1}, {0, 0, 0}};
    ox_sim_get_device_pose(device.user_path, &pose, &is_active);

    if (!device.always_active) {
        bool active_toggle = is_active;
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Active");
        ImGui::SameLine();
        if (vog::widgets::ToggleButton("", &active_toggle)) {
            ox_sim_set_device_pose(device.user_path, &pose, active_toggle ? XR_TRUE : XR_FALSE);
        }
        ShowItemTooltip("Enable/disable device tracking");
    } else {
        ImGui::TextColored(tc.positive.value(), "Active: Always On");
        ShowItemTooltip("This device is always active");
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Position and Rotation — fixed label column so both float-triple rows start at the same X.
    // drag_w is computed live (after SameLine) so it adapts to any sidebar width.
    const float pos_lbl_col = std::max(ImGui::CalcTextSize("Position:").x, ImGui::CalcTextSize("Rotation:").x) + 8.0f;

    ImGui::SetCursorPosX(content_start_x);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Position:");
    ImGui::SameLine(content_start_x + pos_lbl_col);
    float pos[3] = {pose.position.x, pose.position.y, pose.position.z};
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - pad);
    if (ImGui::DragFloat3("##Position", pos, 0.01f, -10.0f, 10.0f, "%.4f")) {
        pose.position = {pos[0], pos[1], pos[2]};
        ox_sim_set_device_pose(device.user_path, &pose, is_active);
    }

    // Rotation — gimbal-lock-free via per-device cached Euler state.
    // Each drag delta is applied as an incremental world-space rotation so axes
    // remain independent regardless of the current orientation.
    ImGui::SetCursorPosX(content_start_x);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Rotation:");
    ImGui::SameLine(content_start_x + pos_lbl_col);
    RenderRotationControl(device, device_index, pose, is_active);

    ImGui::Spacing();
    if (ImGui::Button("Reset Pose", ImVec2(ImGui::GetContentRegionAvail().x - pad, 0))) {
        XrPosef default_pose = device.default_pose;
        ox_sim_set_device_pose(device.user_path, &default_pose, is_active);
    }

    // ---- Input Components ----
    // Predicate: should this component be shown in the UI for this device?
    //   - Filters out hand-restricted components that don't match the device's user_path.
    //   - Hides VEC2 "parent" components whose x/y axes are exposed as linked FLOATs
    //     (those should be edited through the individual axis sliders, not as a 2D widget).
    auto ShouldShowComponent = [&](const ComponentDef& comp) -> bool {
        // Hand restriction check
        if (comp.hand_restriction != nullptr && std::strcmp(comp.hand_restriction, device.user_path) != 0) return false;
        // Hide VEC2 components that have linked FLOAT axis children
        if (comp.type == ComponentType::VEC2) {
            for (const auto& c : device.components) {
                if (c.linked_vec2_path != nullptr && std::strcmp(c.linked_vec2_path, comp.path) == 0) return false;
            }
        }
        return true;
    };

    // Collect visible components and compute the label column width in one pass.
    std::vector<const ComponentDef*> visible_comps;
    float label_col_w = 0.0f;
    for (const auto& comp : device.components) {
        if (!ShouldShowComponent(comp)) continue;
        visible_comps.push_back(&comp);
        float lw = ImGui::CalcTextSize(comp.description).x + ImGui::CalcTextSize(":").x;
        if (lw > label_col_w) label_col_w = lw;
    }
    label_col_w += 20.0f;  // gap between right edge of label and left edge of control

    if (!visible_comps.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(tc.warning.value(), "Input Components");
        ImGui::Spacing();
        for (const ComponentDef* comp : visible_comps) {
            RenderComponentControl(device, *comp, device_index, label_col_w, content_start_x);
        }
    }

    ImGui::PopItemWidth();
    ImGui::EndGroup();

    const ImVec2 group_br = ImGui::GetItemRectMax();
    const ImVec2 panel_br = ImVec2(panel_tl.x + panel_width, group_br.y + pad);
    draw_list->AddRect(panel_tl, panel_br, ImGui::ColorConvertFloat4ToU32(tc.border.value()), rounding);

    ImGui::SetCursorScreenPos(ImVec2(panel_tl.x, panel_br.y));
    ImGui::Dummy(ImVec2(panel_width, 0.0f));

    ImGui::PopID();
}

void GuiWindow::RenderComponentControl(const DeviceDef& device, const ComponentDef& component, int device_index,
                                       float label_col_w, float content_start_x) {
    ImGui::PushID(component.path);

    // Right-align the label text within the label column.
    const float lw = ImGui::CalcTextSize(component.description).x + ImGui::CalcTextSize(":").x;
    ImGui::SetCursorPosX(content_start_x + label_col_w - lw);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s:", component.description);
    ImGui::SameLine(content_start_x + label_col_w);

    switch (component.type) {
        case ComponentType::BOOLEAN: {
            uint32_t raw_value = 0;
            ox_sim_get_input_state_boolean(device.user_path, component.path, &raw_value);
            bool value = raw_value != 0;
            // Use empty label so no label text is rendered (we already drew the description above).
            if (vog::widgets::ToggleButton("", &value)) {
                ox_sim_set_input_state_boolean(device.user_path, component.path, value ? 1u : 0u);
            }
            break;
        }
        case ComponentType::FLOAT: {
            float value = 0.0f;
            ox_sim_get_input_state_float(device.user_path, component.path, &value);
            // Linked axis components (thumbstick/trackpad x-y) have a -1..1 range;
            // all other FLOAT components (triggers, grips) use 0..1.
            const float v_min = (component.linked_vec2_path != nullptr) ? -1.0f : 0.0f;
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderFloat("##value", &value, v_min, 1.0f, "%.2f")) {
                ox_sim_set_input_state_float(device.user_path, component.path, value);
            }
            break;
        }
        case ComponentType::VEC2: {
            // Standalone VEC2 (no linked FLOAT axes); show as a double-width slider pair.
            XrVector2f value = {0.0f, 0.0f};
            ox_sim_get_input_state_vector2f(device.user_path, component.path, &value);
            float vec2[2] = {value.x, value.y};
            ImGui::SetNextItemWidth(100.0f * 2.0f + ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::SliderFloat2("##vec2", vec2, -1.0f, 1.0f, "%.2f")) {
                value.x = vec2[0];
                value.y = vec2[1];
                ox_sim_set_input_state_vector2f(device.user_path, component.path, &value);
            }
            break;
        }
    }

    ImGui::PopID();
    ImGui::Spacing();
}

// Rotation control with gimbal-lock-free incremental updates via cached Euler angles per device.
void GuiWindow::RenderRotationControl(const DeviceDef& device, int device_index, XrPosef& pose, XrBool32 is_active) {
    const std::string key = std::string(device.user_path) + "_" + std::to_string(device_index);
    auto it = euler_cache_.find(key);
    if (it == euler_cache_.end()) {
        EulerCache ec;
        ec.quat = pose.orientation;
        ec.euler = sim_math::EulerDegrees(pose.orientation);
        euler_cache_[key] = ec;
    }
    auto& ec = euler_cache_[key];

    // If the quaternion changed externally (Reset Pose / API), re-derive Euler.
    if (ec.quat.x != pose.orientation.x || ec.quat.y != pose.orientation.y || ec.quat.z != pose.orientation.z ||
        ec.quat.w != pose.orientation.w) {
        ec.euler = sim_math::EulerDegrees(pose.orientation);
        ec.quat = pose.orientation;
    }

    // OpenXR right-handed Y-up: euler.x=pitch, euler.y=yaw, euler.z=roll — no shuffling needed.
    float rot[3] = {ec.euler.x, ec.euler.y, ec.euler.z};
    const float pad = 8.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - pad);
    if (ImGui::DragFloat3("##Rotation", rot, 1.0f, -FLT_MAX, FLT_MAX, "%.2f°")) {
        const float dp = glm::radians(rot[0] - ec.euler.x);
        const float dy = glm::radians(rot[1] - ec.euler.y);
        const float dr = glm::radians(rot[2] - ec.euler.z);

        glm::quat orientation = sim_math::ToGlm(pose.orientation);
        orientation = sim_math::RotateWorld(orientation, sim_math::kLocalRight, dp);
        orientation = sim_math::RotateWorld(orientation, sim_math::kLocalUp, dy);
        orientation = sim_math::RotateWorld(orientation, sim_math::kWorldZ, dr);
        pose.orientation = sim_math::ToXr(orientation);

        ec.euler = {rot[0], rot[1], rot[2]};
        ec.quat = pose.orientation;
        ox_sim_set_device_pose(device.user_path, &pose, is_active);
    }
}

bool GuiWindow::GetHeadPose(XrPosef& pose, XrBool32& is_active) const {
    return ox_sim_get_device_pose("/user/head", &pose, &is_active) == OX_SIM_SUCCESS;
}

bool GuiWindow::UpdatePreviewHoverText(const std::vector<PreviewImageRect>& image_rects) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    preview_hover_text_.clear();

    for (const PreviewImageRect& rect : image_rects) {
        if (!rect.visible) {
            continue;
        }

        if (mouse.x < rect.min.x || mouse.x > rect.max.x || mouse.y < rect.min.y || mouse.y > rect.max.y) {
            continue;
        }

        const float width = std::max(rect.max.x - rect.min.x, 1.0f);
        const float height = std::max(rect.max.y - rect.min.y, 1.0f);
        const uint32_t pixel_x = std::min(static_cast<uint32_t>(((mouse.x - rect.min.x) / width) * preview_width_),
                                          preview_width_ > 0 ? preview_width_ - 1 : 0);
        const uint32_t pixel_y = std::min(static_cast<uint32_t>(((mouse.y - rect.min.y) / height) * preview_height_),
                                          preview_height_ > 0 ? preview_height_ - 1 : 0);
        const char* eye_name = rect.eye_index == 1 ? "Cursor: Right" : "Cursor: Left";
        preview_hover_text_ =
            std::string(eye_name) + " (" + std::to_string(pixel_x) + ", " + std::to_string(pixel_y) + ")";
        return true;
    }

    return false;
}

void GuiWindow::HandlePreviewNavigation(bool allow_navigation, bool block_navigation) {
    if (!allow_navigation || block_navigation) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return;
    }

    XrPosef head_pose = {};
    XrBool32 is_active = XR_FALSE;
    if (!GetHeadPose(head_pose, is_active)) {
        return;
    }

    const float speed = (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) ? 4.5f : 1.5f;
    bool pose_dirty = false;

    // --- Translation (WASD + R/F) ---
    // Use an FPS-style basis: forward/right come from yaw only, and R/F stay world vertical.
    {
        const glm::quat orientation = sim_math::ToGlm(head_pose.orientation);
        const glm::vec3 horizontal_forward = sim_math::HorizontalForwardAxis(orientation);
        const glm::vec3 horizontal_right = sim_math::HorizontalRightAxis(orientation);
        glm::vec3 move(0.0f);
        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            move += horizontal_forward;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            move -= horizontal_forward;
        }
        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            move += horizontal_right;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            move -= horizontal_right;
        }
        if (ImGui::IsKeyDown(ImGuiKey_R)) {
            move += sim_math::kWorldUp;
        }
        if (ImGui::IsKeyDown(ImGuiKey_F)) {
            move -= sim_math::kWorldUp;
        }

        if (io.DeltaTime > 0.0f) {
            const float move_length = glm::length(move);
            if (move_length > 0.0f) {
                const glm::vec3 translated =
                    sim_math::ToGlm(head_pose.position) + (move / move_length) * speed * io.DeltaTime;
                head_pose.position = sim_math::ToXr(translated);
                pose_dirty = true;
            }
        }
    }

    // --- Rotation (arrow keys + Q/E) ---
    // Arrow keys match mouse-look: yaw around world up and pitch around the yaw-aligned right axis.
    if (io.DeltaTime > 0.0f) {
        const float rot_rad = glm::radians(90.0f * io.DeltaTime);
        glm::quat orientation = sim_math::ToGlm(head_pose.orientation);

        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
            orientation = sim_math::RotateWorld(orientation, sim_math::kWorldUp, rot_rad);
            pose_dirty = true;
        }
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
            orientation = sim_math::RotateWorld(orientation, sim_math::kWorldUp, -rot_rad);
            pose_dirty = true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
            const float pitch_delta = ImGui::IsKeyDown(ImGuiKey_UpArrow) ? rot_rad : -rot_rad;
            const glm::vec3 pitch_axis = sim_math::HorizontalRightAxis(orientation);
            const glm::quat candidate = sim_math::RotateWorld(orientation, pitch_axis, pitch_delta);
            if (std::abs(sim_math::ForwardElevationDegrees(candidate)) <= sim_math::kPitchLimitDeg) {
                orientation = candidate;
                pose_dirty = true;
            }
        }

        if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_E)) {
            if (ImGui::IsKeyDown(ImGuiKey_Q)) {
                orientation = sim_math::RotateWorld(orientation, sim_math::Forward(orientation), -rot_rad);
                pose_dirty = true;
            }
            if (ImGui::IsKeyDown(ImGuiKey_E)) {
                orientation = sim_math::RotateWorld(orientation, sim_math::Forward(orientation), rot_rad);
                pose_dirty = true;
            }
        }

        head_pose.orientation = sim_math::ToXr(orientation);
    }

    if (pose_dirty) {
        ox_sim_set_device_pose("/user/head", &head_pose, is_active);
    }
}

bool GuiWindow::CopyPreviewPixelsToClipboard(const std::vector<uint8_t>& pixels, uint32_t width, uint32_t height) {
    if (pixels.size() != static_cast<size_t>(width) * height * 4) {
        status_message_ = "Preview copy failed: unexpected pixel buffer size";
        return false;
    }

    clip::image image(pixels.data(), make_clip_rgba_image_spec(width, height));
    if (!clip::set_image(image)) {
        status_message_ = "Preview copy failed: could not write image to clipboard";
        return false;
    }

    return true;
}

bool GuiWindow::CopyCurrentPreviewToClipboard() {
    if (preview_width_ == 0 || preview_height_ == 0) {
        status_message_ = "No eye texture available to copy";
        return false;
    }

    if (preview_eye_selection_ == 2) {
        const bool has_left = !preview_pixels_[0].empty();
        const bool has_right = !preview_pixels_[1].empty();
        if (!has_left && !has_right) {
            status_message_ = "No eye texture available to copy";
            return false;
        }

        std::vector<uint8_t> composite =
            ComposeSideBySidePreview(preview_pixels_[0], preview_pixels_[1], preview_width_, preview_height_);

        if (CopyPreviewPixelsToClipboard(composite, preview_width_ * 2, preview_height_)) {
            status_message_ = "Copied eye texture preview to clipboard";
            return true;
        }
        return false;
    }

    const int eye = preview_eye_selection_ == 1 ? 1 : 0;
    if (preview_pixels_[eye].empty()) {
        status_message_ = eye == 1 ? "No right eye texture available to copy" : "No left eye texture available to copy";
        return false;
    }

    if (CopyPreviewPixelsToClipboard(preview_pixels_[eye], preview_width_, preview_height_)) {
        status_message_ = "Copied eye texture preview to clipboard";
        return true;
    }

    return false;
}

void GuiWindow::HandlePreviewInteraction(const ImVec2& preview_min, const ImVec2& preview_max,
                                         const std::vector<PreviewImageRect>& image_rects, const ImVec2& composite_min,
                                         const ImVec2& composite_max, bool has_image) {
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mouse = io.MousePos;
    const bool preview_hovered =
        mouse.x >= preview_min.x && mouse.x <= preview_max.x && mouse.y >= preview_min.y && mouse.y <= preview_max.y;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        preview_has_focus_ = preview_hovered;
    }

    const bool hovering_image = UpdatePreviewHoverText(image_rects);

    bool copy_button_hovered = false;
    if (preview_hovered && has_image && !image_rects.empty()) {
        const float button_size = ImGui::GetFrameHeight();
        ImGui::SetCursorScreenPos(ImVec2(composite_max.x - button_size - 8.0f, composite_min.y + 8.0f));
        if (ImGui::Button(ICON_FA_COPY "##copy_preview_image", ImVec2(button_size, button_size))) {
            CopyCurrentPreviewToClipboard();
        }
        copy_button_hovered = ImGui::IsItemHovered();
        vog::widgets::ShowItemTooltip("Copy the current eye texture preview to the clipboard");
    }

    if (hovering_image && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !copy_button_hovered) {
        preview_drag_active_ = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        preview_drag_active_ = false;
    }

    if (preview_drag_active_ && !copy_button_hovered) {
        if (std::abs(io.MouseDelta.x) > 0.0f || std::abs(io.MouseDelta.y) > 0.0f) {
            XrPosef head_pose = {};
            XrBool32 is_active = XR_FALSE;
            if (GetHeadPose(head_pose, is_active)) {
                glm::quat orientation = sim_math::ToGlm(head_pose.orientation);
                orientation =
                    sim_math::RotateWorld(orientation, sim_math::kWorldUp, glm::radians(-io.MouseDelta.x * 0.25f));

                const float pitch_delta = glm::radians(-io.MouseDelta.y * 0.25f);
                if (pitch_delta != 0.0f) {
                    const glm::vec3 pitch_axis = sim_math::HorizontalRightAxis(orientation);
                    const glm::quat candidate = sim_math::RotateWorld(orientation, pitch_axis, pitch_delta);
                    if (std::abs(sim_math::ForwardElevationDegrees(candidate)) <= sim_math::kPitchLimitDeg) {
                        orientation = candidate;
                    }
                }

                head_pose.orientation = sim_math::ToXr(orientation);
                ox_sim_set_device_pose("/user/head", &head_pose, is_active);
            }
        }
    }

    HandlePreviewNavigation(preview_has_focus_, copy_button_hovered);
}

}  // namespace ox_sim
