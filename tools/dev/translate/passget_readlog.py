#!/usr/bin/env python3
"""passget_readlog.py: the ORDERED reads of one captured call (0x14094E0E0's replay), filtered to the records the
window holds (the image's own code/constants excluded): the PES side of a read-log diff against GF's window run.

Usage: PES_EXE=<pes_full.bin> python3 passget_readlog.py SNAP HOOKS   (harness Python: unicorn)
  One JSON object per line: {"a": addr, "n": size} per read, in call order.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import trap_oracle as to  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402
from unicorn import UC_HOOK_MEM_READ  # noqa: E402

reads = []


def main():
    snap_dir, hooks = sys.argv[1:3]
    import tempfile
    scratch = tempfile.mkdtemp(prefix="pgrl", dir=os.environ.get("TMPDIR"))
    regs = {}

    def on_code(uc, addr, size, data):
        if not regs:
            for r in to.REGS:
                regs[r] = uc.reg_read(getattr(xc, "UC_X86_REG_" + r.upper()))
            uc.hook_add(UC_HOOK_MEM_READ, on_read)

    def on_read(uc, access, addr, size, value, data):
        if to.IMG_LO <= addr < to.IMG_GLOBALS:
            return
        reads.append((addr, size))

    import frame_trace as ft  # noqa: E402
    trace = {"out": scratch, "impure": {}, "stubs": ft.stubs_for_run(ft.stub_table.load())}
    _, err = ft.run_recorded(snap_dir, orig=ft.hook_bytes(hooks), trace=trace, on_code=on_code)
    if err is not None:
        sys.exit("replay failed: %s" % err)
    for a, n in reads:
        print(json.dumps({"a": hex(a), "n": n}))


if __name__ == "__main__":
    main()
