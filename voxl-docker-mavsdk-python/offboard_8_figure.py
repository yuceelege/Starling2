#!/usr/bin/env python3

#Specific implementation of offboard_position_ned.py

import asyncio
import math

from mavsdk import System
from mavsdk.offboard import (OffboardError, PositionNedYaw)


async def run():
    """ Does Offboard control using position NED coordinates and flies a figure-8. """

    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")

    print("Waiting for drone to connect...")
    async for state in drone.core.connection_state():
        if state.is_connected:
            print("-- Connected to drone!")
            break

    print("Waiting for drone to pass basic health checks...")
    async for health in drone.telemetry.health():
        if (
            health.is_gyrometer_calibration_ok and
            health.is_accelerometer_calibration_ok and
            health.is_magnetometer_calibration_ok and
            health.is_local_position_ok
        ):
            print("Basic health checks passed")
            break

    print("-- Arming")
    await drone.action.arm()

    print("-- Setting initial setpoint")
    await drone.offboard.set_position_ned(PositionNedYaw(0.0, 0.0, 0.0, 0.0))

    print("-- Starting offboard")
    try:
        await drone.offboard.start()
    except OffboardError as error:
        print(f"Starting offboard mode failed with error code: {error._result.result}")
        print("-- Disarming")
        await drone.action.disarm()
        return

    # Fly a figure-8 pattern using parametric lemniscate
    A = 5.0               # loop radius in meters
    period = 20.0         # time to complete full figure-8 (s)
    rate_hz = 20.0        # command update rate
    dt = 1.0 / rate_hz
    start_time = asyncio.get_event_loop().time()

    print("-- Flying figure-8")
    while True:
        now = asyncio.get_event_loop().time()
        t = now - start_time
        if t > period:
            break

        # Lemniscate of Gerono parametric equations
        theta = 2 * math.pi * (t / period)
        x = A * math.sin(theta)
        y = A * math.sin(theta) * math.cos(theta)
        yaw = math.degrees(math.atan2(y, x))

        await drone.offboard.set_position_ned(
            PositionNedYaw(x, y, -2.0, yaw)
        )
        await asyncio.sleep(dt)

    print("-- Stopping offboard")
    try:
        await drone.offboard.stop()
    except OffboardError as error:
        print(f"Stopping offboard mode failed with error code: {error._result.result}")

    print("-- Landing")
    await drone.action.land()


if __name__ == "__main__":
    asyncio.run(run())

