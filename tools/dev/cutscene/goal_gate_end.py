#!/usr/bin/env python3
"""At the frame a goal's DemoInfo gate opened (rendezvous capture cut_anchor_write_47D280: DemoInfo state 5), read the goal run
object A0 (DemoInfo+0x1E8) and the 22 man positions, and say which branch of FUN_140472640 opened the gate: the scorer within
A0+0xC - A0+0x10 of T (A0+0x30), or the timer A0+0x14 at 0. Side-1 scorers print the nearest side-1 man (slot map 0x140A3B320
is not decoded). PES-derived output: scratch only.
usage: PES_EXE=pes_full.bin goal_gate_end.py LIVE2_DIR [LIVE2_DIR ...]   (each a rendezvous run folder holding seqNNN captures)"""
import glob, json, math, os, struct, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'translate'))
import full_snapshot as fs

G, ROOT = 0x1436F3DC8, 0x1436F3F88
for run in sys.argv[1:]:
    for d in sorted(glob.glob(os.path.join(run, 'seq*'))):
        try:
            if json.load(open(d + '/index.json'))['rv']['hook'] != 'cut_anchor_write_47D280':
                continue
        except Exception:
            continue
        s = fs.SnapMem(d)
        g, D = s.u64(G), s.u64(ROOT)
        mi = s.u64(g + 0x270)
        a = s.read(D + 0x1E8, 0x140)
        I = lambda o: struct.unpack('<i', a[o:o + 4])[0]
        F = lambda o: struct.unpack('<f', a[o:o + 4])[0]
        T = (F(0x30), F(0x38))
        P = []
        for i in range(22):  # man record G[(i+0xBF)*0x28]: x +0x55C, z +0x564 (FUN_140A48A10)
            e = s.u64(g + (i + 0xBF) * 0x28)
            P.append(struct.unpack('<ff', s.read(e + 0x55C, 4) + s.read(e + 0x564, 4)))
        side, idx = struct.unpack('<2I', s.read(mi + 0x15C8, 8))
        dist = [math.hypot(x - T[0], z - T[1]) for x, z in P]
        who, dd = ('slot %d' % idx, dist[idx]) if side == 0 and idx < 22 else ('side-1 nearest', min(dist[11:]))
        why = [n for n, ok in (('distance', dd < F(0xC) - F(0x10)), ('timer', I(0x14) == 0)) if ok]
        print(d, 'scene %#x bc %d radius %.3f t14 %d %s dist %.2f -> %s' % (I(8), I(0xBC), F(0xC), I(0x14), who, dd, '+'.join(why) or 'NEITHER'))
