#pragma once

#include <atomic>

#include "device_profiles.h"
#include "frame_data.h"
#include "simulator_core.h"

namespace ox_sim {

bool AcquireSimulatorRuntime();
void ReleaseSimulatorRuntime();
bool StartSimulatorGui();
void StopSimulatorGui();
bool IsSimulatorRuntimeInitialized();
bool IsSimulatorGuiRunning();

SimulatorCore& GetSimulatorCore();
FrameData* GetFrameData();

const DeviceProfile* GetCurrentDeviceProfile();
std::atomic<const DeviceProfile*>* GetCurrentDeviceProfileSlot();
bool SetCurrentDeviceProfile(const DeviceProfile* profile);

}  // namespace ox_sim