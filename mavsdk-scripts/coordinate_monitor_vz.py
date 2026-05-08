#!/usr/bin/env python3
import asyncio
import subprocess
import struct
from mavsdk import System

# Message format for VioPoseMsg (7 floats: x, y, z, yaw, vx, vy, vz)
MSG_FORMAT = 'fffffff'  # 7 floats
MSG_SIZE = struct.calcsize(MSG_FORMAT)

async def main():
    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")

    async for state in drone.core.connection_state():
        if state.is_connected:
            print("-- Connected to drone!")
            break

    print("-- Monitoring VIO coordinates from mpa_reader pipe...")
    print("-- Format: x(m) | y(m) | z(m) | yaw(deg) | vx(m/s) | vy(m/s) | vz(m/s)")
    print("-" * 80)

    try:
        while True:
            # Run mpa_reader to get one reading
            try:
                process = subprocess.Popen(['./mpa_reader'], 
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE,
                                        bufsize=0)
                
                # Wait for completion with timeout
                stdout, stderr = process.communicate(timeout=1.0)
                
                if process.returncode == 0 and stdout:
                    # Read raw binary data (like mpa_pipe_reader.py)
                    if len(stdout) >= MSG_SIZE:
                        try:
                            # Unpack the binary data
                            x, y, z, yaw, vx, vy, vz = struct.unpack(MSG_FORMAT, stdout[:MSG_SIZE])
                            print(f"Position: {x:8.3f} | {y:8.3f} | {z:8.3f} | {yaw:6.1f}° | {vx:6.2f} | {vy:6.2f} | {vz:6.2f}")
                        except struct.error as e:
                            print(f"Struct unpack error: {e}")
                            print(f"Received {len(stdout)} bytes, expected {MSG_SIZE}")
                    else:
                        print(f"Not enough data: got {len(stdout)} bytes, need {MSG_SIZE}")
                        
                else:
                    print(f"No data from pipe (return code: {process.returncode})")
                    if stderr:
                        print(f"Stderr: {stderr}")
                    
            except subprocess.TimeoutExpired:
                print("mpa_reader timeout - killing process")
                process.kill()
                process.communicate()
            except Exception as e:
                print(f"Error running mpa_reader: {e}")
            
            # Wait before next reading
            await asyncio.sleep(0.01)  # 10Hz update rate
            
    except KeyboardInterrupt:
        print("\n-- Stopping coordinate monitoring")
    except Exception as e:
        print(f"Error: {e}")


if __name__ == "__main__":
    asyncio.run(main())
