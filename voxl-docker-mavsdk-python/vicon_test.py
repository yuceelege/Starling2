#!/usr/bin/env python3
import asyncio
from mavsdk import System

async def run():
    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")
    async for state in drone.core.connection_state():
        if state.is_connected:
            break
    await drone.telemetry.set_rate_odometry(10)
    async for odom in drone.telemetry.odometry():
        p = odom.position_body
        print(odom.time_usec, p.x_m, p.y_m, p.z_m)

if __name__ == "__main__":
    asyncio.run(run())

