import unittest
import bpy
import sys


class TestStartXR(unittest.TestCase):
    def test_xr_start(self):
        area = next((a for a in bpy.context.screen.areas if a.type == "VIEW_3D"), None)
        wm = bpy.context.window_manager

        self.assertIsNone(wm.xr_session_state)

        with bpy.context.temp_override(area=area):
            bpy.ops.wm.xr_session_toggle()

        self.assertIsNotNone(wm.xr_session_state)
        self.assertTrue(wm.xr_session_state.is_running(bpy.context))


if __name__ == "__main__":
    import sys

    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else [])
    unittest.main()
