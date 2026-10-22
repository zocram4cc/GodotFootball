#!/usr/bin/env python3
"""Live gag (owner request, 21-10-26): minimum players per side = 1.

Target byte: E+0x17, where E = *(G+0x2c0) is the match settings and
G = *(0x1436F3DC8). FUN_140447b50 case 6 reads it at 0x140447e79
(`cmp al, byte ptr [rdx+0x17]`; `jae` skips setting the short flag), once per
side, so it is one shared threshold: a side is short when its on-pitch count is
below the byte. Stock live value 7 (read 21-10-26). At 1, only a side with zero
players on the pitch is short.

Writes only E+0x17, and only in apply/revert. Never touches MI+0x1168/0x116c/0x1574.

  sudo -n python3 min_players_gag.py <pid> status
  sudo -n python3 min_players_gag.py <pid> apply      # 7 -> 1; aborts on any other value
  sudo -n python3 min_players_gag.py <pid> revert     # writes back the saved value
  sudo -n python3 min_players_gag.py <pid> watch [s]  # read-only; prints changes
"""
import json
import os
import struct
import sys
import time

G_VA = 0x1436F3DC8
E_OFF = 0x2C0
THR_OFF = 0x17
MI_OFF = 0x270
STOCK = 7
GAG = 1
STATE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pes_min_players_gag.json")


def open_pes(pid, write):
    with open(f"/proc/{pid}/cmdline", "rb") as f:
        if b"PES2021.exe" not in f.read():
            sys.exit(f"pid {pid} is not PES2021.exe")
    return os.open(f"/proc/{pid}/mem", os.O_RDWR if write else os.O_RDONLY)


def u64(fd, va):
    return struct.unpack("<Q", os.pread(fd, 8, va))[0]


def probe(fd):
    """(G, E, threshold) for this poll, or None while the chain is torn down."""
    try:
        g = u64(fd, G_VA)
        e = u64(fd, g + E_OFF) if g else 0
        return (g, e, os.pread(fd, 1, e + THR_OFF)[0]) if e else None
    except OSError:
        return None


def locate(fd):
    for _ in range(40):
        p = probe(fd)
        if p:
            return p
        time.sleep(0.25)
    sys.exit("settings chain not valid after 40 polls (no match loaded?)")


def status(fd):
    g, e, thr = locate(fd)
    print(f"G={g:#x} E={e:#x} E+0x17={thr}")
    try:
        mi = u64(fd, g + MI_OFF)
        on = os.pread(fd, 22, mi + 0x1748)
        period = struct.unpack("<i", os.pread(fd, 4, mi + 0x1168))[0]
        print(f"period={period} on-pitch side0={sum(1 for b in on[:11] if b)} "
              f"side1={sum(1 for b in on[11:] if b)}")
    except OSError:
        print("MI unreadable this poll")


def apply(fd):
    g, e, thr = locate(fd)
    if thr == GAG:
        print(f"already {GAG}; nothing written")
        return
    if thr != STOCK:
        sys.exit(f"abort: E+0x17 holds {thr}, expected stock {STOCK}")
    with open(STATE, "w") as f:
        json.dump({"G": g, "E": e, "old": thr, "set": GAG, "t": time.time()}, f)
    if probe(fd) != (g, e, STOCK):
        sys.exit("abort: settings chain changed between reads; nothing written")
    os.pwrite(fd, bytes([GAG]), e + THR_OFF)
    back = probe(fd)
    print(f"wrote {GAG} at E+0x17 ({e:#x}+0x17); readback {back}")
    if back != (g, e, GAG):
        sys.exit("readback mismatch")


def revert(fd):
    with open(STATE) as f:
        st = json.load(f)
    g, e, thr = locate(fd)
    if thr == st["old"]:
        print(f"already {thr}; nothing written")
        return
    if thr != GAG:
        sys.exit(f"abort: E+0x17 holds {thr}, expected {GAG}")
    os.pwrite(fd, bytes([st["old"]]), e + THR_OFF)
    print(f"restored {st['old']} at E+0x17; readback {probe(fd)}")


def watch(fd, seconds):
    last, end = None, time.time() + seconds
    while time.time() < end:
        p = probe(fd)
        if p != last:
            print(f"{time.time():.1f} {p}", flush=True)
            last = p
        time.sleep(0.5)
    print("watch end")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    pid, cmd = int(sys.argv[1]), sys.argv[2]
    if cmd == "status":
        status(open_pes(pid, False))
    elif cmd == "apply":
        apply(open_pes(pid, True))
    elif cmd == "revert":
        revert(open_pes(pid, True))
    elif cmd == "watch":
        watch(open_pes(pid, False), float(sys.argv[3]) if len(sys.argv) > 3 else 30.0)
    else:
        sys.exit(__doc__)
