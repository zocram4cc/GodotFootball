#!/usr/bin/env python3
"""Regenerate GF_PES_DRIBBLE_FRAMES: the frame F of REAL dribble-tick entries (live2 hook dribbletick_1407B37D0) and the man and ball they hold.

PES-derived runtime memory: the output goes to scratch and is NEVER committed (tests/sim/pes_decision_frame_test.cpp reads it through
GF_PES_DRIBBLE_FRAMES and skips when it is unset). One line per capture:
  <pid>/<seq> <slot> <P+8 x> <P+0x10 z> <ball0 x> <ball0 y> <ball0 z> <F[0]> <F[1]> <F+0x58> <F+0x5C> <F+0x5E4> <F+0x14> <F+0x18> <F+0x1C>
(hex words; the ball is G's ball record's sample 0, B+0x88).

Usage: dribble_frames.py --live2 DIR --scratch DIR --out FILE
"""
import argparse, glob, os, shutil, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
HOOK = "dribbletick_1407B37D0"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--live2", required=True)
    ap.add_argument("--scratch", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    sys.path.insert(0, HERE)
    from full_snapshot import SnapMem
    out = open(a.out, "w")
    d = os.path.join(a.scratch, "df_cap")
    os.makedirs(a.scratch, exist_ok=True)
    for src in sorted(glob.glob(os.path.join(a.live2, "pid*", "seq*"))):
        try:
            if '"hook": "%s"' % HOOK not in open(os.path.join(src, "index.json")).read():
                continue
        except OSError:
            continue
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        for f in os.listdir(src):
            shutil.copy(os.path.join(src, f), d)
        if os.path.exists(os.path.join(d, "mem.bin.zst")):
            subprocess.run([sys.executable, os.path.join(REPO, "tools/dev/rendezvous/capz.py"), "unpack", d], capture_output=True, check=True)
        m = SnapMem(d)
        call = struct.unpack("<49Q", open(os.path.join(d, "call.bin"), "rb").read(49 * 8))
        u = lambda addr, k=4: int.from_bytes(m.read(addr, k), "little")
        P = call[2]
        F = u(call[8], 8)  # the tick's third argument is the slot holding F's address
        G = u(0x1436F3DC8, 8)
        ball = u(G + 0x40, 8) + 0x88
        words = [u(P + 0x2458, 1), u(P + 8), u(P + 0x10), u(ball), u(ball + 4), u(ball + 8), u(F), u(F + 4), u(F + 0x58), u(F + 0x5C), u(F + 0x5E4),
                 u(F + 0x14), u(F + 0x18), u(F + 0x1C)]
        pid, seq = src.rstrip("/").split("/")[-2:]
        out.write("%s/%s %s\n" % (pid, seq, " ".join("%x" % w for w in words)))
        shutil.rmtree(d, ignore_errors=True)
    out.close()


if __name__ == "__main__":
    main()
