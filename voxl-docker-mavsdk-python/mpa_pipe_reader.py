#!/usr/bin/env python3
import subprocess
import struct

MSG_FORMAT = 'ffffQ'  # 4 floats (vx, vy, vz, yaw) + 1 uint64
MSG_SIZE = struct.calcsize(MSG_FORMAT)

def main():
    proc = subprocess.Popen(
        ['/home/mpa_reader'],  # Adjust if path is different
        stdout=subprocess.PIPE,
        bufsize=0
    )

    while True:
        data = proc.stdout.read(MSG_SIZE)
        if len(data) < MSG_SIZE:
            continue
        vx, vy, vz, yaw, timestamp = struct.unpack(MSG_FORMAT, data)
        print(f"ControlMsg → vx={vx:.6f}, vy={vy:.6f}, vz={vz:.6f}, yaw={yaw:.6f}, ts={timestamp}")

if __name__ == '__main__':
    main()
