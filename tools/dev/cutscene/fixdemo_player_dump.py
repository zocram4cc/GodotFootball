#!/usr/bin/env python3
"""fixdemo_player_dump.py: what a rendezvous proc_camera_141F7BFE0 capture holds of the FixDemo player behind the camera.

For each capture (a full snapshot taken at a call of the procedural camera engine) it prints
  targets: the camera's 28 CAMERA_TARGET_02nn records (valid byte, ground, joint 10, joint 0, facing) and the record's target bytes;
  player:  the FixDemo player found by the camera pointer at player+0xC68: its human map (+0xB38: fdc Human slot -> FixDemoHuman
           actor: pitch key +0x36C, roster member +0x36D, position), the camera record map (+0xB58), the camera list the cut start
           0x141F2DD80 built (+0x88..), +0x78 (the set word) and +0x7C (the number of camera sets);
  cast:    the scene manager's 0xA8 cast list (8-byte entries [id u32][kind][side][roster slot][flag], ids 0x11xx/0x13xx/0x15xx/0x16xx).
Output is PES-derived: keep it in scratch, never commit it. Read-only on the snapshots.

Usage: PES_EXE=<pes_full.bin> GF_DIFF_PYTHON=<venv python> fixdemo_player_dump.py --cfg HOOKS_CFG [--cast] SNAP_DIR...
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "translate")))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "diffharness")))
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402

CHUNK = 1 << 20
CAM_TABLE, TARGETS = 0x10 + 0x38, 28
PLAYER_CAMERA, HUMAN_MAP, HUMAN_ARRAY, CAM_MAP, CAM_LIST, CAM_SET_WORD, CAM_SETS = 0xC68, 0xB38, 0xC70, 0xB58, 0x88, 0x78, 0x7C


def q(sm, va):
    b = sm.read(va, 8)
    return struct.unpack("<Q", b)[0] if b else None


def scan(sm, pattern, accept):
    """Every 8-aligned hit of `pattern` in a writable mapping (accept(va, bytes, i) filters)."""
    hits = []
    for m in sm.maps:
        if "w" not in m["perm"] or m["size"] > (1 << 31):
            continue
        va, end = m["va"], m["va"] + m["size"]
        while va < end:
            n = min(CHUNK, end - va)
            b = sm.read(va, n + 64 if va + n + 64 <= end else n)
            if b:
                i = b.find(pattern)
                while i >= 0:
                    if i % 8 == 0 and accept(sm, va + i, b, i):
                        hits.append(va + i)
                    i = b.find(pattern, i + 1)
            va += n
    return hits


def walk(sm, node, head, out):
    """In-order walk of an MSVC std::map: node+0 left, +8 parent, +0x10 right, +0x19 the nil flag."""
    if node is None or node == head or not sm.read(node, 0x40) or sm.read(node + 0x19, 1)[0]:
        return
    walk(sm, q(sm, node), head, out)
    out.append(node)
    walk(sm, q(sm, node + 0x10), head, out)


def record_text(sm, rp):
    rb = sm.read(rp, 0x120)
    return "start %.1f mode %d targets %s style %s" % (struct.unpack_from("<f", rb, 0xC)[0], rb[8 + 0xB0], list(rb[8 + 0xB1:8 + 0xB4]),
                                                       list(rb[8 + 0xD8:8 + 0xDF]))


def targets(sm, cam):
    rec = q(sm, cam + 0x130)
    rb = sm.read(rec, 0x11C)
    print("camera %#x state %d mode %d target bytes %s" % (cam, struct.unpack("<I", sm.read(cam + 0x128, 4))[0], rb[0xB0], list(rb[0xB1:0xB4])))
    for slot in range(TARGETS):
        entry = q(sm, cam + CAM_TABLE + 8 * slot)
        name = sm.read(entry + 0x28, 0x14).split(b"\0")[0].decode()
        raw = sm.read(q(sm, q(sm, entry + 0x10) + 0x10), 0x30)
        if raw[4]:
            f = struct.unpack_from("<10f", raw, 8)
            print("  %s valid ground (%.2f %.2f %.2f) joint10 (%.2f %.2f %.2f) joint0 (%.2f %.2f %.2f) facing %.2f" % ((name,) + f))


def player(sm, cam):
    pat = struct.pack("<Q", cam)

    def accept(sm, va, b, i):
        p = va - PLAYER_CAMERA
        hb = sm.read(p + CAM_LIST, 0x18)
        sets = sm.read(p + CAM_SETS, 4)
        if not hb or not sets:
            return False
        beg, end, _ = struct.unpack("<QQQ", hb)
        return bool(beg) and beg < end and (end - beg) // 8 < 64 and 1 <= struct.unpack("<I", sets)[0] <= 10

    for hit in scan(sm, pat, accept):
        p = hit - PLAYER_CAMERA
        print("player %#x +0x78 %d +0x7C %d" % (p, struct.unpack("<I", sm.read(p + CAM_SET_WORD, 4))[0], struct.unpack("<I", sm.read(p + CAM_SETS, 4))[0]))
        beg, end, _ = struct.unpack("<QQQ", sm.read(p + CAM_LIST, 0x18))
        for k in range((end - beg) // 8):
            print("  list[%d] %s" % (k, record_text(sm, q(sm, beg + 8 * k))))
        head = q(sm, p + CAM_MAP)
        nodes = []
        walk(sm, q(sm, head + 8), head, nodes)
        for n in nodes:
            print("  camera id %d: %s" % (struct.unpack("<i", sm.read(n + 0x20, 4))[0], record_text(sm, n + 0x28)))
        head, array = q(sm, p + HUMAN_MAP), q(sm, p + HUMAN_ARRAY)
        nodes = []
        walk(sm, q(sm, head + 8), head, nodes)
        for n in nodes:
            raw = sm.read(n, 0x40)
            fdc_slot, idx = struct.unpack_from("<I", raw, 0x20)[0], struct.unpack_from("<h", raw, 0x30)[0]
            actor = q(sm, array + 8 * idx)
            ab = sm.read(actor, 0x380)
            pos = struct.unpack_from("<3f", ab, 8)
            print("  fdc Human slot %d -> actor key %d roster member %d at (%.2f %.2f %.2f)" % (fdc_slot, ab[0x36C], ab[0x36D], *pos))


def cast_lists(sm):
    """Every cast list found: [(address, [(id, kind, side, roster slot)])]."""
    def accept(sm, va, b, i):
        return (struct.unpack_from("<I", b, i)[0] in (0x1300, 0x1100, 0x1500, 0x1600) and b[i + 4] < 3 and b[i + 6] < 0x30 and b[i + 7] <= 2
                and i + 12 <= len(b) and struct.unpack_from("<I", b, i + 8)[0] == struct.unpack_from("<I", b, i)[0] + 1)

    out = []
    for kind in (0x1300, 0x1100, 0x1500, 0x1600):
        for hit in scan(sm, struct.pack("<I", kind), accept):
            ents, a = [], hit
            while len(ents) < 40:
                raw = sm.read(a, 8)
                if not raw:
                    break
                ident = struct.unpack_from("<I", raw, 0)[0]
                if ident > 0x2000 and ident >> 8 not in (0x11, 0x13, 0x15, 0x16):
                    break
                ents.append((ident, raw[4], raw[5], raw[6]))
                a += 8
            out.append((hit, ents))
    return out


def cast(sm):
    for hit, ents in cast_lists(sm):
        print("cast list at %#x: %s" % (hit, " ".join("%#x:kind%d,side%d,slot%d" % e for e in ents)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cfg", required=True, help="rendezvous hooks.cfg (the original bytes the hooks displaced)")
    ap.add_argument("--cast", action="store_true", help="also scan for the 0xA8 cast list")
    ap.add_argument("snaps", nargs="+")
    a = ap.parse_args()
    orig = ft.hook_bytes(a.cfg)
    for snap in a.snaps:
        print("==", os.path.basename(snap.rstrip("/")))
        sm = fs.SnapMem(snap, restore=orig)
        cam = ft.load_recorded_call(snap)["gpr"]["rcx"]
        targets(sm, cam)
        player(sm, cam)
        if a.cast:
            cast(sm)


if __name__ == "__main__":
    main()
