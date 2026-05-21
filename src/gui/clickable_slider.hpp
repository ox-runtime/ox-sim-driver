#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

#include "vog.h"

namespace ox_sim {

class ClickableSlider {
   public:
    static constexpr float kPressedValue = 1.0f;
    static constexpr float kReleasedValue = 0.0f;
    static constexpr float kPressDurationSeconds = 0.3f;
    static constexpr float kReleaseDurationSeconds = 0.1f;
    static constexpr float kEpsilon = 0.0001f;

    bool SliderFloat(const char* label, float* value, float v_min, float v_max, const char* format = "%.2f") {
        if (!label || !value) {
            return false;
        }

        const uint32_t id = ImGui::GetID(label);
        const float button_side = ImGui::GetFrameHeight();
        const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        const float slider_width = std::max(150.0f - button_side - button_spacing, 60.0f);

        ImGui::SetNextItemWidth(slider_width);
        const bool slider_changed = ImGui::SliderFloat(label, value, v_min, v_max, format);
        if (slider_changed) {
            StopAnimation(id);
            *value = std::clamp(*value, v_min, v_max);
        }

        ImGui::SameLine(0.0f, button_spacing);
        ImGui::Button(ICON_FA_CIRCLE "##click", ImVec2(button_side, button_side));
        const bool button_down = ImGui::IsItemActive();
        vog::widgets::ShowItemTooltip("Ramp to 1 while held, then release back to 0");

        const bool animation_changed = UpdateAnimation(id, value, button_down, ImGui::GetIO().DeltaTime);
        return slider_changed || animation_changed;
    }

    bool UpdateAnimation(uint32_t widget_id, float* value, bool button_down, float delta_time) {
        if (!value) {
            return false;
        }

        return AdvanceState(states_[widget_id], value, button_down, delta_time);
    }

    void StopAnimation(uint32_t widget_id) {
        auto it = states_.find(widget_id);
        if (it == states_.end()) {
            return;
        }

        it->second.animating = false;
        it->second.button_down = false;
        it->second.transition_elapsed = 0.0f;
        it->second.transition_start_value = kReleasedValue;
        it->second.target_value = kReleasedValue;
    }

   private:
    struct AnimationState {
        float transition_start_value = kReleasedValue;
        float transition_elapsed = 0.0f;
        float target_value = kReleasedValue;
        bool animating = false;
        bool button_down = false;
    };

    bool AdvanceState(AnimationState& state, float* value, bool button_down, float delta_time) {
        if (button_down != state.button_down) {
            state.button_down = button_down;
            state.transition_start_value = *value;
            state.target_value = button_down ? kPressedValue : kReleasedValue;
            state.transition_elapsed = 0.0f;
            state.animating = std::fabs(state.transition_start_value - state.target_value) > kEpsilon;
        }

        if (!state.animating) {
            return false;
        }

        const float duration = state.button_down ? kPressDurationSeconds : kReleaseDurationSeconds;
        const float step = std::max(delta_time, 0.0f);
        state.transition_elapsed = std::min(state.transition_elapsed + step, duration);

        const float progress =
            duration <= kEpsilon ? 1.0f : std::clamp(state.transition_elapsed / duration, 0.0f, 1.0f);
        const float next_value =
            state.transition_start_value + (state.target_value - state.transition_start_value) * progress;
        const bool changed = std::fabs(next_value - *value) > kEpsilon;
        *value = next_value;

        if (progress >= 1.0f - kEpsilon) {
            *value = state.target_value;
            state.animating = false;
        }

        return changed;
    }

    std::unordered_map<uint32_t, AnimationState> states_;
};

}  // namespace ox_sim