"""
Run this script using `blender --python example_test_blender.py`

Make sure you set the following environment variables before running Blender:
- OX_USE_SIMULATOR=1
- OX_PATH="/path/to/ox"
- XR_RUNTIME_JSON="/path/to/ox/ox_runtime.json"
"""

import bpy
import time

# set up path to include the wrapper
import sys

sys.path.append("C:/Users/sshekhar/projects/ox/ox-sim-driver/wrappers/python")

from ox_sim import Simulator


STARTUP_TIMEOUT_SECONDS = 10.0
SETTLE_SECONDS = 0.5
DRAW_STEPS = 20


def start_xr():
    screen = bpy.context.screen
    area = next((a for a in screen.areas if a.type == "VIEW_3D"), None)

    bpy.context.window_manager.xr_session_settings.base_scale = 10

    with bpy.context.temp_override(area=area):
        bpy.ops.wm.xr_session_toggle()


def is_xr_running():
    state = bpy.context.window_manager.xr_session_state
    return state is not None and state.is_running(bpy.context)


def verify_result(before):
    bpy.context.view_layer.update()

    after = set(bpy.data.objects)
    new_objects = list(after - before)

    if len(new_objects) == 0:
        raise RuntimeError("No new object created")

    if not any(obj.type == "CURVE" for obj in new_objects):
        raise RuntimeError("No CURVE object created")


async def simulate_draw(sim):
    left = sim.device("/user/hand/left")
    base = (0.0, 0.0, -0.5)

    left.position = base
    left.orientation = (0, 0, 0, 1)
    left.active = True
    left.set_input("/input/trigger/value", 1.0)
    left.set_input("/input/trigger/touch", True)

    for i in range(DRAW_STEPS):
        x = base[0] + i * 0.01
        left.position = (x, base[1], base[2])
        await sim.wait_frame()

    left.set_input("/input/trigger/value", 0.0)
    left.set_input("/input/trigger/touch", False)
    await sim.wait_frame()


def pump_redraws_until_done(future, timeout_seconds):
    deadline = time.monotonic() + timeout_seconds

    while not future.done():
        if not is_xr_running() and time.monotonic() > deadline:
            raise TimeoutError("Timed out waiting for Blender XR session to start")

        if time.monotonic() > deadline:
            raise TimeoutError("Timed out waiting for Blender XR frames")

        bpy.ops.wm.redraw_timer(type="DRAW_WIN_SWAP", iterations=1)

    return future.result()


def pump_redraws_for(seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        bpy.ops.wm.redraw_timer(type="DRAW_WIN_SWAP", iterations=1)


def test_draw_creates_curve():
    sim = Simulator()
    sim.profile = "oculus_quest_3"

    before = set(bpy.data.objects)

    try:
        start_xr()
        future = sim.run_async(simulate_draw(sim))
        pump_redraws_until_done(future, STARTUP_TIMEOUT_SECONDS)
        pump_redraws_for(SETTLE_SECONDS)
        verify_result(before)
        print("[PASS] Curve object created")
    except Exception as exc:
        print(f"[FAIL] {exc}")
    finally:
        sim.shutdown()


if __name__ == "__main__":
    test_draw_creates_curve()
