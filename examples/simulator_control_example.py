import ctypes
import os
import sys
import time
from pathlib import Path


class OxVector3f(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float), ("z", ctypes.c_float)]


class OxQuaternion(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float), ("z", ctypes.c_float), ("w", ctypes.c_float)]


class OxPose(ctypes.Structure):
    _fields_ = [("position", OxVector3f), ("orientation", OxQuaternion)]


def default_library_path() -> Path:
    base = Path(__file__).resolve().parents[1]
    if sys.platform.startswith("win"):
        return base / "build" / "ox_simulator" / "ox_driver.dll"
    if sys.platform == "darwin":
        return base / "build" / "ox_simulator" / "libox_driver.dylib"
    return base / "build" / "ox_simulator" / "libox_driver.so"


def main() -> None:
    library_path = Path(os.environ.get("OX_SIM_LIB", default_library_path()))
    sim = ctypes.CDLL(str(library_path))

    sim.ox_sim_initialize.argtypes = []
    sim.ox_sim_initialize.restype = ctypes.c_int
    sim.ox_sim_shutdown.argtypes = []
    sim.ox_sim_set_current_profile.argtypes = [ctypes.c_char_p]
    sim.ox_sim_set_current_profile.restype = ctypes.c_int
    sim.ox_sim_set_device_pose.argtypes = [ctypes.c_char_p, ctypes.POINTER(OxPose), ctypes.c_uint32]
    sim.ox_sim_set_device_pose.restype = ctypes.c_int
    sim.ox_sim_set_input_state_float.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_float]
    sim.ox_sim_set_input_state_float.restype = ctypes.c_int

    if sim.ox_sim_initialize() != 0:
        raise RuntimeError("ox_sim_initialize failed")

    sim.ox_sim_set_current_profile(b"oculus_quest_2")
    print("Driving the simulator through the local C API. Press Ctrl+C to stop.")

    offset = 0.0
    try:
        while True:
            offset += 0.01
            pose = OxPose(
                position=OxVector3f(-0.2 + offset, 1.4, -0.4),
                orientation=OxQuaternion(0.0, 0.0, 0.0, 1.0),
            )
            sim.ox_sim_set_device_pose(b"/user/hand/left", ctypes.byref(pose), 1)
            sim.ox_sim_set_input_state_float(b"/user/hand/left", b"/input/trigger/value", 0.5)
            time.sleep(1.0 / 60.0)
    except KeyboardInterrupt:
        pass
    finally:
        sim.ox_sim_shutdown()


if __name__ == "__main__":
    main()
