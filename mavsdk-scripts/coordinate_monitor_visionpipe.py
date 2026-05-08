#!/usr/bin/env python3

import asyncio
import subprocess
import struct
import numpy as np
import os
import time
from mavsdk import System

def compute_centroid(mask):
    ys, xs = np.where(mask == 255)
    if xs.size == 0:
        return -2.0, -2.0
    return xs.mean(), ys.mean()

def control_from_centroid(cx, cy, img_w=320, img_h=320, deadband=0.05,
                          k_yaw=(-1.5/0.8), k_vy=1.0, forward_speed=0.5,
                          yaw_clip=45.0, vy_clip=0.8):
    if cx < 0 or cy < 0:
        return 0.0, 0.0, 0.0
    nx = (cx - img_w/2) / (img_w/2)
    ny = (cy - img_h/2) / (img_h/2)
    if abs(nx) <= deadband and abs(ny) <= deadband:
        return 0.5, 0.0, 0.0
    yaw_rate = float(np.clip(nx * k_yaw * 180/np.pi, -yaw_clip, yaw_clip))
    vy = float(np.clip(-ny * k_vy, -vy_clip, vy_clip))
    return 0.0, vy, yaw_rate

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
        """Read one frame from mpa_reader with proper synchronization"""
        if not self.process or not self.running:
            return None
            
        try:
            # Read available data into buffer
            data = self.process.stdout.read(4096)  # Read in chunks
            if not data:
                return None
                
            self.buffer += data
            
            # Look for complete frames in buffer
            while len(self.buffer) >= 16:  # At least header size
                # Try to parse header
                header_data = self.buffer[:16]
                timestamp, width, height = struct.unpack('<QII', header_data)
                
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
                    return None
                
                # Extract complete frame
                frame_data = self.buffer[:total_frame_size]
                self.buffer = self.buffer[total_frame_size:]
                
                # Parse the frame
                mask_data = frame_data[16:]
                
                self.frame_count += 1
                
                # Convert to numpy array
                try:
                    mask_array = np.frombuffer(mask_data, dtype=np.uint8).reshape(height, width)
                except Exception as e:
                    print(f"DEBUG: Error converting mask data: {e}")
                    continue
                
                return {
                    'timestamp': timestamp,
                    'width': width,
                    'height': height,
                    'mask': mask_array,
                    'frame_count': self.frame_count
                }
            
            return None
            
        except Exception as e:
            print(f"Error reading frame: {e}")
            return None
    
    def stop_reader(self):
        """Stop the mpa_reader subprocess"""
        self.running = False
        if self.process and self.process.poll() is None:
            self.process.terminate()
            self.process.wait()

async def display_mask(frame_data):
    """Display the mask data with centroid and control info"""
    if not frame_data:
        return
        
    # Clear screen and display
    os.system('clear')
    print(f"UNet Mask - Frame {frame_data['frame_count']}")
    print(f"Timestamp: {frame_data['timestamp']}")
    print(f"Dimensions: {frame_data['width']}x{frame_data['height']}")
    print("=" * 50)
    
    # Compute centroid
    cx, cy = compute_centroid(frame_data['mask'])
    print(f"Centroid: ({cx:.2f}, {cy:.2f})")
    
    # Compute control
    vx, vy, yaw_rate = control_from_centroid(cx, cy)
    print(f"Control: vx={vx:.3f}, vy={vy:.3f}, yaw_rate={yaw_rate:.3f}")
    print("=" * 50)
    
    # Comment out the fancy visualization for now
    # # Display mask preview (first 20x20)
    # display_size = min(20, frame_data['height'], frame_data['width'])
    # print(f"Mask Preview ({display_size}x{display_size}):")
    # print("=" * (display_size * 2 + 10))
    # 
    # for i in range(display_size):
    #     row = frame_data['mask'][i][:display_size]
    #     print(f"{i:2d}: {' '.join('██' if p > 0 else '  ' for p in row)}")
    # 
    # if frame_data['height'] > display_size or frame_data['width'] > display_size:
    #     print(f"... (showing {display_size}x{display_size} of {frame_data['width']}x{frame_data['height']})")

async def do_other_work():
    """Example of other work you can do while monitoring"""
    counter = 0
    while True:
        counter += 1
        print(f"\n[OTHER WORK] Counter: {counter}")
        await asyncio.sleep(2)  # Do other work every 2 seconds

async def main():
    # Connect to drone
    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")

    async for state in drone.core.connection_state():
        if state.is_connected:
            print("-- Connected to drone!")
            break

    print("-- Starting UNet mask monitor:")
    print("-- Press Ctrl+C to stop")

    # Initialize mask reader
    mask_reader = UnetMaskReader()
    
    try:
        # Create tasks for concurrent execution
        tasks = [
            asyncio.create_task(mask_monitor_loop(mask_reader)),
            asyncio.create_task(do_other_work())
        ]
        
        # Run both tasks concurrently
        await asyncio.gather(*tasks)
        
    except KeyboardInterrupt:
        print("\n-- Stopping...")
    finally:
        mask_reader.stop_reader()

async def mask_monitor_loop(mask_reader):
    """Main loop for monitoring masks"""
    while True:
        try:
            # Start mpa_reader for continuous reading
            if not await mask_reader.start_reader():
                await asyncio.sleep(1)
                continue

            # Read frames continuously from the same process
            while mask_reader.running:
                frame_data = await mask_reader.read_frame()
                
                if frame_data:
                    # Display the mask
                    await display_mask(frame_data)
                else:
                    # No frame available, check if process is still running
                    if mask_reader.process and mask_reader.process.poll() is not None:
                        # Process exited, break to restart
                        print("-- mpa_reader exited, restarting...")
                        break
                    else:
                        # Process still running but no data, wait a bit
                        await asyncio.sleep(0.001)  # Very short delay for 6Hz capture
            
            # Stop the reader and wait before restarting
            mask_reader.stop_reader()
            await asyncio.sleep(0.1)
            
        except Exception as e:
            print(f"Error in mask monitor loop: {e}")
            mask_reader.stop_reader()
            await asyncio.sleep(0.1)

if __name__ == "__main__":
    asyncio.run(main())
