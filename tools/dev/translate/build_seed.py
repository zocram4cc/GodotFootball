#!/usr/bin/env python3
"""StateSynth: build the per-impure-spec region seed from the emulated G.

The impure specs rebind G's child pointers (regions_init) to separate 2 MiB
regions, but both sides start every region zeroed. Zeroed means every G
sub-object vtable / class-flag reads 0, so the functions' "is this object
inited" gates all fail before the per-case inputs (which vary the DATA fields
the analyzer found the function reads) are reached. That is the ~6% coverage.

The seed supplies the missing STRUCTURAL base, from the emulated G
(tools/dev/translate/snapshot.py: PES's own FUN_140a44940 + FUN_140a4a300
run in the emulator). Per spec:

  * the G-shadow region  <- the emulated G (0x2e58 B: real sub-object
    vtables, class bytes, inited flags; the child slots it holds are all 0,
    match load never ran, so no dangling pointer a spec might read), then a
    0x28 pointer at every G offset the spec links to a child region (the
    slot-record pointer arrays, e.g. the E1EE links), pointing into that
    region's record.
  * each child region  <- one real record header:
        +0x1 class byte 1, +0x8 inited flag 1
      (no vtable at +0: the slot records' class predicates read the class
      byte, and a vtable would dispatch the wrong class's methods; the
      E5 MI phase words at the MI region's +0x11a0/+0x11a4 where the spec
      links the MI region).

Only structural offsets are seeded; the per-case inputs (applied after the
seed, on both sides) still vary the data fields, so the branch coverage the
inputs were built for is preserved.

The seed is PES-derived (emulated G bytes + image vtables): written to the
build tree, referenced by the spec by path, never committed.
"""
import argparse
import json
import os
import struct

GVA = "1436F3DC8"


def _region_is_g(rname):
    return rname == "IMG.%s" % GVA or rname.split(".")[-1] == GVA


def build_seed(spec, g_blob):
    """{region_name: [[offset_int, bytes], ...]} for one spec."""
    regions = spec.get("regions") or {}
    ri = spec.get("regions_init") or {}
    # g-offset (hex) -> child region name, per this spec's linkage
    links = {}
    for dst, val in ri.items():
        dname, doff = dst.split("+", 1)
        if not _region_is_g(dname) or val not in regions:
            continue
        links[int(doff, 16)] = val
    seed = {}
    for rname, r in regions.items():
        if _region_is_g(rname):
            seed[rname] = [[0, bytearray(g_blob)]]
        elif rname.split(".")[-1] == "270" and "270" not in rname.split(".")[1:]:
            # the match-info region: the E5 phase words (no vtable: some
            # specs input-vary +0x1198, and a vcall through the wrong MI
            # class is riskier than skipping it).
            b = bytearray()
            b += struct.pack("<I", 10)  # +0x11a0 possession timer
            b += struct.pack("<I", 2)   # +0x11a4
            seed[rname] = [[0x11a0, b[:4]], [0x11a4, b[4:]]]
        elif "." in rname and rname.split(".")[0] == "IMG":
            # child record region: class byte + inited flag
            hdr = bytearray(16)
            hdr[1] = 1
            struct.pack_into("<I", hdr, 0x8, 1)
            seed[rname] = [[0, bytes(hdr)]]
    # pointer arrays: at every G offset linked to a child region, store a
    # pointer into that region's record (all links of one region point to
    # its record 0; a function reading slot k gets record 0 for every k -
    # a documented approximation, the per-slot variation is the inputs').
    gname = next((n for n in regions if _region_is_g(n)), None)
    if gname:
        entries = seed[gname]
        for off, child in sorted(links.items()):
            if off >= len(g_blob) or off < 0x20:
                continue
            entries.append([off, bytearray(struct.pack("<Q", 0))])
            # patch to the child base at apply time: use a sentinel 0 here,
            # the runner replaces the pointer with the child region base.
        # record the mapping for the runner (offset -> child region)
        seed[gname + "\x00links"] = [[off, struct.pack("<4s", child.encode()[:4])]
                                     for off, child in sorted(links.items())
                                     if off < len(g_blob) and off >= 0x20]
    return seed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", required=True)
    ap.add_argument("--g", required=True, help="emulated G bytes (snapshot G.bin)")
    ap.add_argument("--out", required=True, help="seed JSON (build tree only)")
    args = ap.parse_args()
    spec = json.load(open(args.spec))
    g_blob = open(args.g, "rb").read()
    seed = build_seed(spec, g_blob)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    json.dump({"g_size": len(g_blob), "seed": seed},
              open(args.out, "w"), indent=1, sort_keys=True)
    nreg = sum(1 for k in seed if "\x00" not in k)
    print("seed: %d regions -> %s" % (nreg, args.out))
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
