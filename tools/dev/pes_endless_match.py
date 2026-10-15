#!/usr/bin/env python3
"""Make a running PES 2021 match endless: half-time and full time never fire.

Raises the two thresholds of the referee's time-up gate (FUN_14050b9b0) in the
running game's memory. The gate fires when the match clock passes
`(period in {5,7} ? FLAG : BASE) + floor(LIMIT / 3600) * 3600`; with BASE and FLAG
at ~1e12 it never does. Play, the HUD clock and every period/phase field are
untouched: the clock keeps counting, the halves simply never end.
See docs/re/pes-re-clock-live.md section 8.

Session-only: the values live in the process image, so restarting PES2021.exe
restores them. Each write checks the current bytes first and aborts on any
other value, so a different build is never written to.

usage: sudo python3 tools/dev/pes_endless_match.py [--revert]
"""
import argparse
import os
import struct
import subprocess
import sys

# .rdata constants read by FUN_14050b9b0 (PES2021.exe 1.0x, stock bytes).
TIME_UP_BASE_VA = 0x142647C68  # 162000.0: the base threshold
TIME_UP_FLAG_VA = 0x142647C64  # 54000.0: the threshold for periods 5 and 7
STOCK_BASE = 162000.0
STOCK_FLAG = 54000.0
# Far beyond any reachable clock value (the clock runs ~1e9 after an hour).
ENDLESS = 1.0e12


def pes_pid():
    # Anchored on the binary path: scripts that take the exe as an argument must not match.
    out = subprocess.run(['pgrep', '-f', r'^\S*PES2021\.exe( |$)'], capture_output=True, text=True).stdout.split()
    if len(out) != 1:
        sys.exit(f'PES2021.exe process missing or ambiguous: {out}')
    return int(out[0])


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def write_f32(fd, va, expect, new):
    old = struct.unpack('<f', os.pread(fd, 4, va))[0]
    if old == f32(new):
        print(f'{va:#x} already {old!r}')
        return
    if old != f32(expect):
        sys.exit(f'abort: {va:#x} holds {old!r}, expected {expect!r}; not this build or already patched')
    os.pwrite(fd, struct.pack('<f', new), va)
    back = struct.unpack('<f', os.pread(fd, 4, va))[0]
    print(f'{va:#x} {old!r} -> {back!r}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--revert', action='store_true', help='restore the stock thresholds')
    args = ap.parse_args()
    fd = os.open(f'/proc/{pes_pid()}/mem', os.O_RDWR)
    pairs = [(TIME_UP_BASE_VA, STOCK_BASE), (TIME_UP_FLAG_VA, STOCK_FLAG)]
    for va, stock in pairs:
        if args.revert:
            write_f32(fd, va, ENDLESS, stock)
        else:
            write_f32(fd, va, stock, ENDLESS)


if __name__ == '__main__':
    main()
