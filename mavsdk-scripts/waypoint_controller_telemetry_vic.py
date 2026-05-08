#!/usr/bin/env python3

"""
Waypoint Controller using MAVSDK Telemetry
Moves through waypoints using drone telemetry for position feedback.
"""

import asyncio
from mavsdk import System
from mavsdk.offboard import PositionNedYaw, OffboardError

def generate_smooth_trajectory(waypoints, target_spacing=0.067, sleep_time=0.15):
    if len(waypoints) < 2:
        return waypoints
    
    smooth_trajectory = []
    
    for i in range(len(waypoints) - 1):
        start_x, start_y, start_z, start_yaw = waypoints[i]
        end_x, end_y, end_z, end_yaw = waypoints[i + 1]
        
        segment_distance = ((end_x - start_x)**2 + (end_y - start_y)**2 + (end_z - start_z)**2)**0.5
        sub_waypoints_needed = max(1, int(segment_distance / target_spacing))
        
        smooth_trajectory.append((start_x, start_y, start_z, start_yaw))
        
        for j in range(1, sub_waypoints_needed + 1):
            t = j / (sub_waypoints_needed + 1)
            x = start_x + t * (end_x - start_x)
            y = start_y + t * (end_y - start_y)
            z = start_z + t * (end_z - start_z)
            yaw = start_yaw + t * (end_yaw - start_yaw)
            
            smooth_trajectory.append((x, y, z, yaw))
    
    smooth_trajectory.append(waypoints[-1])
    
    return smooth_trajectory, sleep_time

async def get_current_position(drone):
    """Get current position from drone telemetry."""
    try:
        async for odometry in drone.telemetry.odometry():
            position = odometry.position_body
            orientation = odometry.orientation_body
            
            x = position.x_m
            y = position.y_m
            z = position.z_m
            yaw = orientation.yaw_rad * 180 / 3.14159  # Convert to degrees
            
            return x, y, z, yaw
    except Exception as e:
        print(f"Error getting position: {e}")
        return None

async def run():
    """Waypoint control using MAVSDK telemetry for position feedback."""
    
    # Define waypoints relative to starting position
    waypoints = [
        (2.0, 0.0, -0.60, 0.0),
        (4.15, -0.426, -0.60, -35.45),
        (5.39, -1.76, -0.60, -67.90),
        (5.51, -3.86, -0.60, -90.0)
    ]
    
    TARGET_SPACING = 0.067
    SLEEP_TIME = 0.15
    
    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")

    async for state in drone.core.connection_state():
        if state.is_connected:
            print("-- Connected to drone!")
            break

    print("-- Getting current position from telemetry...")
    current_pos = await get_current_position(drone)
    if current_pos is None:
        print("Failed to get position from telemetry. Exiting.")
        return

    start_x, start_y, start_z, start_yaw = current_pos
    print(f"Current position: x={start_x:.3f}, y={start_y:.3f}, z={start_z:.3f}, yaw={start_yaw:.1f}°")

    # Build full waypoint list starting from current position
    full_waypoints = [(start_x, start_y, start_z, start_yaw)]
    for x, y, z, yaw in waypoints:
        # Add waypoints relative to current position
        full_waypoints.append((start_x + x, start_y + y, start_z + z, start_yaw + yaw))

    smooth_trajectory, sleep_time = generate_smooth_trajectory(
        full_waypoints, TARGET_SPACING, SLEEP_TIME
    )

    print(f"Generated {len(smooth_trajectory)} waypoints for smooth movement")

    print("-- Ready to execute waypoints!")
    print("-- Please start offboard mode manually when ready")
    print("-- Press Enter to continue...")
    input()

    print("-- Executing waypoints...")

    for i, (x, y, z, yaw) in enumerate(smooth_trajectory):
        try:
            await drone.offboard.set_position_ned(PositionNedYaw(x, y, z, yaw))
            print(f"Waypoint {i+1}/{len(smooth_trajectory)}: x={x:.3f}, y={y:.3f}, z={z:.3f}, yaw={yaw:.1f}°")
            await asyncio.sleep(sleep_time)

        except OffboardError as error:
            print(f"Setting waypoint failed with error code: {error._result.result}")
            break

    print("-- Waypoint execution complete!")
    print("-- Staying at final position")
    print("-- You can stop offboard mode when ready")

if __name__ == "__main__":
    asyncio.run(run()) 