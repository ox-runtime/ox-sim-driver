#include "gui_window.h"

#include <GLFW/glfw3.h>
#include <ox_sim.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "device_profiles.hpp"
#include "imgui_impl_opengl3.h"
#include "utils.hpp"
#include "vog.h"

using ox_sim::utils::DegToRad;
using ox_sim::utils::RadToDeg;

namespace ox_sim {

namespace {

const DeviceProfile* current_profile() {
    char profile_name[64] = {};
    if (ox_sim_get_current_profile(profile_name, sizeof(profile_name)) != OX_SIM_SUCCESS) {
        return nullptr;
    }

    return GetDeviceProfileByName(profile_name);
}

bool is_session_active() {
    OxSessionState state = OX_SESSION_STATE_UNKNOWN;
    if (ox_sim_get_session_state(&state) != OX_SIM_SUCCESS) {
        return false;
    }

    return state == OX_SESSION_STATE_SYNCHRONIZED || state == OX_SESSION_STATE_VISIBLE ||
           state == OX_SESSION_STATE_FOCUSED;
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
    vog::WindowConfig cfg{"ox simulator", 1280, 720};

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

    ImGuiIO& io = ImGui::GetIO();

    ImVec2 content_size = ImGui::GetContentRegionAvail();
    const ImGuiStyle& style = ImGui::GetStyle();

    // ========== TOP TOOLBAR STRIP ==========
    const float top_toolbar_h = 48.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, tc.bg.value());
    ImGui::BeginChild("TopToolbar", ImVec2(0, top_toolbar_h), false, ImGuiWindowFlags_NoScrollbar);
    {
        const float btn_runtime_w = 190.0f;
        const float lbl_device_w = ImGui::CalcTextSize("Device:").x + style.ItemSpacing.x;
        const float combo_device_w = 190.0f;
        const float spacing = style.ItemSpacing.x * 3.0f;
        const float total_w = btn_runtime_w + spacing + lbl_device_w + combo_device_w;
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
            ImGui::Text("Display: %dx%d @ %u fps  |  %s", p->display_width, p->display_height, app_fps,
                        status_message_.c_str());
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
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();

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
            ImGui::SetCursorPos(ImVec2(left_x, y_off));
            if (preview_textures_[0]) {
                ImGui::Image((ImTextureID)(intptr_t)preview_textures_[0], ImVec2(w_each, h_each), ImVec2(0, 1),
                             ImVec2(1, 0), ImVec4(1, 1, 1, 1), tc.border.value());
            } else {
                ImGui::Dummy(ImVec2(w_each, h_each));
            }
            ImGui::SetCursorPos(ImVec2(right_x, y_off));
            if (preview_textures_[1]) {
                ImGui::Image((ImTextureID)(intptr_t)preview_textures_[1], ImVec2(w_each, h_each), ImVec2(0, 1),
                             ImVec2(1, 0), ImVec4(1, 1, 1, 1), tc.border.value());
            } else {
                ImGui::Dummy(ImVec2(w_each, h_each));
            }
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
                ImGui::SetCursorPos(ImVec2(x_off, y_off));
                ImGui::Image((ImTextureID)(intptr_t)preview_textures_[eye], ImVec2(img_w, img_h), ImVec2(0, 1),
                             ImVec2(1, 0), ImVec4(1, 1, 1, 1), tc.border.value());
            } else {
                ImVec2 ts = ImGui::CalcTextSize(no_msg);
                ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
                ImGui::Text("%s", no_msg);
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void GuiWindow::UpdateFrameTextures() {
    OxSimFramePreview frame_preview = {};
    if (ox_sim_get_frame_preview(&frame_preview) != OX_SIM_SUCCESS || !frame_preview.has_new_frame) return;

    uint32_t w = frame_preview.width;
    uint32_t h = frame_preview.height;
    if (w == 0 || h == 0) return;

    for (int eye = 0; eye < 2; ++eye) {
        if (!frame_preview.pixel_data[eye]) continue;
        size_t expected_size = w * h * 4;
        if (frame_preview.data_size[eye] != expected_size) continue;

        if (!preview_textures_[eye]) {
            glGenTextures(1, &preview_textures_[eye]);
            glBindTexture(GL_TEXTURE_2D, preview_textures_[eye]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            spdlog::info("[GUI] Created OpenGL texture {} for eye {}", preview_textures_[eye], eye);
        }
        glBindTexture(GL_TEXTURE_2D, preview_textures_[eye]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame_preview.pixel_data[eye]);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    preview_width_ = w;
    preview_height_ = h;
    preview_textures_valid_ = true;
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

    uint32_t is_active = false;
    OxPose pose = {{0, 0, 0}, {0, 0, 0, 1}};
    ox_sim_get_device_pose(device.user_path, &pose, &is_active);

    if (!device.always_active) {
        bool active_toggle = is_active;
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Active");
        ImGui::SameLine();
        if (vog::widgets::ToggleButton("", &active_toggle)) {
            ox_sim_set_device_pose(device.user_path, &pose, active_toggle ? 1u : 0u);
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
        OxPose default_pose = device.default_pose;
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
            OxVector2f value = {0.0f, 0.0f};
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
void GuiWindow::RenderRotationControl(const DeviceDef& device, int device_index, OxPose& pose, bool is_active) {
    const std::string key = std::string(device.user_path) + "_" + std::to_string(device_index);
    auto it = euler_cache_.find(key);
    if (it == euler_cache_.end()) {
        EulerCache ec;
        ec.quat = pose.orientation;
        QuatToEuler(pose.orientation, ec.euler);
        euler_cache_[key] = ec;
    }
    auto& ec = euler_cache_[key];

    // If the quaternion changed externally (Reset Pose / API), re-derive Euler.
    if (ec.quat.x != pose.orientation.x || ec.quat.y != pose.orientation.y || ec.quat.z != pose.orientation.z ||
        ec.quat.w != pose.orientation.w) {
        QuatToEuler(pose.orientation, ec.euler);
        ec.quat = pose.orientation;
    }

    // OpenXR right-handed Y-up: euler.x=pitch, euler.y=yaw, euler.z=roll — no shuffling needed.
    float rot[3] = {ec.euler.x, ec.euler.y, ec.euler.z};
    const float pad = 8.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - pad);
    if (ImGui::DragFloat3("##Rotation", rot, 1.0f, -FLT_MAX, FLT_MAX, "%.2f°")) {
        // Apply each axis delta as an incremental world-space rotation so
        // the three axes stay independent (no gimbal-lock singularity).
        float dp = DegToRad(rot[0] - ec.euler.x);  // pitch delta (rot around X)
        float dy = DegToRad(rot[1] - ec.euler.y);  // yaw delta   (rot around Y)
        float dr = DegToRad(rot[2] - ec.euler.z);  // roll delta  (rot around Z)

        OxQuaternion q = pose.orientation;
        ApplyRotation(q, OxVector3f{1, 0, 0}, dp);
        ApplyRotation(q, OxVector3f{0, 1, 0}, dy);
        ApplyRotation(q, OxVector3f{0, 0, 1}, dr);
        pose.orientation = q;

        ec.euler = {rot[0], rot[1], rot[2]};
        ec.quat = q;
        ox_sim_set_device_pose(device.user_path, &pose, is_active);
    }
}

// Convert quaternion to Euler angles (in degrees) using the OpenXR right-handed Y-up convention.
// euler.x = pitch (rotation around X), euler.y = yaw (rotation around Y), euler.z = roll (rotation around Z).
void GuiWindow::QuatToEuler(const OxQuaternion& q, OxVector3f& euler) {
    // OpenXR right-handed Y-up: pitch=rotate-X, yaw=rotate-Y, roll=rotate-Z
    float sinp_cosy = 2.0f * (q.w * q.x + q.y * q.z);
    float cosp_cosy = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    float siny = 2.0f * (q.w * q.y - q.z * q.x);
    float sinr_cosy = 2.0f * (q.w * q.z + q.x * q.y);
    float cosr_cosy = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    euler.x = RadToDeg(std::atan2(sinp_cosy, cosp_cosy));  // pitch (rot around X)
    euler.y =
        (std::abs(siny) >= 1.0f ? std::copysign(90.0f, siny) : RadToDeg(std::asin(siny)));  // yaw   (rot around Y)
    euler.z = RadToDeg(std::atan2(sinr_cosy, cosr_cosy));                                   // roll  (rot around Z)
}

// Apply an incremental rotation (angle in radians) around the given axis to the input quaternion.
void GuiWindow::ApplyRotation(OxQuaternion& q, const OxVector3f& a, float angle) {
    if (angle == 0.0f) return;
    float s = std::sin(angle * 0.5f), c = std::cos(angle * 0.5f);
    float nx = c * q.x + s * (a.x * q.w + a.y * q.z - a.z * q.y);
    float ny = c * q.y + s * (a.y * q.w + a.z * q.x - a.x * q.z);
    float nz = c * q.z + s * (a.z * q.w + a.x * q.y - a.y * q.x);
    float nw = c * q.w - s * (a.x * q.x + a.y * q.y + a.z * q.z);
    float len = std::sqrt(nx * nx + ny * ny + nz * nz + nw * nw);
    q.x = nx / len;
    q.y = ny / len;
    q.z = nz / len;
    q.w = nw / len;
}

}  // namespace ox_sim
