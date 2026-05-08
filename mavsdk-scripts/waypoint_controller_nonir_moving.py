#!/usr/bin/env python3
import asyncio
import subprocess
import struct
import numpy as np
import os
import time
from mavsdk import System
from mavsdk.offboard import VelocityBodyYawspeed, OffboardError


# def compute_centroid(mask):
#     ys, xs = np.where(mask == 255)
#     if xs.size == 0:
#         return -2.0, -2.0
#     return xs.mean(), ys.mean()+30


def control_from_centroid(cx, cy, img_w=320, img_h=320, deadband=0.05,
                          k_yaw=(0.1), k_vy=1.8,k_vz=0.6, forward_speed=0.5,
                          yaw_clip=45.0, vy_clip=0.3, vz_clip=0.3):
    if cx < 0 or cy < 0:
        return 0.0, 0.0, 0.0
    nx = (cx - img_w/2) / (img_w/2)
    ny = (cy - img_h/2) / (img_h/2)
    #if abs(nx) <= deadband and abs(ny) <= deadband:
    #    return 0.5, 0.0, 0.0
    yaw_rate = float(np.clip(nx * k_yaw * 180/np.pi, -yaw_clip, yaw_clip))
    vy = float(np.clip(nx * k_vy, -vy_clip, vy_clip))
    vz = float(np.clip(ny * k_vz, -vz_clip, vz_clip))
    return 0.0, vy, vz, yaw_rate


def largest_component_centroid(mask):
    import cv2, numpy as np
    m = np.ascontiguousarray(mask, dtype=np.uint8)
    # Remove threshold since UNet already gives binary output
    num, labels, stats, _ = cv2.connectedComponentsWithStats(m, connectivity=8)
    if num <= 1:
        return -2.0, -2.0
    largest_label = 1 + np.argmax(stats[1:, cv2.CC_STAT_AREA])
    cm = (labels == largest_label).astype(np.uint8) * 255
    mom = cv2.moments(cm, binaryImage=True)
    if mom["m00"] == 0:
        return -2.0, -2.0
    #print(float(mom["m10"] / mom["m00"]), float(mom["m01"] / mom["m00"]) +20)
    return float(mom["m10"] / mom["m00"]), float(mom["m01"] / mom["m00"]) + 30


class UnetMaskReader:
    def __init__(self):
        self.process = None
        self.frame_count = 0
        self.running = False
        self.buffer = b''
        
    async def start_reader(self):
        """Start the mpa_reader subprocess"""
        try:
            self.process = subprocess.Popen(
                ["./mpa_reader"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                bufsize=0
            )
            self.running = True
            self.buffer = b''
            print("-- mpa_reader started")
            return True
        except Exception as e:
            print(f"Failed to start mpa_reader: {e}")
            return False
    
    async def read_frame(self):
        """Read frames with high-speed buffer processing"""
        if not self.process or not self.running:
            return None
            
        try:
            # Read available data into buffer
            data = self.process.stdout.read(4096)  # Read in chunks
            if not data:
                return None
                
            self.buffer += data
            
            # Process ALL complete frames in buffer (high-speed processing)
            frames = []
            while len(self.buffer) >= 16:  # At least header size
                # Try to parse header
                header_data = self.buffer[:16]
                try:
                    timestamp, width, height = struct.unpack('<QII', header_data)
                except Exception as e:
                    print(f"Error parsing header: {e}")
                    #self.buffer = self.buffer[1:]
                    return None
                
                # Validate dimensions
                if width <= 0 or height <= 0 or width > 2000 or height > 2000:
                    # Invalid header, skip one byte and try again
                    self.buffer = self.buffer[1:]
                    continue
                
                # Calculate total frame size
                mask_size = width * height
                total_frame_size = 16 + mask_size
                
                # Check if we have complete frame
                if len(self.buffer) < total_frame_size:
                    # Incomplete frame, wait for more data
                    break
                
                # Extract complete frame
                frame_data = self.buffer[:total_frame_size]
                self.buffer = self.buffer[total_frame_size:]
                
                # Parse the frame
                mask_data = frame_data[16:]
                
                self.frame_count += 1
                
                # Convert to numpy array
                try:
                    mask_array = np.frombuffer(mask_data, dtype=np.uint8).reshape(height, width)
                    frames.append({
                        'timestamp': timestamp,
                        'width': width,
                        'height': height,
                        'mask': mask_array,
                        'frame_count': self.frame_count
                    })
                except Exception as e:
                    print(f"Error converting mask data: {e}")
                    continue
            
            # Return the LATEST frame (most recent)
            if frames:
                return frames[-1]  # Return the newest frame
            
            return None
            
        except Exception as e:
            print(f"Error reading frame: {e}")
            return None
    
    def stop_reader(self):
        """Stop the mpa_reader subprocess"""
        self.running = False
        #if self.process and self.process.poll() is None:
        self.process.terminate()
        self.process.wait()


async def main():
    # Connect to drone
    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")

    async for state in drone.core.connection_state():
        if state.is_connected:
            print("-- Connected to drone!")
            break

    print("-- Please start offboard mode manually when ready")
    print("-- Press Enter to continue...")
    input()

    # Initialize mask reader
    mask_reader = UnetMaskReader()
    await mask_reader.start_reader()

    print("-- Entering control loop")
    
    while True:
        try:
            frame = await mask_reader.read_frame()
            
            if frame is not None:
                cx, cy = largest_component_centroid(frame['mask'])
                vx, vy, vz, yaw_rate = control_from_centroid(cx, cy, img_w=frame['width'], img_h=frame['height'])
                
                print(f"Centroid: ({cx:.2f}, {cy:.2f}), Control: ({vx:.3f}, {vy:.3f}, {vz:.3f}, {yaw_rate:.3f})")
                
                await drone.offboard.set_velocity_body(VelocityBodyYawspeed(vx, vy, vz, yaw_rate))
                
            else:
                pass
                # else:
                #     # Process still running, just no frame available yet
                #     await asyncio.sleep(0.01)  # Wait a bit for more data
           
            
            await asyncio.sleep(0.001)  # High-speed loop (1000Hz)
        
        except Exception as e:
            print(f"Error in control loop: {e}")
            print("Setting velocity to 0,0,0 for safety")
            try:
                # await drone.offboard.set_velocity_body(VelocityBodyYawspeed(0.0, 0.0, 0.0, 0.0))
                pass
            except:
                pass
    # finally:
    #     print("-- Stopping")
    #     mask_reader.stop_reader()
    #     await drone.offboard.set_velocity_body(VelocityBodyYawspeed(0.0, 0.0, 0.0, 0.0))
    #     print("-- Staying at current position")
    #     print("-- You can stop offboard mode when ready")


if __name__ == "__main__":
    asyncio.run(main())
