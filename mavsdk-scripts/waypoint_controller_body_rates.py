#!/usr/bin/env python3


import asyncio
from mavsdk import System
from mavsdk.offboard import VelocityBodyYawspeed, OffboardError

async def run():
    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")
    
    print("Waiting for drone to connect...")
    async for state in drone.core.connection_state():
        if state.is_connected:
            print("-- Connected to drone!")
            break

    print("-- Please start offboard mode manually when ready")
    print("-- Press Enter to continue...")
    input()

    print("-- Starting offboard")
    try:
        await drone.offboard.set_velocity_body(VelocityBodyYawspeed(0.0, 0.0, 0.0, 0.0))
    except OffboardError as error:
        print(f"Setting velocity failed with error code: {error._result.result}")
        return

    print("-- Moving forward at 0.5 m/s for 4 seconds")
    await drone.offboard.set_velocity_body(VelocityBodyYawspeed(0.0, 0.2, 0.0, 10))
    await asyncio.sleep(4.0)
    
    print("-- Stopping")
    await drone.offboard.set_velocity_body(VelocityBodyYawspeed(0.0, 0.0, 0.0, 0.0))
    
    print("-- Staying at current position")
    print("-- You can stop offboard mode when ready")

if __name__ == "__main__":
    asyncio.run(run())
