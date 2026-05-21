import asyncio
from ox_sim import Simulator


async def main():
    sim = Simulator()

    # Switch profile
    sim.profile = "oculus_quest_2"
    print("Profile:", sim.profile_info)

    # Get left controller
    left = sim.device("/user/hand/left")

    # Simulate trigger press
    left.set_input("/input/trigger/value", 1.0)
    left.set_input("/input/trigger/touch", True)

    await asyncio.sleep(0.2)

    left.set_input("/input/trigger/value", 0.0)
    left.set_input("/input/trigger/touch", False)

    # Wait for a new frame
    await sim.wait_frame()

    # Read first eye
    view = sim.views()[0]
    pixels, w, h = view.image()

    # RGBA at (50, 60)
    x, y = 50, 60
    idx = (y * w + x) * 4
    r, g, b, a = pixels[idx : idx + 4]

    print(f"Pixel (50,60): R={r} G={g} B={b} A={a}")

    sim.shutdown()


asyncio.run(main())
