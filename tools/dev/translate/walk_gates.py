#!/usr/bin/env python3
"""walk_gates.py: why a carrier-walk (0x14210F110) call does or does not reach the Clear block 0x14210F4F2.

Replays each recorded walk call (a carrier_walk_210F110 capture, or a state synth_walk.py wrote) on PES's own bytes and prints,
per capture: whether the Clear block ran, the returned kind, and the inputs of the kind-1 (type 5) gates the walk evaluates
before the block (decompile 0x14210F110; addresses are the instructions the hooks read):
  0x14210F455  pressure record P = 0x14069A3E0(BP+0x2210, 0xFF): P+0x30 and P+0x34 (P+0x30 != 0 and P+0x34 > 1 goes straight to the block)
  0x14210F465  BP+0xE4 (nonzero skips the event test)
  0x14210F4AB  0x141052E30(BP+0x32B0) (zero goes to the block)
  0x14210F4BF  the previous decision record BP+0x4204 (type; 3 = dribble) and BP+0x420C (its age)
  0x14210F4E8  0x140481B00(BP+0x3380, BP+0x4210): the angle between the carrier's facing and that decision, against 60.0
               (0x14259A764); below it (previous decision a dribble of age > 2) the unit stands down, the block is skipped.
The block is reached when the pressure record says so, or the event/32B0/4204 tests fail, or the angle is >= 60.0.

Usage: PES_EXE=<pes_full.bin> python3 walk_gates.py HOOKS_CFG SCRATCH_DIR CAPTURE_DIR [CAPTURE_DIR ...]   (harness Python: unicorn)
SCRATCH_DIR receives each replay's trace (outputs are PES-derived: keep it out of the repo).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
import stubs as stub_table  # noqa: E402
import synth_walk as sw  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402

CLEAR_BLOCK = 0x14210F4F2


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    sw.patch_imports()
    orig = ft.hook_bytes(sys.argv[1])
    for d in sys.argv[3:]:
        info = {}

        def u32(u, va):
            return struct.unpack("<I", bytes(u.mem_read(va, 4)))[0]

        def on_code(u, a, size, data):
            r15 = u.reg_read(xc.UC_X86_REG_R15)  # BP
            if a == 0x14210F455:
                p = u.reg_read(xc.UC_X86_REG_RAX)
                info["P+0x30"], info["P+0x34"] = u32(u, p + 0x30), u32(u, p + 0x34)
            elif a == 0x14210F465:
                info["BP+0xE4"] = bytes(u.mem_read(r15 + 0xE4, 1))[0]
            elif a == 0x14210F4AB:
                info["0x141052E30"] = u.reg_read(xc.UC_X86_REG_EAX)
            elif a == 0x14210F4BF:
                info["BP+0x4204/420C"] = (u32(u, r15 + 0x4204), u32(u, r15 + 0x420C))
            elif a == 0x14210F4E8:
                x = u.reg_read(xc.UC_X86_REG_XMM0)
                x = x if isinstance(x, int) else int.from_bytes(x, "little")
                info["angle"] = struct.unpack("<f", struct.pack("<I", x & 0xFFFFFFFF))[0]
            elif a == CLEAR_BLOCK:
                info["clear_block"] = True

        trace = {"out": os.path.join(sys.argv[2], os.path.basename(d.rstrip("/"))),
                 "impure": ft.load_impure(os.path.join(ft.DIFF, "specs", "impure")),
                 "stubs": ft.stubs_for_run(stub_table.load())}
        os.makedirs(trace["out"], exist_ok=True)
        uc, err = ft.run_recorded(d, orig=orig, trace=trace, on_code=on_code)
        print("%s err=%s kind=%#x clear_block=%s %s" % (os.path.basename(d.rstrip("/")), err, uc.reg_read(xc.UC_X86_REG_RAX),
                                                         info.pop("clear_block", False), info), flush=True)


if __name__ == "__main__":
    main()
