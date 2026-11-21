#!/usr/bin/env python3
"""press_kickoff_oracle.py: PES's press state at each team tick of the kick-off, read from rendezvous captures of the selector hook.

Each capture of `teamsel_1404FB2A0` (FUN_1404FB2A0, called once per side and frame inside the team tick 0x1404F6C30 at 0x1404F784B; the
press dispatcher FUN_140535C60 is called later in the same tick, at 0x1404F7955) holds the brain Y in rcx. This prints, per capture:
  side, MI+0x116C (play state), T+0x255 (the possession latch), T+0x254 (attack sign), T+0x78 (the pass target), T+0x7C (the MATCH_UP
  man), and the eleven action words T+0xA2A0 + 4k (T = *(Y+0x6D50)), in capture order. A word is the state of the PREVIOUS dispatcher
  call of that side (the selector runs before it).

tests/sim/pes_press_kickoff_test.cpp asserts the relations of the first 24 in-play calls after the kick-off whistle
(live2/pid1231179 seq337..361 minus seq343); this tool prints the table they were read from, and GF's own rows print from the same
test (`[press-kickoff]` lines) so the two can be compared call by call.

Usage: python3 press_kickoff_oracle.py LAUNCH_DIR seqN [seqM ...]   (unpack the captures first: rendezvous/capz.py unpack LAUNCH_DIR/seqN)
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402

G_CELL = 0x1436F3DC8
MI_PTR, MI_PLAY = 0x270, 0x116C
Y_SIDE, Y_T = 0x8, 0x6D50
T_PASS, T_SECOND, T_LATCH, T_SIGN, T_WORDS = 0x78, 0x7C, 0x255, 0x254, 0xA2A0


def main():
    launch, seqs = sys.argv[1], sys.argv[2:]
    for seq in seqs:
        snap_dir = os.path.join(launch, seq)
        call = ft.load_recorded_call(snap_dir)
        y = call["gpr"]["rcx"]
        mem = fs.SnapMem(snap_dir)

        def u(addr, n):
            return int.from_bytes(mem.read(addr, n), "little")

        g = u(G_CELL, 8)
        mi = u(g + MI_PTR, 8)
        t = u(y + Y_T, 8)
        sign = struct.unpack("<b", mem.read(t + T_SIGN, 1))[0]
        words = [u(t + T_WORDS + 4 * k, 4) for k in range(11)]
        print("%s side=%d play=%d latch=%d sign=%+d pass=%d second=%d words=%s" % (
            seq, u(y + Y_SIDE, 4), u(mi + MI_PLAY, 4), u(t + T_LATCH, 1), sign, u(t + T_PASS, 4), u(t + T_SECOND, 4),
            " ".join("%02x" % w for w in words)), flush=True)


if __name__ == "__main__":
    main()
