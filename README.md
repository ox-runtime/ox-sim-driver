# ox-simulator

`ox-simulator` is the reference virtual device driver for the `ox` OpenXR stack. It exports two interfaces from the same shared library:

- the low-level `ox_driver_register` ABI consumed by `ox-runtime`
- a separate high-level C automation API declared in `include/ox_sim.h`

Both the GUI and the local REST server talk to the same internal C automation API. When the driver is loaded by `ox-runtime`, it starts the GUI and the HTTP API server together. The GUI can turn the HTTP API on or off at runtime.

## Build

1. (Linux-only) Install platform dependencies:

```bash
# Ubuntu / Debian
sudo apt-get update
sudo apt-get install -y \
  libgl1-mesa-dev \
  libxcb-dev \
  libx11-dev \
  libxrandr-dev \
  libxinerama-dev \
  libxcursor-dev \
  libxi-dev \
  libpng-dev \
  pkg-config
```
2. Configure and build:

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
- When loaded as a driver, it starts the simulator GUI and the HTTP API server automatically.
- When used through the C API alone, it does not start the GUI and the HTTP API server automatically.
- The active profile can be changed at runtime through the GUI, the REST API, or the C API.
- The eye preview supports cursor coordinate readout, FPS-style drag-to-look navigation, yaw-relative `WASD` movement with world-up `R/F`, arrow-key pitch/yaw that ignores roll, and copying the current preview image to the clipboard from the preview overlay button.

## Documentation

- GUI reference: https://github.com/ox-runtime/ox/blob/main/docs/gui.md
- HTTP API reference: https://github.com/ox-runtime/ox/blob/main/docs/rest_api.md
- C API reference: https://github.com/ox-runtime/ox/blob/main/docs/c_api.md
- Public header: `include/ox_sim.h`

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
- The REST server uses the same `ox_sim_*` API surface as the GUI.
- The small internal runtime layer only coordinates process-global lifetime, the current profile pointer, frame preview storage, GUI startup, and HTTP server startup.
