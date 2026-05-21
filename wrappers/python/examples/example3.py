"""
Run this using:

set OX_USE_SIMULATOR=1
set OX_PATH=C:\path\to\ox

blender --python example3.py

"""

import bpy
from mathutils import Vector
import sys

sys.path.append("C:/Users/sshekhar/projects/ox/ox-sim-driver/wrappers/python")

from ox_sim import Simulator

sim, left_controller = None, None
next_state = "SET"  # or 'TEST', or 'DONE'
prev_val = None


def init_sim():
    global sim, left_controller

    sim = Simulator()

    # Switch profile
    sim.profile = "oculus_quest_2"
    print("Profile:", sim.profile_info)

    # Get left controller
    left_controller = sim.device("/user/hand/left")


def start_xr():
    area = next((a for a in bpy.context.screen.areas if a.type == "VIEW_3D"), None)

    with bpy.context.temp_override(area=area):
        bpy.ops.wm.xr_session_toggle()


def do_timer():
    global next_state, prev_val

    session_state = bpy.context.window_manager.xr_session_state

    try:
        if next_state == "SET":
            left_controller.set_input("/input/thumbstick/y", 1.0)  # from ox_sim API
            prev_val = Vector(session_state.viewer_pose_location)
            next_state = "TEST"
            return 0.5

        if next_state == "TEST":
            next_state = "DONE"

            curr_pos = Vector(session_state.viewer_pose_location)
            print(curr_pos.y, prev_val.y)
            if curr_pos.y > prev_val.y:
                print("PASS: Viewer moved forward in XR session")
            else:
                print("FAIL: Viewer did not move forward in XR session")

            return 0.1

        if next_state == "DONE":
            bpy.ops.wm.quit_blender()
    except Exception:
        traceback.print_exc()


def test_xr_start():
    init_sim()

    start_xr()

    bpy.app.timers.register(do_timer, first_interval=0.1)


if __name__ == "__main__":
    try:
        test_xr_start()
    except:
        import traceback

        traceback.print_exc()
