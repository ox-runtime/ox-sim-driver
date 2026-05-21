import bpy
import traceback

frames = 0


def test_draw_handler_xr():
    def draw_callback():
        global frames
        frames += 1
        if frames > 10:
            print("Test passed: Draw handler executed in XR session")
            bpy.ops.wm.quit_blender()

    handler = bpy.types.SpaceView3D.draw_handler_add(draw_callback, (), "XR", "POST_VIEW")

    area = next((a for a in bpy.context.screen.areas if a.type == "VIEW_3D"), None)
    with bpy.context.temp_override(area=area):
        bpy.ops.wm.xr_session_toggle()


if __name__ == "__main__":
    try:
        test_draw_handler_xr()
    except:
        traceback.print_exc()
