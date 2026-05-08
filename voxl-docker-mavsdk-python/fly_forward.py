#!/usr/bin/env python3
"""
Arms the drone, starts offboard mode, flies straight forward at 1 m/s
for 1 second, then lands.
"""

import asyncio
from mavsdk import System
from mavsdk.offboard import OffboardError, VelocityBodyYawspeed


FORWARD_SPEED = 1.0   # m/s
DURATION       = 1.0  # seconds


async def run():
    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")

    print("Waiting for drone to connect...")
    async for state in drone.core.connection_state():
        if state.is_connected:
            print("Connected.")
            break

    print("Waiting for local position estimate...")
    async for health in drone.telemetry.health():
        if health.is_local_position_ok:
            print("Local position OK.")
            break

    print("Arming...")
    await drone.action.arm()

    # Zero setpoint required before offboard can start
    await drone.offboard.set_velocity_body(VelocityBodyYawspeed(0.0, 0.0, 0.0, 0.0))

    print("Starting offboard mode...")
    try:
        await drone.offboard.start()
    except OffboardError as e:
        print(f"Offboard start failed: {e._result.result} — disarming.")
        await drone.action.disarm()
        return

    print(f"Flying forward at {FORWARD_SPEED} m/s for {DURATION}s...")
    await drone.offboard.set_velocity_body(
        VelocityBodyYawspeed(
            forward_m_s    = FORWARD_SPEED,
            right_m_s      = 0.0,
            down_m_s       = 0.0,
            yawspeed_deg_s = 0.0
        )
    )
    await asyncio.sleep(DURATION)

    print("Stopping...")
    await drone.offboard.set_velocity_body(VelocityBodyYawspeed(0.0, 0.0, 0.0, 0.0))
    await asyncio.sleep(0.5)

    try:
        await drone.offboard.stop()
    except OffboardError:
        pass

    print("Landing.")
    await drone.action.land()


if __name__ == '__main__':
    asyncio.run(run())
