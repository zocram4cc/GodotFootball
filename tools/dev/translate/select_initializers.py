#!/usr/bin/env python3
"""select_initializers.py: which of PES's C++ static initializers the translated layer needs run.

PES's process start runs a table of static initializers (one contiguous run of code pointers, 0x142530868 up to 0x14253DF08 in the unpacked image:
the `.CRT$XCU` entries, 6,849 of them). The dump holds their results plus the dead game's runtime mutations, so a global the layer reads can hold
pointers into a heap that does not exist here (a std::map's head node, a hash table). An initializer is NEEDED when it stores to, or takes the
address of, a `.data`/`.bss` cell that a translated function names: running it builds the object the way PES's start-up did.

usage: select_initializers.py --image IMG [--layer DIR] [--roots OUT]   (prints the count; --roots writes one VA per line, table order)
A cell matches when the initializer's operand is the cell itself or an object start up to OBJECT_SPAN bytes below it (a member address).
"""
import argparse
import bisect
import glob
import os
import re
import struct
import sys

import capstone

BASE = 0x140000000
TABLE_VA, TABLE_ENTRIES = 0x142530868, 6868
CODE = (0x140001000, 0x14252E800)
GLOBALS = (0x14339B000, 0x143C98000)  # .data (initialized + virtual .bss tail) up to .xtext
OBJECT_SPAN = 0x40
SCAN_BYTES = 0x100  # an initializer is a few instructions; its leading bytes name what it builds
HERE = os.path.dirname(os.path.abspath(__file__))


def layer_globals(layer):
    """Every .data/.bss address a translated function names (`0x1436CF7E8ULL` constants and `k_c...` baked cells)."""
    out = set()
    rx = re.compile(r"0x(14[3-4][0-9A-F]{6})ULL")
    for path in glob.glob(os.path.join(layer, "*", "pes_fn_*.cpp")):
        with open(path) as f:
            for m in rx.finditer(f.read()):
                v = int(m.group(1), 16)
                if GLOBALS[0] <= v < GLOBALS[1]:
                    out.add(v)
    return sorted(out)


def table(image):
    ents = []
    for k in range(TABLE_ENTRIES):
        q = struct.unpack_from("<Q", image, TABLE_VA + 8 * k - BASE)[0]
        if q:
            if not CODE[0] <= q < CODE[1]:
                raise SystemExit("the initializer table holds a non-code word %#x at entry %d" % (q, k))
            ents.append(q)
    return ents


def stores(image, entry):
    """Rip-relative `.data`/`.bss` operands in the initializer's first instructions (up to its ret)."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    out = set()
    for i in md.disasm(image[entry - BASE:entry - BASE + SCAN_BYTES], entry):
        if i.mnemonic in ("ret", "int3"):
            break
        for op in i.operands:
            if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                t = i.address + i.size + op.mem.disp
                if GLOBALS[0] <= t < GLOBALS[1]:
                    out.add(t)
    return out


def select(image, layer):
    cells = layer_globals(layer)
    chosen = []
    for e in table(image):
        for t in stores(image, e):
            k = bisect.bisect_left(cells, t)
            if k < len(cells) and cells[k] - t < OBJECT_SPAN:
                chosen.append(e)
                break
    return chosen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--layer", default=os.path.join(HERE, "..", "..", "..", "src", "pes", "translated"))
    ap.add_argument("--roots", help="write the selected initializer VAs here, one per line, in table order")
    a = ap.parse_args()
    with open(a.image, "rb") as f:
        image = f.read()
    chosen = select(image, a.layer)
    print("select_initializers: %d of the table's initializers need running" % len(chosen))
    if a.roots:
        with open(a.roots, "w") as f:
            f.write("".join("0x%X\n" % e for e in chosen))
    return 0


if __name__ == "__main__":
    sys.exit(main())
