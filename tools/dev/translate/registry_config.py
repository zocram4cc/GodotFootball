#!/usr/bin/env python3
"""registry_config.py: the 0x88-byte config block of PES's registry creator FUN_141B55E70, read from a REAL capture of it.

Usage: PES_EXE=<pes_full.bin> python3 registry_config.py CAPTURE_DIR OUT_FILE
  CAPTURE_DIR: an unpacked rendezvous capture of the registry_create_141B55E70 hook (call.bin's rcx is the block). OUT_FILE: one line of 272
  hex digits (the block's bytes). The test TheRegistryTheBootBuildsHoldsTheCapturedConfig reads it through GF_PES_REGISTRY_CONFIG; the bytes are
  PES's runtime memory and stay in scratch (never committed).
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import full_snapshot as fs  # noqa: E402

BLOCK_BYTES = 0x88


def main(argv):
    if len(argv) != 3 or "PES_EXE" not in os.environ:
        sys.exit(__doc__)
    capture, out = argv[1], argv[2]
    regs = struct.unpack("<49Q", open(os.path.join(capture, "call.bin"), "rb").read())
    rcx = regs[1]
    block = fs.SnapMem(capture, image=os.environ["PES_EXE"]).read(rcx, BLOCK_BYTES)
    with open(out, "w") as f:
        f.write(block.hex() + "\n")


if __name__ == "__main__":
    main(sys.argv)
