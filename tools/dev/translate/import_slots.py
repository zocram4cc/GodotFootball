#!/usr/bin/env python3
"""import_slots.py: the named import slots of the PES image (import_slots.json).

An absolute indirect call (CALL/JMP qword ptr [slot]) whose slot holds an OS export is a call through
an import address table entry. The slot's name comes from the loaded PE module that holds its value:
read the slot's qword from a snapshot, find the module whose image range (base, SizeOfImage) holds that
value, and take the export at exactly that RVA. The module is found by range, not by mapping name: a
DLL's code sections can be anonymous in the snapshot.

Slots holding exe code, null, or an address with no exact export are recorded with a kind and no name.

Usage: import_slots.py --snapshot SNAPDIR [--universe tools/dev/ledger/universe.json] [--out import_slots.json]
"""
import argparse
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import full_snapshot as fs  # noqa: E402

IMAGE_LO, IMAGE_HI = 0x140001000, 0x14252E800  # the translated image range (driver.CODE range)
DEFAULT_OUT = os.path.join(HERE, "import_slots.json")
ABS_CALL = re.compile(r"(?:CALL|JMP) qword ptr \[(0x[0-9a-fA-F]+)\]$")


def universe_slots(universe_path):
    """Every distinct absolute slot the universe calls or jumps through."""
    rows = json.load(open(universe_path))["functions"]
    slots = set()
    for r in rows.values():
        for ind in r.get("indirect") or []:
            m = ABS_CALL.match(ind.get("text", "").strip())
            if m:
                slots.add(int(m.group(1), 16))
    return sorted(slots)


def modules_of(sm):
    """[(base, end, file name)] of every PE image in the snapshot: a named mapping whose base holds 'MZ'.
    The extent is SizeOfImage, so an address in an anonymous section of the image still belongs to it."""
    out = []
    for name in sorted({m["name"] for m in sm.maps if m["name"]}):
        base = min(m["va"] for m in sm.maps if m["name"] == name)
        if sm.u16(base) != 0x5A4D:  # 'MZ'
            continue
        e_lfanew = sm.i32(base + 0x3C) or 0
        size = sm.u64(base + e_lfanew + 24 + 56) & 0xFFFFFFFF  # PE32+ SizeOfImage
        out.append((base, base + size, os.path.basename(name)))
    return out


def classify(sm, slot, modules):
    """{"kind": "export", "module", "export"} for an exact OS export; otherwise a kind and no name."""
    v = sm.u64(slot)
    if v == 0:
        return {"kind": "null"}
    if IMAGE_LO <= v < IMAGE_HI:
        return {"kind": "image-code"}
    hit = [(b, mod) for b, e, mod in modules if b <= v < e]
    if not hit:
        return {"kind": "unmapped"}
    base, mod = hit[0]
    names = [nm for rva, nm in sm._exports_of(base)[1] if rva == v - base]
    if not names:
        return {"kind": "no-exact-export", "module": mod}
    return {"kind": "export", "module": mod, "export": names[0]}


def build(snap_dir, slots):
    sm = fs.SnapMem(snap_dir)
    mods = modules_of(sm)
    return {"0x%X" % s: classify(sm, s, mods) for s in slots}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--snapshot", required=True)
    ap.add_argument("--universe", default=os.path.join(HERE, "..", "ledger", "universe.json"))
    ap.add_argument("--out", default=DEFAULT_OUT)
    args = ap.parse_args()
    table = build(args.snapshot, universe_slots(args.universe))
    doc = {"note": ("Named import slots: each absolute CALL/JMP qword ptr [slot] the universe reaches, named by "
                    "the export its snapshot value resolves to (import_slots.py). kind export = an OS export; "
                    "anything else is not an import and keeps the dispatch path."),
           "slots": table}
    with open(args.out, "w") as f:
        json.dump(doc, f, indent=1, sort_keys=True)
    kinds = collections.Counter(v["kind"] for v in table.values())
    print("slots %d: %s -> %s" % (len(table), dict(kinds), args.out))


if __name__ == "__main__":
    sys.exit(main())
