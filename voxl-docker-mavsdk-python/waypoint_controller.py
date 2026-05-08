#!/usr/bin/env python3

"""
Waypoint Controller using VIO coordinates
Moves 2m north from current position using intermediate waypoints.
"""

import asyncio
import subprocess
import struct
import csv
import os
from datetime import datetime
from mavsdk import System
from mavsdk.offboard import PositionNedYaw, OffboardError

# Message format for VioPoseMsg (4 floats: x, y, z, yaw)
MSG_FORMAT = 'ffff'  # 4 floats
MSG_SIZE = struct.calcsize(MSG_FORMAT)

def get_current_vio_position():
    try:
        process = subprocess.Popen(['./mpa_reader'],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                bufsize=0)

        stdout, stderr = process.communicate(timeout=1.0)

        if process.returncode == 0 and stdout and len(stdout) >= MSG_SIZE:
            x, y, z, yaw = struct.unpack(MSG_FORMAT, stdout[:MSG_SIZE])
            return x, y, z, yaw
        else:
            print(f"Failed to get VIO position: return code {process.returncode}")
            return None
    except Exception as e:
        print(f"Error getting VIO position: {e}")
        return None

def generate_smooth_trajectory(waypoints, target_spacing=0.067, sleep_time=0.05):
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

async def run():
    """Waypoint control with configurable waypoint list and smooth trajectory generation."""
    
    waypoints = [
        (1.718, -0.221, -0.916, -0.05),
        (3.845, 0.429, -0.920, 22.30),
        (6.202, 0.136, -0.893, -22.39)
    ]

    
    TARGET_SPACING = 0.022
    SLEEP_TIME = 0.05
    
    # Setup CSV logging
    os.makedirs('/statelog', exist_ok=True)
    csv_file = '/statelog/coordinates_random_10.csv'
    with open(csv_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['timestamp', 'x', 'y', 'z', 'yaw'])
    
    print("-- Waypoint Controller with CSV logging enabled")
    print(f"-- Logging to: {csv_file}")
    
    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")

    async for state in drone.core.connection_state():
        if state.is_connected:
            print("-- Connected to drone!")
            break

    print("-- Getting current VIO position...")
    current_pos = get_current_vio_position()
    if current_pos is None:
        print("Failed to get VIO position. Exiting.")
        return

    start_x, start_y, start_z, start_yaw = current_pos
    print(f"Current VIO position: x={start_x:.3f}, y={start_y:.3f}, z={start_z:.3f}, yaw={start_yaw:.1f}°")

    # Log initial position
    timestamp = datetime.now().isoformat()
    with open(csv_file, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([timestamp, start_x, start_y, start_z, start_yaw])

    full_waypoints = [(start_x, start_y, start_z, start_yaw)]
    for x, y, z, yaw in waypoints:
        full_waypoints.append((x, y, z, yaw))

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
            
            # Log actual VIO position (what we observe) instead of waypoint
            current_vio_pos = get_current_vio_position()
            if current_vio_pos is not None:
                vio_x, vio_y, vio_z, vio_yaw = current_vio_pos
                timestamp = datetime.now().isoformat()
                with open(csv_file, 'a', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerow([timestamp, vio_x, vio_y, vio_z, vio_yaw])
                print(f"VIO: {vio_x:8.3f} | {vio_y:8.3f} | {vio_z:8.3f} | {vio_yaw:6.1f}°")
            else:
                print("Failed to get VIO position for logging")
            
            await asyncio.sleep(sleep_time)

        except OffboardError as error:
            print(f"Setting waypoint failed with error code: {error._result.result}")
            break

    print("-- Waypoint execution complete!")
    print("-- Staying at final position")
    print(f"-- Data logged to: {csv_file}")
    print("-- You can stop offboard mode when ready")

if __name__ == "__main__":
    asyncio.run(run())
