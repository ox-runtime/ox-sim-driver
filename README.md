# ox-simulator

**WORK-IN-PROGRESS** - This is still a prototype and is not (yet) fully compliant with the OpenXR spec.

A fully open-source simulator for OpenXR. This can help test OpenXR apps with simulated VR devices (Quest, Vive, Index, Vive trackers, etc.).

You can either use the graphical interface, or use the API server to test programmatically in any language (by setting the device states and checking for the expected outcome).

Supports Windows, Linux and Mac. Backends: Vulkan, OpenGL, Metal.

<img height="350" alt="ox-sim dark on Windows" src="https://github.com/user-attachments/assets/dcad53f0-9339-4523-9e4d-32f39e4ae6be" />


## Purpose

This simulator allows developers to test OpenXR applications without physical hardware by simulating popular VR devices. It provides two interface modes for controlling the simulated devices:
- Web server interface - locally-hosted API for programmatic control and device switching
- GUI interface - windowed application for manual control (TBD)
## Building

1. Clone this repository using `git clone https://github.com/ox-runtime/ox-simulator`
2. Download [ox_driver.h](https://github.com/ox-runtime/ox/releases/latest/download/ox_driver.h) and place it inside this folder.
3. Compile using `cmake`:

```bash
cmake -B build
cmake --build build --config Release
```

4. Your driver will be built inside `build/ox_simulator`.

## Installation

Copy the driver (i.e. the `build/ox_simulator` folder) to the ox runtime's `drivers` folder:

For example:

```
ox-runtime/
├── bin/
│   ├── ox-service.exe (or ox-service on Linux/Mac)
│   └── drivers/
│       └── ox_simulator/
│           └── ox_driver.dll (or libox_driver.so / libox_driver.dylib)
│           └── config.json
```

## Configuration

Edit the `config.json` file in the deployed driver folder (`drivers/ox_simulator/config.json`):

```json
{
  "device": "oculus_quest_2",
  "mode": "api",
  "api_port": 8765,
  "comment": "Device options: oculus_quest_2, oculus_quest_3, htc_vive, valve_index, htc_vive_tracker. Mode options: api, gui"
}
```

**Configuration Options:**
- `device`: VR device to emulate (`oculus_quest_2`, `oculus_quest_3`, `htc_vive`, `valve_index`, `htc_vive_tracker`)
- `mode`: Interface mode (`api` for HTTP server, `gui` for graphical interface)
- `api_port`: Port for HTTP API server (default: 8765)

## Usage

### Starting the Simulator

The simulator starts automatically when the ox service is launched.

### HTTP API Reference

The simulator provides a REST API for controlling virtual devices:

#### Get Session Status
```bash
GET http://localhost:8765/v1/status
```

**Response:**
```json
{
  "session_state": "focused",
  "session_state_id": 5,
  "session_active": true,
  "fps": 72
}
```

**Response fields:**
- `session_state`: Human-readable session state (`unknown`, `idle`, `ready`, `synchronized`, `visible`, `focused`, `stopping`, `exiting`)
- `session_state_id`: Numeric session state ID (0-7)
- `session_active`: Boolean indicating if session is active (synchronized, visible, or focused)
- `fps`: Current application frame rate (0 if session not active)

#### Get Eye Textures
```bash
GET http://localhost:8765/v1/views/0  # Left eye
GET http://localhost:8765/v1/views/1  # Right eye
```

**Query Parameters:**
- `size` (optional): Target width for the returned image in pixels. Height is automatically calculated to maintain aspect ratio. Must be greater than 0. If not specified, returns the original full-resolution image.

**Examples:**
```bash
GET http://localhost:8765/v1/views/0?size=128  # Left eye, scaled to 128px width
GET http://localhost:8765/v1/views/1?size=512  # Right eye, scaled to 512px width
```

**Response:** PNG image data (Content-Type: `image/png`)

**Response codes:**
- `200`: PNG image data returned
- `404`: No frame available yet
- `503`: Frame data unavailable

#### Get Current Device Profile
```bash
GET http://localhost:8765/v1/profile
```

**Response:**
```json
{
  "type": "Meta Quest 2 (Simulated)",
  "manufacturer": "Meta Platforms",
  "interaction_profile": "/interaction_profiles/oculus/touch_controller",
  "devices": [
    {
      "user_path": "/user/head",
      "role": "hmd",
      "always_active": true,
      "components": []
    },
    {
      "user_path": "/user/hand/left",
      "role": "left_controller",
      "always_active": false,
      "components": [
        {
          "path": "/input/trigger/value",
          "type": "float",
          "description": "Trigger analog value"
        },
        {
          "path": "/input/trigger/touch",
          "type": "boolean",
          "description": "Trigger touch state"
        }
      ]
    }
  ]
}
```

#### Switch Device Profile
```bash
PUT http://localhost:8765/v1/profile
Content-Type: application/json

{
  "device": "htc_vive"
}
```

**Response:**
```json
{
  "status": "ok",
  "device": "HTC Vive (Simulated)",
  "interaction_profile": "/interaction_profiles/htc/vive_controller"
}
```

**Available devices:**
- `oculus_quest_2` - Meta Quest 2
- `oculus_quest_3` - Meta Quest 3
- `htc_vive` - HTC Vive
- `valve_index` - Valve Index
- `htc_vive_tracker` - HTC Vive Trackers (no HMD)

#### Get Device State
```bash
GET http://localhost:8765/v1/devices/user/hand/left
```

**Response:**
```json
{
  "active": true,
  "position": {"x": -0.2, "y": 1.4, "z": -0.3},
  "orientation": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0}
}
```

#### Set Device State
```bash
PUT http://localhost:8765/v1/devices/user/hand/left
Content-Type: application/json

{
  "position": {"x": -0.2, "y": 1.4, "z": -0.3},
  "orientation": {"x": 0, "y": 0, "z": 0, "w": 1},
  "active": true
}
```

**Parameters:**
- `position`: Object with x, y, z coordinates
- `orientation`: Object with x, y, z, w quaternion components
- `active`: Boolean indicating if device is active (optional, default: true)

#### Get Input Component State
Look at the output of `GET /v1/profile` to find the possible input path values.

```bash
GET http://localhost:8765/v1/inputs/user/hand/left/input/trigger/value
```

**Response:**
```json
{
  "boolean_value": false,
  "float_value": 0.8,
  "x": 0.0,
  "y": 0.0
}
```

#### Set Input Component State
```bash
PUT http://localhost:8765/v1/inputs/user/hand/left/input/trigger/value
Content-Type: application/json

{
  "value": 0.8
}
```

**Parameters:**
- `value`: Numeric value (0.0 to 1.0) or boolean

## API Usage Examples

### Using cURL

**Get session status:**
```bash
curl http://localhost:8765/v1/status
```

**Download left eye texture:**
```bash
curl -o left_eye.png http://localhost:8765/v1/views/0
```

**Download right eye texture (scaled to 256px width):**
```bash
curl -o right_eye.png "http://localhost:8765/v1/views/1?size=256"
```

**Get current device profile:**
```bash
curl http://localhost:8765/v1/profile
```

**Switch to HTC Vive:**
```bash
curl -X PUT http://localhost:8765/v1/profile -H "Content-Type: application/json" -d '{"device": "htc_vive"}'
```

**Switch to Vive trackers:**
```bash
curl -X PUT http://localhost:8765/v1/profile -H "Content-Type: application/json" -d '{"device": "htc_vive_tracker"}'
```

**Get left controller state:**
```bash
curl http://localhost:8765/v1/devices/user/hand/left
```

**Position left controller:**
```bash
curl -X PUT http://localhost:8765/v1/devices/user/hand/left -H "Content-Type: application/json" -d '{
  "position": {"x": -0.2, "y": 1.4, "z": -0.3},
  "orientation": {"x": 0, "y": 0, "z": 0, "w": 1},
  "active": true
}'
```

**Get trigger value:**
```bash
curl http://localhost:8765/v1/inputs/user/hand/left/input/trigger/value
```

**Press the left trigger:**
```bash
curl -X PUT http://localhost:8765/v1/inputs/user/hand/left/input/trigger/value -H "Content-Type: application/json" -d '{
  "value": 1.0
}'
```

**Position a Vive tracker on waist:**
```bash
curl -X PUT http://localhost:8765/v1/devices/user/vive_tracker_htcx/role/waist -H "Content-Type: application/json" -d '{
  "position": {"x": 0, "y": 1.0, "z": 0},
  "orientation": {"x": 0, "y": 0, "z": 0, "w": 1},
  "active": true
}'
```

### Using Python

```python
import requests
import json

BASE_URL = "http://localhost:8765"

# Get session status
response = requests.get(f"{BASE_URL}/v1/status")
if response.status_code == 200:
    status = response.json()
    print(f"Session state: {status['session_state']} ({status['fps']} FPS)")
    print(f"Session active: {status['session_active']}")

# Download eye textures
response = requests.get(f"{BASE_URL}/v1/views/0")  # Left eye (full resolution)
if response.status_code == 200:
    with open("left_eye.png", "wb") as f:
        f.write(response.content)
    print("Downloaded left eye texture")

response = requests.get(f"{BASE_URL}/v1/views/1?size=256")  # Right eye (scaled to 256px width)
if response.status_code == 200:
    with open("right_eye.png", "wb") as f:
        f.write(response.content)
    print("Downloaded right eye texture (scaled)")

# Get current device profile
response = requests.get(f"{BASE_URL}/v1/profile")
if response.status_code == 200:
    profile = response.json()
    print(f"Current device: {profile['type']}")
    print(f"Interaction profile: {profile['interaction_profile']}")

# Switch to HTC Vive
switch_request = {"device": "htc_vive"}
response = requests.put(f"{BASE_URL}/v1/profile", json=switch_request)
if response.status_code == 200:
    result = response.json()
    print(f"Switched to: {result['device']}")

# Switch to Vive trackers
switch_request = {"device": "htc_vive_tracker"}
response = requests.put(f"{BASE_URL}/v1/profile", json=switch_request)
if response.status_code == 200:
    result = response.json()
    print(f"Switched to: {result['device']}")

# Get current left controller state
response = requests.get(f"{BASE_URL}/v1/devices/user/hand/left")
if response.status_code == 200:
    left_state = response.json()
    print(f"Left controller state: {left_state}")

# Activate and position left controller
left_controller = {
    "position": {"x": -0.2, "y": 1.4, "z": -0.3},
    "orientation": {"x": 0, "y": 0, "z": 0, "w": 1},
    "active": True
}
response = requests.put(f"{BASE_URL}/v1/devices/user/hand/left", json=left_controller)
print(f"Set left controller: {response.status_code}")

# Get current trigger value
response = requests.get(f"{BASE_URL}/v1/inputs/user/hand/left/input/trigger/value")
if response.status_code == 200:
    trigger_state = response.json()
    print(f"Trigger value: {trigger_state['float_value']}")

# Press trigger on left controller
trigger_input = {"value": 0.8}
response = requests.put(f"{BASE_URL}/v1/inputs/user/hand/left/input/trigger/value", json=trigger_input)
print(f"Set trigger: {response.status_code}")

# Press A button
button_input = {"value": True}
response = requests.put(f"{BASE_URL}/v1/inputs/user/hand/left/input/a/click", json=button_input)
print(f"Press A button: {response.status_code}")

# Add Vive tracker on waist
tracker_pose = {
    "position": {"x": 0, "y": 1.0, "z": 0},
    "orientation": {"x": 0, "y": 0, "z": 0, "w": 1},
    "active": True
}
response = requests.put(f"{BASE_URL}/v1/devices/user/vive_tracker_htcx/role/waist", json=tracker_pose)
print(f"Set waist tracker: {response.status_code}")
```

## Testing

After deploying and configuring the simulator, you can test it with OpenXR applications:

1. Start the ox service (which loads the simulator driver)
2. Set the OpenXR runtime environment variable:
   - **Windows:** `$env:XR_RUNTIME_JSON = "C:\path\to\ox\build\win-x64\bin\ox_openxr.json"`
   - **Linux:** `export XR_RUNTIME_JSON="/path/to/ox/build/linux-x64/bin/ox_openxr.json"`
3. Control the simulator via HTTP API
4. Run your OpenXR application

## Troubleshooting

**Port already in use:**
- Change the `api_port` in `config.json`
- Check for other processes using the port: `netstat -ano | findstr :8765` (Windows) or `lsof -i :8765` (Linux)

**Driver not loading:**
- Verify the driver folder is in the `drivers` inside ox runtime.
- Check that `ox-service` is running
- Ensure `config.json` exists and is valid JSON

**API not responding:**
- Confirm `mode` is set to `"api"` in `config.json`
- Check `ox-service` console output for startup messages
- Test the root endpoint: `curl http://localhost:8765/`

**Device switching not working:**
- Ensure the device name is spelled correctly (case-sensitive)
- Check that the device profile exists by calling `GET /v1/profile` first
- Restart ox-service if device switching fails
- Some input components may not be available on certain devices (e.g., A/B buttons on Vive controllers)
