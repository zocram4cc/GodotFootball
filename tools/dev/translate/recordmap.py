#!/usr/bin/env python3
"""RecordMap: memory-access map of the four PES port chains, grouped by record identity, merged into fieldmap.json.

Usage:
  python3 tools/dev/translate/recordmap.py analyze --image PES_EXE --out ANALYSIS.json
  python3 tools/dev/translate/recordmap.py merge --analysis ANALYSIS.json --pin PIN.json \
      --fieldmap tools/dev/translate/fieldmap.json [--report REPORT.md]

analyze: specgen.analyze (capstone; object identity = entry arg root + pointer path + offset) on each chain's top
  level. Callees are the direct-call closure of the top level, so every access is relative to the top level's entry
  registers. Sizes: .pdata instruction count, else the ledger universe's count.
merge: picks the curated record identities (top level + object root), groups their fields, attaches the live pin
  verdicts from recordmap_pin.py, and writes them under fieldmap.json "recordmap" (other keys untouched).

Chains (the four port slices): KICK = KickInputs, TRAP = TrapClipPort, CLEAR = EndClear2, SANCTION = SanctionSwitch.
Verdicts (live pins, GF citations) are in the report; this file holds the mechanics only.
"""
import argparse
import json
import os
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "diffharness"))
sys.path.insert(0, HERE)

from capstone import Cs, CS_ARCH_X86, CS_MODE_64  # noqa: E402
from capstone.x86 import X86_OP_IMM  # noqa: E402
from specgen import analyze as AN  # noqa: E402
from specgen import sets  # noqa: E402
import pdata  # noqa: E402

CHAINS = {
    "KICK": ["0x14083CC30", "0x1408414A0", "0x1408398D5", "0x140723320", "0x1406B0250", "0x1406EA340",
             "0x140A6E340", "0x140A48910", "0x140A48A10"],
    "TRAP": ["0x1407A01C0", "0x14079A470", "0x14079AFD0", "0x140799440", "0x14079FBC0", "0x14079D310",
             "0x140857220", "0x140857250", "0x140858B90", "0x1408586A0", "0x14085B900", "0x140858260",
             "0x14085F150", "0x14084DF20", "0x14085B3E0", "0x14085C070", "0x14084DB00", "0x14084BCA0",
             "0x140798A70"],
    "CLEAR": ["0x142109EB0", "0x142108630", "0x14063A290", "0x14063A370", "0x14063A490", "0x14063C560",
              "0x14063C9F0", "0x14063D2F0"],
    "SANCTION": ["0x14047F170", "0x140A81230", "0x140A80CF0", "0x140A80EF0", "0x140A80D10", "0x14042D5E0",
                 "0x140A3A690", "0x140A3DAC0", "0x140A3E150", "0x140A3E190", "0x140480910", "0x140A3DC40"],
}

# Record identities: (identity, top level, object root in that top level's analysis, chain).
# The object root is the analyzer's name (A0..A3 = entry registers; IMG.1436F3DC8.<off> = *(G+off) record).
RECORDS = [
    ("kick.P (kicker ap; rcx of 0x14083CC30)", "0x14083CC30", "A0", "KICK"),
    ("G anime table entry (block start, G+0x19C8)", "0x14083CC30", "IMG.1436F3DC8.19C8", "KICK"),
    ("G man table entry (block start + 0x32C, G+0x1DD8)", "0x14083CC30", "IMG.1436F3DC8.1DD8", "KICK"),
    ("G slot table entry (block start + 0x10F0, G+0x25F8)", "0x14083CC30", "IMG.1436F3DC8.25F8", "KICK"),
    ("G+0x180 carrier table entry (stride 0x74)", "0x14083CC30", "IMG.1436F3DC8.180", "KICK"),
    ("G+0x40 carrier record", "0x142108630", "IMG.1436F3DC8.40", "CLEAR"),
    ("MI (G+0x270)", "0x14047F170", "IMG.1436F3DC8.270", "SANCTION"),
    ("ctx = corridor-angle A0 = Clear unit+8 = sanction A1", "0x142108630", "A0", "CLEAR"),
    ("E0 ring (ctx+0xE0)", "0x142108630", "A0.E0", "CLEAR"),
    ("ctx+0x18 object", "0x142108630", "A0.18", "CLEAR"),
    ("Clear unit (r8 of 0x142109EB0)", "0x142109EB0", "A2", "CLEAR"),
    ("carrier record (unit+0)", "0x142109EB0", "A2.0", "CLEAR"),
    ("imageA (unit+0x28)", "0x142109EB0", "A2.28", "CLEAR"),
    ("sanction A1 (0x14047F170 arg 2)", "0x14047F170", "A1", "SANCTION"),
    ("sanction A1+0xE0 E0 ring", "0x14047F170", "A1.E0", "SANCTION"),
    ("sanction A1+0x18 object", "0x14047F170", "A1.18", "SANCTION"),
    ("trap action (A0 of 0x14079AFD0, commit)", "0x14079AFD0", "A0", "TRAP"),
    ("trap anime (A1 of 0x14079AFD0, commit)", "0x14079AFD0", "A1", "TRAP"),
    ("trap query (A0 of 0x14084DF20)", "0x14084DF20", "A0", "TRAP"),
]

# Live verdicts per identity, from recordmap_pin.py (captures under live2/pid407792; see the report).
PIN_STATUS = {
    "kick.P (kicker ap; rcx of 0x14083CC30)": "REFUTED as a G block start (0/12 captures); fields readable to +0x32E8; "
                                               "+0xAB8 interface object image-resident (12/12)",
    "G anime table entry (block start, G+0x19C8)": "CONFIRMED entries 0..20: one 0x3F40 block each (12/12); 21+ irregular",
    "G man table entry (block start + 0x32C, G+0x1DD8)": "CONFIRMED anime+0x32C for entries 0..20 (12/12)",
    "G slot table entry (block start + 0x10F0, G+0x25F8)": "CONFIRMED anime+0x10F0 for entries 0..19 (12/12); 20+ irregular",
    "G+0x180 carrier table entry (stride 0x74)": "table readable at [id]*0x74+0x6E (6/6); field semantics OPEN",
    "G+0x40 carrier record": "readable to +0x2954 (6/6); not the Clear unit's carrier (not equal)",
    "MI (G+0x270)": "NOT PINNED: no sanction/foul capture in live2/pid407792 or live2/pid161465",
    "ctx = corridor-angle A0 = Clear unit+8 = sanction A1": "extents readable (6/6); identity A0=A1=unit+8 INF (same "
                                                             "+0x18/+0xE0 pair read by both chains)",
    "E0 ring (ctx+0xE0)": "CONFIRMED count (E0+0x1900) in 0..8 and head (E0+0x1904) in 0..7 (6/6, count saturates at 8)",
    "ctx+0x18 object": "readable to +0x2601C+4 (6/6)",
    "Clear unit (r8 of 0x142109EB0)": "CONFIRMED +0 carrier ptr, +8 ctx ptr, +0x28 imageA ptr readable (5/5)",
    "carrier record (unit+0)": "CONFIRMED +0 u32 carrier id in 0..21 (5/5); NOT a G table entry (0/5)",
    "imageA (unit+0x28)": "CONFIRMED +0x31C1 danger byte in {0,1} (5/5)",
    "sanction A1 (0x14047F170 arg 2)": "NOT PINNED: no sanction capture",
    "sanction A1+0xE0 E0 ring": "NOT PINNED: no sanction capture (ring domain pinned via ctx+0xE0)",
    "sanction A1+0x18 object": "NOT PINNED: no sanction capture",
    "trap action (A0 of 0x14079AFD0, commit)": "NOT PINNED: no trap hook in hooks.cfg",
    "trap anime (A1 of 0x14079AFD0, commit)": "NOT PINNED: no trap hook in hooks.cfg",
    "trap query (A0 of 0x14084DF20)": "NOT PINNED: no trap hook in hooks.cfg",
}

MD = Cs(CS_ARCH_X86, CS_MODE_64)
MD.detail = True


def pdata_ends(image):
    """{entry VA: end VA} of the image's .pdata, from the shared reader (corrupt entries dropped)."""
    with open(image, "rb") as f:
        return pdata.Pdata(f).entries


def count_insns(va, end):
    return sum(1 for _ in MD.disasm(AN.rd(va, end - va), va))


def direct_calls(va, size):
    out = []
    for ins in MD.disasm(AN.rd(va, size * 15 + 16), va, count=size):
        if ins.mnemonic == "call" and ins.operands and ins.operands[0].type == X86_OP_IMM:
            out.append(ins.operands[0].imm)
    return out


def closure(seeds, ends, uni, maxn):
    """Direct-call closure of the seeds: {VA: instruction count}. Callees with no size source are left out."""
    sizes = {}
    todo = list(seeds)
    while todo and len(sizes) < maxn:
        va = todo.pop(0)
        if va in sizes:
            continue
        if va in ends:
            n = count_insns(va, ends[va])
        elif "0x%X" % va in uni:
            n = int(uni["0x%X" % va]["size"])
        else:
            continue
        sizes[va] = n
        todo.extend(t for t in direct_calls(va, n) if t not in sizes)
    return sizes


def analyze(args):
    AN.open_image(args.image)
    ends = pdata_ends(args.image)
    uni = sets.load_universe()
    seeds = [int(v, 16) for vs in CHAINS.values() for v in vs]
    size_of = closure(seeds, ends, uni, args.max)
    rows = []
    for chain, vs in CHAINS.items():
        for v in vs:
            va = int(v, 16)
            if va not in size_of:
                rows.append({"chain": chain, "va": v, "error": "no size source (no .pdata, not in ledger)"})
                continue
            r = AN.analyze_functions([dict(va=va, size=size_of[va], status="chain", subsystem=chain)],
                                     size_of)[0]
            r["chain"] = chain
            rows.append(r)
    flags = Counter(f.split("@")[0].split(":")[0] for r in rows if "flags" in r for f in r["flags"])
    out = {"image": os.path.basename(args.image), "closure_size": len(size_of), "flag_kinds": dict(flags),
           "rows": rows}
    with open(args.out, "w", encoding="utf8") as f:
        json.dump(out, f, indent=1)
        f.write("\n")
    print("closure", len(size_of), "functions; rows", len(rows), "; flag kinds", dict(flags.most_common()))


def off_num(s):
    return None if s.startswith("IDX") else int(s, 16)


def merge(args):
    an = json.load(open(args.analysis))
    pin = json.load(open(args.pin))
    fm = json.load(open(args.fieldmap))
    tops = {(r["chain"], r["va"]): r for r in an["rows"] if "fields" in r}
    records = []
    for identity, top, root, chain in RECORDS:
        row = tops.get((chain, top))
        if row is None:
            records.append({"identity": identity, "chain": chain, "top": top, "obj": root, "error": "no row"})
            continue
        fields = {}
        for f in row["fields"]:
            if f["obj"] != root:
                continue
            key = f["off"]
            e = fields.setdefault(key, {"off": key, "w": set(), "r": False, "wr": False, "kind": set(),
                                        "ptr": False})
            e["w"].update(f["w"])
            e["r"] = e["r"] or f["r"]
            e["wr"] = e["wr"] or f["wr"]
            e["kind"].update(f["kind"])
            e["ptr"] = e["ptr"] or bool(f.get("ptr"))
        flist = []
        for key in sorted(fields, key=lambda k: (k.startswith("IDX"), off_num(k) if off_num(k) is not None else 0)):
            e = fields[key]
            flist.append({"off": e["off"], "widths": sorted(e["w"]), "read": e["r"], "written": e["wr"],
                          "kinds": sorted(e["kind"]), "ptr": e["ptr"]})
        size_lb = max((off_num(e["off"]) + max(e["widths"] or [0]) for e in flist if off_num(e["off"]) is not None),
                      default=0)
        records.append({"identity": identity, "chain": chain, "top": top, "obj": root,
                        "size_lower_bound": size_lb, "fields": flist,
                        "pin": PIN_STATUS.get(identity, "NOT PINNED")})
    fm["recordmap"] = {
        "contract": "RecordMap v1 (tools/dev/translate/recordmap.py analyze + merge; live pins from "
                    "recordmap_pin.py). Record identity = (chain top level, analyzer object root). size_lower_bound "
                    "is the largest read/written offset+width; the true allocation size is OPEN unless stated.",
        "pins": {k: {"pass": v["pass"], "fail": v["fail"]} for k, v in pin.get("checks", {}).items()},
        "records": records,
    }
    with open(args.fieldmap, "w", encoding="utf8") as f:
        json.dump(fm, f, indent=1)
        f.write("\n")
    print("records", len(records), "fields", sum(len(r.get("fields", [])) for r in records))
    if args.report:
        with open(args.report, "w", encoding="utf8") as f:
            for r in records:
                if "error" in r:
                    f.write("%s | %s %s | %s\n" % (r["identity"], r["chain"], r["top"], r["error"]))
                    continue
                f.write("%s | %s %s %s | size>=%#x | fields %d | %s\n" % (
                    r["identity"], r["chain"], r["top"], r["obj"], r["size_lower_bound"], len(r["fields"]), r["pin"]))


def main(argv=None):
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("analyze")
    a.add_argument("--image", required=True)
    a.add_argument("--out", required=True)
    a.add_argument("--max", type=int, default=600)
    m = sub.add_parser("merge")
    m.add_argument("--analysis", required=True)
    m.add_argument("--pin", required=True)
    m.add_argument("--fieldmap", required=True)
    m.add_argument("--report")
    args = ap.parse_args(argv)
    if args.cmd == "analyze":
        analyze(args)
    else:
        merge(args)


if __name__ == "__main__":
    main()
