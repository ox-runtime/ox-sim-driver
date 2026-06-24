# ox_sim

A minimal, Pythonic wrapper around the `ox_sim` C API for controlling the ox simulator.

The ox runtime will run inside the running Blender process, avoiding cross-process communication (i.e. no IPC/HTTP).

## Requirements

- Python 3.8+
- [ox runtime](https://github.com/ox-runtime/ox) installed.
- ox runtime set as the active OpenXR runtime (or use the `XR_RUNTIME_JSON` environment variable).

## Setup

Set the `OX_USE_SIMULATOR` environment variable:
- Linux/macOS: `export OX_USE_SIMULATOR=1`
- Windows Powershell: `$env:OX_USE_SIMULATOR="1"`
- Windows Command (DOS): `set OX_USE_SIMULATOR=1`

## Quick Example

```python
import time
from ox_sim import Simulator

sim = Simulator()

sim.profile = "oculus_quest_3"

left = sim.device("/user/hand/left")

# Press trigger
left.set_input("/input/trigger/value", 1.0)
time.sleep(0.1)

# Move controller
left.position = (0.1, 0.0, -0.5)

# Release trigger
left.set_input("/input/trigger/value", 0.0)

# Wait a bit
time.sleep(0.5)

# Read a frame
pixels, w, h = sim.views()[0].image()

x, y = 50, 60
i = (y * w + x) * 4
print(pixels[i:i+4])  # RGBA

sim.shutdown()
```

---

# API Reference

## Simulator

```python
sim = Simulator()
```

| Member | Returns | Notes |
| --- | --- | --- |
| `shutdown()` | `None` | Releases simulator resources. |
| `status` | `dict` | `{"state": int, "active": bool, "fps": int}`. |
| `profile` | `str` | Gets or sets the active profile ID, for example `"oculus_quest_3"`. |
| `profile_info` | `dict` | `{"name": str, "manufacturer": str, "interaction_profile": str}`. |
| `devices()` | `list[Device]` | Enumerates devices in the active profile. |
| `device(user_path)` | `Device` | Looks up a device by path such as `"/user/hand/left"`. Raises `OxSimDeviceNotFoundError` if missing. |
| `views()` | `list[View]` | Returns one `View` per eye. |

## Device

Represents a simulator device such as `"/user/hand/left"` or `"/user/head"`.

| Member | Returns | Notes |
| --- | --- | --- |
| `user_path` | `str` | Static profile path. |
| `role` | `str` | Static profile role such as `"left_controller"`. |
| `always_active` | `bool` | `True` for devices that are always considered active. |
| `position` | `tuple[float, float, float]` | Getter returns `(x, y, z)`; setter updates simulator pose. |
| `orientation` | `tuple[float, float, float, float]` | Getter returns quaternion `(x, y, z, w)`; setter updates simulator pose. |
| `active` | `bool` | Gets or sets whether the device is active. |
| `set_input(path, value)` | `None` | Accepts `bool`, `float`, or `(x, y)` for vec2 inputs. Validates the path against profile metadata before dispatching to the matching C API. |
| `get_input(path)` | `bool | float | tuple[float, float]` | Returns the current input value using the component's declared type. |

`set_input()` and `get_input()` raise `OxSimComponentNotFoundError` when `path` is not valid for that device.

## View

| Member | Returns | Notes |
| --- | --- | --- |
| `info()` | `OxSimViewInfo` | Struct with `data_size`, `width`, `height`, and `frame_time`. |
| `image()` | `(bytes, int, int)` | Returns RGBA8 pixels, width, and height. Pixels are row-major with 4 bytes per pixel. |

## Exceptions

All simulator API failures raise `OxSimError` or a subclass.

| Exception | Meaning |
| --- | --- |
| `OxSimInvalidArgumentError` | Invalid argument passed to the C API. |
| `OxSimNotInitializedError` | The simulator is not initialized, or required runtime state is not ready yet. |
| `OxSimBufferTooSmallError` | A caller-provided output buffer was too small. |
| `OxSimProfileNotFoundError` | The requested profile ID does not exist. |
| `OxSimDeviceNotFoundError` | The requested device path does not exist. |
| `OxSimComponentNotFoundError` | The requested component path does not exist for the device. |
| `OxSimError` | Base class for all wrapper errors, including unknown future error codes. |
