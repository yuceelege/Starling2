#!/usr/bin/env python3
"""
Reads CtrlLyaMsg packets from mpa_reader and prints them to console.
Usage:  python3 ctrl_lya_monitor.py [path/to/mpa_reader]
"""

import sys
import struct
import subprocess

# CtrlLyaMsg layout: 4B magic + 6 floats (action) + 1 float (V) + uint64 (timestamp_ns)
MSG_FORMAT = '=4s7fQ'
MSG_SIZE   = struct.calcsize(MSG_FORMAT)   # 44 bytes

MPA_READER_PATH = sys.argv[1] if len(sys.argv) > 1 else '/home/mpa_reader'


def main():
    proc = subprocess.Popen(
        [MPA_READER_PATH],
        stdout=subprocess.PIPE,
        bufsize=0
    )

    buf = b''
    prev_ts_ns = None

    try:
        while True:
            chunk = proc.stdout.read(MSG_SIZE - len(buf))
            if not chunk:
                break

            buf += chunk

            if len(buf) < MSG_SIZE:
                continue

            fields = struct.unpack(MSG_FORMAT, buf[:MSG_SIZE])
            buf = buf[MSG_SIZE:]

            _, dx, dy, dz, dyaw, dpitch, droll, V, ts_ns = fields  # _ = magic

            if prev_ts_ns is None:
                delay_ms_str = "N/A"
            else:
                delay_ms = (ts_ns - prev_ts_ns) / 1e6
                delay_ms_str = f"{delay_ms:.3f}"

            prev_ts_ns = ts_ns

            print(
                f"[{ts_ns}]  "
                f"dt={delay_ms_str} ms  "
                f"action=[ dx={dx:+.4f}  dy={dy:+.4f}  dz={dz:+.4f} "
                f"dyaw={dyaw:+.4f}  dpitch={dpitch:+.4f}  droll={droll:+.4f} ]  "
                f"V={V:.6f}"
            )

    except KeyboardInterrupt:
        pass
    finally:
        proc.terminate()


if __name__ == '__main__':
    main()