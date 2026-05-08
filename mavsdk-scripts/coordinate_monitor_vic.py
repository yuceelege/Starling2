#!/usr/bin/env python3

import asyncio
from mavsdk import System

async def run():
    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")

    async for state in drone.core.connection_state():
        if state.is_connected:
            print("-- Connected to drone!")
            break

    print("-- X, Y, Z, Yaw Monitor:")
    print("-- Press Ctrl+C to stop")
    print("-" * 60)
    print(f"{'Time':<12} {'X (m)':<8} {'Y (m)':<8} {'Z (m)':<8} {'Yaw (rad)':<8}")
    print("-" * 60)

    try:
        import time
        
        while True:
            async for odometry in drone.telemetry.odometry():
                position = odometry.position_body
                angular_velocity = odometry.angular_velocity_body
                
                x = position.x_m
                y = position.y_m
                z = position.z_m
                yaw = angular_velocity.yaw_rad_s
                
                current_time = time.strftime("%H:%M:%S")
                print(f"{current_time:<12} {x:<8.3f} {y:<8.3f} {z:<8.3f} {yaw:<8.3f}")
                
                await asyncio.sleep(0.1)
                break
        
    except KeyboardInterrupt:
        print("\n-- Monitoring stopped by user")
    except Exception as e:
        print(f"\n-- Error: {e}")

if __name__ == "__main__":
    asyncio.run(run()) 