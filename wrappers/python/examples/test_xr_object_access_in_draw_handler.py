import bpy
import traceback
from mathutils import Vector

eps = 0.001


def test_draw_handler_xr():
    def draw_callback():
        bpy.data.objects["Light"].location = Vector((1, 1, 1))
        new_loc = bpy.data.objects["Light"].location
        print(f"Light location in draw handler: {new_loc}")

        assert abs(new_loc.x - 1) < eps
        assert abs(new_loc.y - 1) < eps
        assert abs(new_loc.z - 1) < eps

        print("Test passed: Light location updated correctly in draw handler")
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
