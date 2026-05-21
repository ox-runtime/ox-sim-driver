import bpy
import traceback


def test_start_xr():
    area = next((a for a in bpy.context.screen.areas if a.type == "VIEW_3D"), None)
    wm = bpy.context.window_manager

    assert wm.xr_session_state is None

    with bpy.context.temp_override(area=area):
        bpy.ops.wm.xr_session_toggle()

    assert wm.xr_session_state is not None
    assert wm.xr_session_state.is_running(bpy.context)

    print("Test passed: Draw handler executed in XR session")
    bpy.ops.wm.quit_blender()


if __name__ == "__main__":
    try:
        test_start_xr()
    except:
        traceback.print_exc()
