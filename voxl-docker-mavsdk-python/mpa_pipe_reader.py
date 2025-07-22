#!/usr/bin/env python3
import subprocess
import struct

MSG_FORMAT = 'fffQ'  # 3 floats + 1 uint64
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
        x, y, z, timestamp = struct.unpack(MSG_FORMAT, data)
        print(f"GateXyzMsg → x={x:.6f}, y={y:.6f}, z={z:.6f}, ts={timestamp}")

if __name__ == '__main__':
    main()
