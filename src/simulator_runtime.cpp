#include "simulator_runtime.h"

#include <spdlog/spdlog.h>

#include <mutex>

#include "gui_window.h"

namespace ox_sim {

namespace {

SimulatorCore g_simulator;
GuiWindow g_gui_window;
FrameData g_frame_data;
std::atomic<const DeviceProfile*> g_device_profile{nullptr};
std::mutex g_runtime_mutex;
uint32_t g_runtime_ref_count = 0;

const DeviceProfile* DefaultProfile() { return &GetDeviceProfile(DeviceType::OCULUS_QUEST_2); }

void ResetFrameData() {
    std::lock_guard<std::mutex> lock(g_frame_data.mutex);
    g_frame_data.pixel_data[0] = nullptr;
    g_frame_data.pixel_data[1] = nullptr;
    g_frame_data.data_size[0] = 0;
    g_frame_data.data_size[1] = 0;
    g_frame_data.width = 0;
    g_frame_data.height = 0;
    g_frame_data.has_new_frame.store(false, std::memory_order_relaxed);
    g_frame_data.session_state.store(OX_SESSION_STATE_UNKNOWN, std::memory_order_relaxed);
    g_frame_data.app_fps.store(0, std::memory_order_relaxed);
    g_frame_data.last_frame_time_ms = 0;
    g_frame_data.dt_history.clear();
}

}  // namespace

bool AcquireSimulatorRuntime() {
    std::lock_guard<std::mutex> lock(g_runtime_mutex);
    if (g_runtime_ref_count > 0) {
        ++g_runtime_ref_count;
        return true;
    }

    const DeviceProfile* profile = g_device_profile.load(std::memory_order_acquire);
    if (!profile) {
        profile = DefaultProfile();
        g_device_profile.store(profile, std::memory_order_release);
    }

    spdlog::info("Simulating device: {}", profile->name);
    if (!g_simulator.Initialize(profile)) {
        return false;
    }

    ResetFrameData();
    g_runtime_ref_count = 1;
    return true;
}

void ReleaseSimulatorRuntime() {
    bool should_shutdown = false;
    {
        std::lock_guard<std::mutex> lock(g_runtime_mutex);
        if (g_runtime_ref_count == 0) {
            return;
        }

        --g_runtime_ref_count;
        should_shutdown = g_runtime_ref_count == 0;
    }

    if (!should_shutdown) {
        return;
    }

    g_simulator.Shutdown();
    ResetFrameData();
}

bool StartSimulatorGui() {
    if (g_gui_window.IsRunning()) {
        return true;
    }

    return g_gui_window.Start(&g_simulator, &g_device_profile);
}

void StopSimulatorGui() { g_gui_window.Stop(); }

bool IsSimulatorRuntimeInitialized() {
    std::lock_guard<std::mutex> lock(g_runtime_mutex);
    return g_runtime_ref_count > 0;
}

SimulatorCore& GetSimulatorCore() { return g_simulator; }

FrameData* GetFrameData() { return &g_frame_data; }

const DeviceProfile* GetCurrentDeviceProfile() { return g_device_profile.load(std::memory_order_acquire); }

std::atomic<const DeviceProfile*>* GetCurrentDeviceProfileSlot() { return &g_device_profile; }

bool SetCurrentDeviceProfile(const DeviceProfile* profile) {
    if (!profile) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_runtime_mutex);
    if (g_runtime_ref_count > 0 && !g_simulator.SwitchDevice(profile)) {
        return false;
    }

    g_device_profile.store(profile, std::memory_order_release);
    return true;
}

}  // namespace ox_sim