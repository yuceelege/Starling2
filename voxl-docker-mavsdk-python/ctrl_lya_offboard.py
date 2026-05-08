#!/usr/bin/env python3
"""
Arms the drone, starts offboard mode, then feeds ctrl_lya action outputs
as body-frame velocity commands until Ctrl-C.

action[6] = [vx, vy, vz, vyaw, vpitch, vroll]
 → VelocityBodyYawspeed(forward=vx, right=vy, down=-vz, yawspeed_deg_s=degrees(vyaw))

Usage:  python3 ctrl_lya_offboard.py [path/to/mpa_reader]
"""

import sys
import asyncio
import struct
import subprocess
import math

from mavsdk import System
from mavsdk.offboard import OffboardError, VelocityBodyYawspeed
from mavsdk.telemetry import FlightMode

# CtrlLyaMsg layout: 4B magic + 6 floats (action) + 1 float (V) + uint64 (timestamp_ns)
MSG_FORMAT = '=4s7fQ'
MSG_SIZE   = struct.calcsize(MSG_FORMAT)   # 44 bytes

MPA_READER_PATH = sys.argv[1] if len(sys.argv) > 1 else '/home/mpa_reader'

# Safety: ignore ctrl_lya commands whose Lyapunov value exceeds this threshold
V_MAX = 1e6


def read_msg(proc) -> tuple | None:
    """Blocking read of one CtrlLyaMsg from mpa_reader stdout."""
    buf = b''
    while len(buf) < MSG_SIZE:
        chunk = proc.stdout.read(MSG_SIZE - len(buf))
        if not chunk:
            return None
        buf += chunk
    return struct.unpack(MSG_FORMAT, buf)


async def print_status(drone: System):
    """Continuously print flight mode and armed state."""
    armed = False
    mode  = None
    async for state in drone.telemetry.armed():
        armed = state
        break
    async for fm in drone.telemetry.flight_mode():
        mode = fm
        break
    print(f"  status → armed={armed}  mode={mode}")


async def run():
    drone = System(mavsdk_server_address="localhost", port=50051)
    await drone.connect(system_address="udp://:14551")

    print("Waiting for drone to connect...")
    async for state in drone.core.connection_state():
        if state.is_connected:
            print("Connected.")
            break

    print("Waiting for local position estimate...")
    async for health in drone.telemetry.health():
        if health.is_local_position_ok:
            print("Local position OK.")
            break

    print("Arming...")
    await drone.action.arm()

    # Send a zero setpoint before starting offboard (required by PX4)
    await drone.offboard.set_velocity_body(VelocityBodyYawspeed(0.0, 0.0, 0.0, 0.0))

    print("Starting offboard mode...")
    try:
        await drone.offboard.start()
    except OffboardError as e:
        print(f"Offboard start failed: {e._result.result} — disarming.")
        await drone.action.disarm()
        return

    print("Offboard active. Streaming ctrl_lya commands. Press Ctrl-C to stop.")

    proc = subprocess.Popen(
        [MPA_READER_PATH],
        stdout=subprocess.PIPE,
        bufsize=0
    )

    loop = asyncio.get_event_loop()
    status_interval = 50   # print status every N messages

    try:
        count = 0
        while True:
            msg = await loop.run_in_executor(None, read_msg, proc)
            if msg is None:
                print("mpa_reader closed — stopping.")
                break

            _, vx, vy, vz, vyaw, _p, _r, V, ts_ns = msg  # _ = magic, _p/_r = pitch/roll ignored

            if V > V_MAX:
                print(f"  V={V:.2f} exceeds threshold — holding zero.")
                await drone.offboard.set_velocity_body(
                    VelocityBodyYawspeed(0.0, 0.0, 0.0, 0.0))
                continue

            yawspeed_deg_s = math.degrees(vyaw)
            await drone.offboard.set_velocity_body(
                VelocityBodyYawspeed(
                    forward_m_s      = vx,
                    right_m_s        = vy,
                    down_m_s         = -vz,
                    yawspeed_deg_s   = yawspeed_deg_s
                )
            )

            count += 1
            if count % status_interval == 0:
                print(f"[{ts_ns}]  V={V:.4f}  "
                      f"cmd=({vx:+.3f}, {vy:+.3f}, {vz:+.3f})m/s  "
                      f"yawrate={yawspeed_deg_s:+.1f}°/s")
                await print_status(drone)

    except KeyboardInterrupt:
        print("\nCtrl-C — stopping offboard.")
    finally:
        proc.terminate()
        try:
            await drone.offboard.stop()
        except OffboardError:
            pass
        await drone.action.land()
        print("Landed.")


if __name__ == '__main__':
    asyncio.run(run())
