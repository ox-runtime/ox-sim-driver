# ox-simulator

`ox-simulator` is the reference virtual device driver for the `ox` OpenXR stack. It exports two interfaces from the same shared library:

- the low-level `ox_driver_register` ABI consumed by `ox-runtime`
- a separate high-level C automation API declared in `src/simulator_api.h`

The GUI still talks directly to `SimulatorCore`. There is no HTTP server and no config file.

## Build

1. Copy the current `ox_driver.h` into the repository root.
2. (Linux-only) Install platform dependencies:

```bash
# Ubuntu / Debian
sudo apt-get update
sudo apt-get install -y \
  libgl1-mesa-dev \
  libx11-dev \
  libxrandr-dev \
  libxinerama-dev \
  libxcursor-dev \
  libxi-dev \
  pkg-config
```
3. Configure and build:

```bash
cmake -B build
cmake --build build --config Release
```

Outputs:

- Windows: `build/ox_simulator/ox_driver.dll`
- Linux: `build/ox_simulator/libox_driver.so`
- macOS: `build/ox_simulator/libox_driver.dylib`

If `OX_SIM_BUILD_EXAMPLES` is enabled, the C API example is also built under `build/examples`.

## Runtime Behavior

- The simulator defaults to the `oculus_quest_2` profile on first initialization.
- When loaded as a driver, it starts the simulator GUI automatically.
- When used through the C API alone, it does not force the GUI to open.
- The active profile can be changed at runtime through either the GUI or the C API.

## C API

The public C API lives in `src/simulator_api.h`.

Main entry points:

- `ox_sim_create_context` / `ox_sim_destroy_context`
- `ox_sim_initialize` / `ox_sim_shutdown`
- `ox_sim_get_current_profile` / `ox_sim_set_current_profile`
- `ox_sim_get_device_pose` / `ox_sim_set_device_pose`
- `ox_sim_get_input_state_boolean` / `ox_sim_set_input_state_boolean`
- `ox_sim_get_input_state_float` / `ox_sim_set_input_state_float`
- `ox_sim_get_input_state_vector2f` / `ox_sim_set_input_state_vector2f`
- `ox_sim_get_session_state`
- `ox_sim_get_app_fps`
- `ox_sim_get_frame_preview`

Examples:

- `examples/simulator_api_example.cpp` shows native usage
- `examples/simulator_control_example.py` shows Python `ctypes` usage

## Installation

Copy the built `ox_simulator` output folder into the wrapper/runtime `drivers` directory. Example:

```text
ox/
  build/
    win-x64/
      bin/
        drivers/
          ox-simulator/
            ox_driver.dll
```

## Notes

- The simulator C API is intended for bindings and automation.
- `SimulatorCore` handles shared device/input state locking between the GUI and the automation API.
- The small internal runtime layer only coordinates process-global lifetime, the current profile pointer, frame preview storage, and GUI startup.
