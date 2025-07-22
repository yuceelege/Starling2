#!/usr/bin/env python3
import asyncio
import math
from mavsdk import System
from mavsdk.telemetry import Position

async def run():
    drone = System(mavsdk_server_address="localhost", port=50051)
    print("Connecting…")
    await drone.connect(system_address="udp://:14551")
    print("Connected")

    # wait for connection
    async for state in drone.core.connection_state():
        print(f"ConnectionState: is_connected={state.is_connected}")
        if state.is_connected:
            print("Discovered")
            break

 

    # now stream health
    print("Health flags:")
    async for health in drone.telemetry.health():
        print(health)
        await asyncio.sleep(2)

if __name__ == "__main__":
    asyncio.run(run())

