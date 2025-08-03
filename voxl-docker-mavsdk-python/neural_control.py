#!/usr/bin/env python3

import asyncio
import subprocess
import struct
import time
from collections import deque
from mavsdk import System
from mavsdk.offboard import (OffboardError, VelocityBodyYawspeed)

MSG_FORMAT = 'ffffQ'  # 4 floats (vx, vy, vz, yaw) + 1 uint64
MSG_SIZE = struct.calcsize(MSG_FORMAT)
BUFFER_SIZE = 5

async def run():
    """Neural control using velocity body coordinates with pipe reading/writing."""

    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")

    print("Waiting for drone to connect...")
    async for state in drone.core.connection_state():
        if state.is_connected:
            print(f"-- Connected to drone!")
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

    # Start pipe processes
    print("Starting mpa_reader process...")
    mpa_reader_proc = subprocess.Popen(
        ['/home/mpa_reader'],
        stdout=subprocess.PIPE,
        bufsize=0
    )
    
    print("Starting mpa_writer process...")
    mpa_writer_proc = subprocess.Popen(
        ['/home/mpa_writer'],
        stdin=subprocess.PIPE,
        bufsize=0
    )

    # Control buffer for last 5 commands
    control_buffer = deque(maxlen=BUFFER_SIZE)
    last_write_time = time.time()

    print("-- Arming")
    await drone.action.arm()

    print("-- Setting initial setpoint")
    await drone.offboard.set_velocity_body(VelocityBodyYawspeed(0.0, 0.0, 0.0, 0.0))

    print("-- Starting offboard")
    try:
        await drone.offboard.start()
    except OffboardError as error:
        print(f"Starting offboard mode failed with error code: {error._result.result}")
        print("-- Disarming")
        await drone.action.disarm()
        return

    print("Starting neural control loop at 100Hz...")
    
    try:
        while True:
            # Read control command from neural network
            data = mpa_reader_proc.stdout.read(MSG_SIZE)
            if len(data) == MSG_SIZE:
                vx, vy, vz, yaw, timestamp = struct.unpack(MSG_FORMAT, data)
                control = {'vx': vx, 'vy': vy, 'vz': vz, 'yaw': yaw, 'timestamp': timestamp}
                
                # Add to buffer
                control_buffer.append(control)
                
                # Apply control to drone
                print(f"Applying control: vx={vx:.3f}, vy={vy:.3f}, vz={vz:.3f}, yaw={yaw:.3f}")
                
                await drone.offboard.set_velocity_body(
                    VelocityBodyYawspeed(vx, vy, vz, yaw)
                )
            
            # Write buffer to control_out pipe every 100ms
            current_time = time.time()
            if current_time - last_write_time >= 0.01:
                # Write all controls in buffer to the writer process
                for buffered_control in control_buffer:
                    msg_data = struct.pack(MSG_FORMAT, 
                                         buffered_control['vx'], buffered_control['vy'], 
                                         buffered_control['vz'], buffered_control['yaw'], 
                                         buffered_control['timestamp'])
                    mpa_writer_proc.stdin.write(msg_data)
                    mpa_writer_proc.stdin.flush()
                last_write_time = current_time
            
            await asyncio.sleep(0.01)  # 10ms loop (100Hz)
                
    except KeyboardInterrupt:
        print("Stopping neural control...")
    finally:
        # Cleanup
        print("-- Stopping offboard")
        try:
            await drone.offboard.stop()
        except OffboardError as error:
            print(f"Stopping offboard mode failed with error code: {error._result.result}")
        
        if mpa_reader_proc:
            mpa_reader_proc.terminate()
        if mpa_writer_proc:
            mpa_writer_proc.terminate()

if __name__ == "__main__":
    # Run the asyncio loop
    asyncio.run(run()) 