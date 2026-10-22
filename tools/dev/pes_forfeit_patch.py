#!/usr/bin/env python3
"""Forfeit patch (owner request, 21-10-26): the minimum-players compare.

FUN_140447b50 case 6 (the forfeit check) compares each side's on-pitch count with
the threshold byte E+0x17 at 0x140447e79: `cmp al, byte ptr [rdx+0x17]` (3a 42 17).
The game sets that byte back to 7 during goal-kick stoppages, and the check runs
in a restart step, so a data write cannot hold. This patch replaces the three
bytes with `cmp al, 1` + nop (3c 01 90): a side is short only with 0 players on
the pitch. The patched compare no longer reads E+0x17, so the other readers of
that byte (emergency substitution, stretcher, substitution plan) keep stock behaviour.

Method, as tools/dev/pes_endless_match.py: check the stock bytes first and abort on
anything else, write through /proc/<pid>/mem, read back. The endless patch changes
two float constants in .rdata; this one changes code, so the instruction keeps its
length and the five bytes after it are checked too. Refuses to write unless the page
is a private mapping (a shared mapping would write through to the file on disk).

  sudo -n python3 forfeit_patch.py <pid> status    # read-only
  sudo -n python3 forfeit_patch.py <pid> apply     # 3a4217 -> 3c0190
  sudo -n python3 forfeit_patch.py <pid> revert    # 3c0190 -> 3a4217
"""
import json
import os
import sys
import time

VA = 0x140447E79
STOCK = bytes.fromhex("3a4217")   # cmp al, byte ptr [rdx + 0x17]
PATCH = bytes.fromhex("3c0190")   # cmp al, 1 ; nop
TEXT = {STOCK: "cmp al, byte ptr [rdx + 0x17]", PATCH: "cmp al, 1 ; nop"}
AFTER = {0x140447E7C: bytes.fromhex("7303"),       # jae 0x140447e81
         0x140447E7E: bytes.fromhex("c60601")}     # mov byte ptr [rsi], 1
STATE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pes_forfeit_patch.json")


def open_pes(pid, write):
    with open(f"/proc/{pid}/cmdline", "rb") as f:
        if b"PES2021.exe" not in f.read():
            sys.exit(f"pid {pid} is not PES2021.exe")
    return os.open(f"/proc/{pid}/mem", os.O_RDWR if write else os.O_RDONLY)


def mapping(pid, va):
    with open(f"/proc/{pid}/maps") as f:
        for line in f:
            rng, perms = line.split()[:2]
            lo, hi = (int(x, 16) for x in rng.split("-"))
            if lo <= va < hi:
                return line.rstrip(), perms
    return None, ""


def state(fd):
    live = os.pread(fd, 3, VA)
    name = {STOCK: "stock", PATCH: "patched"}.get(live, "OTHER")
    after_ok = all(os.pread(fd, len(b), a) == b for a, b in AFTER.items())
    return live, name, after_ok


def status(pid, fd):
    line, _ = mapping(pid, VA)
    live, name, after_ok = state(fd)
    print(f"mapping: {line}")
    print(f"bytes at {VA:#x}: {live.hex()} ({name}) -> {TEXT.get(live, 'unknown')}")
    print(f"bytes after the compare match the stock build: {after_ok}")


def apply(pid, fd):
    line, perms = mapping(pid, VA)
    if line is None or perms[3:4] != "p":
        sys.exit(f"abort: {VA:#x} is not in a private mapping: {line}")
    live, name, after_ok = state(fd)
    if not after_ok:
        sys.exit("abort: bytes after the compare differ from the stock build; wrong code")
    if name == "patched":
        print("already patched; nothing written")
        return
    if name != "stock":
        sys.exit(f"abort: {VA:#x} holds {live.hex()}, expected stock {STOCK.hex()}")
    with open(STATE, "w") as f:
        json.dump({"pid": pid, "va": VA, "stock": STOCK.hex(), "patch": PATCH.hex(),
                   "t": time.time()}, f)
    n = os.pwrite(fd, PATCH, VA)
    back = os.pread(fd, 3, VA)
    print(f"wrote {n} bytes; readback {back.hex()} -> {TEXT.get(back, 'unknown')}")
    if back != PATCH:
        sys.exit("readback mismatch")


def revert(pid, fd):
    live, name, _ = state(fd)
    if name == "stock":
        print("already stock; nothing written")
        return
    if name != "patched":
        sys.exit(f"abort: {VA:#x} holds {live.hex()}, expected patch {PATCH.hex()}")
    os.pwrite(fd, STOCK, VA)
    back = os.pread(fd, 3, VA)
    print(f"restored; readback {back.hex()} -> {TEXT.get(back, 'unknown')}")
    if back != STOCK:
        sys.exit("readback mismatch")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    pid, cmd = int(sys.argv[1]), sys.argv[2]
    if cmd == "status":
        status(pid, open_pes(pid, False))
    elif cmd == "apply":
        apply(pid, open_pes(pid, True))
    elif cmd == "revert":
        revert(pid, open_pes(pid, True))
    else:
        sys.exit(__doc__)
