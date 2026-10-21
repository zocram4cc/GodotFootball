#!/usr/bin/env python3
"""Read-only trace (21-10-26): settings chain E = *(G+0x2c0), threshold E+0x17,
and the match state: period MI+0x1168, play MI+0x116c, restart kind MI+0x1554,
stop reason MI+0x1574, on-pitch count per side. Prints one line whenever the row
changes. Never writes.

  sudo -n python3 gag_trace.py <pid> [seconds=30] [poll_s=0.2]
"""
import os
import struct
import sys
import time

G_VA, E_OFF, THR_OFF, MI_OFF = 0x1436F3DC8, 0x2C0, 0x17, 0x270


def u64(fd, va):
    return struct.unpack("<Q", os.pread(fd, 8, va))[0]


def i32(fd, va):
    return struct.unpack("<i", os.pread(fd, 4, va))[0]


def row(fd):
    try:
        g = u64(fd, G_VA)
        if not g:
            return ("G=0",)
        e = u64(fd, g + E_OFF)
        if not e:
            return ("E=0",)
        thr = os.pread(fd, 1, e + THR_OFF)[0]
    except OSError as err:
        return ("chain-err", err.errno)
    try:
        mi = u64(fd, g + MI_OFF)
        on = os.pread(fd, 22, mi + 0x1748)
        return (hex(e), thr, i32(fd, mi + 0x1168), i32(fd, mi + 0x116C),
                i32(fd, mi + 0x1554), i32(fd, mi + 0x1574),
                sum(1 for b in on[:11] if b), sum(1 for b in on[11:] if b))
    except OSError:
        return (hex(e), thr, "MI-err")


if __name__ == "__main__":
    pid = int(sys.argv[1])
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 30.0
    poll = float(sys.argv[3]) if len(sys.argv) > 3 else 0.2
    fd = os.open(f"/proc/{pid}/mem", os.O_RDONLY)
    last, end = None, time.time() + seconds
    while time.time() < end:
        r = row(fd)
        if r != last:
            print(f"{time.time():.2f} {r}", flush=True)
            last = r
        time.sleep(poll)
    print("trace end")
