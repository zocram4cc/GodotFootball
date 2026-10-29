#!/usr/bin/env python3
"""closure.py: make a function closure regenerable from the image, with no Ghidra bridge.

Given root VAs, walks their direct callees (CALL rel32 and tail JMP to another function start) and, for every
function in the closure:
  1. a universe row in tools/dev/ledger/universe.json (the rows universe.py crawls from the bridge are kept as they
     are; a missing row is decoded from the image with capstone, bounded by the .pdata RUNTIME_FUNCTION table);
  2. a spec from the standard spec generator (specgen, pilot mode: every callee is in the closure, so no stubs),
     written to tools/dev/diffharness/specs/<SPEC_SET>/<VA>.json, the directory a record of that set translates under.
The set file (the seed order of the specs) is tools/dev/diffharness/specgen/sets/closure-<TAG>.txt.

After this, gen_translated.py translates the closure under the same universe the spec set gives it, so a record of
that set reproduces its committed translation byte for byte.

Usage (run with the harness Python, which has capstone):
  closure.py --image IMG --spec-set core --tag endclear 0x142109EB0 0x142108630 0x14063A290 0x14063A490 0x14063D2F0
  closure.py --image IMG --spec-set core --tag endclear --dry-run ...   # report the closure, write nothing
  closure.py --image IMG --spec-set core --tag endclear --roots-file FILE  # one VA per line

Rows and specs are a pure function of (image, roots, spec set, tag): re-running is idempotent.
"""
import argparse
import json
import os
import shutil
import sys
import tempfile

import capstone
from capstone.x86 import X86_OP_IMM

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
DIFFH = os.path.join(REPO, "tools", "dev", "diffharness")
UNIVERSE = os.path.join(REPO, "tools", "dev", "ledger", "universe.json")
sys.path.insert(0, DIFFH)
from specgen import sets as SETS  # noqa: E402
from specgen.__main__ import generate as specgen_generate  # noqa: E402

import stubs as stub_table  # noqa: E402  (the one stub table: a model is never a closure member)
MODEL_VAS = set(stub_table.models(stub_table.load()))

from pdata import Pdata  # noqa: E402
DIRECT_KINDS = ("direct", "tail")  # the callee kinds the translator links as pes_fn_ calls
MAX_MEMBERS = 4000


def hexva(s):
    return "0x%X" % int(s, 16)


class ClosurePdata(Pdata):
    """The .pdata map plus a capstone decoder for the universe rows of functions the bridge never saw."""

    def __init__(self, img):
        super().__init__(img)
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        self.md.detail = True

    def body(self, va):
        """The decoded instructions of the function at va (None if va is not a function boundary), and its end.
        A .pdata function is exactly its range. A leaf or thunk with no .pdata entry (x64 needs none) ends at its
        first ret or tail jump out of its window with no forward branch still pending, never past the next .pdata start."""
        end = self.end_of(va)
        if end is not None:
            return list(self.md.disasm(self.read(va, end - va), va)), end
        if not self.is_boundary(va):
            return None, None
        limit = self.next_start(va)
        insns, pending = [], va
        for ins in self.md.disasm(self.read(va, limit - va), va):
            insns.append(ins)
            ops = ins.operands
            imm = ops[0].imm if ops and ops[0].type == X86_OP_IMM else None
            if imm is not None and ins.mnemonic.startswith("j") and va <= imm < limit:
                pending = max(pending, imm)
            leaves = ins.mnemonic == "ret" or (ins.mnemonic == "jmp" and imm is not None and not va <= imm < limit)
            if leaves and ins.address + ins.size > pending:
                break
        return insns, insns[-1].address + insns[-1].size

    def decode_row(self, va, tag):
        """A universe row for the function at va, decoded from the image; None if va is not a function boundary."""
        insns, end = self.body(va)
        if insns is None:
            return None
        callees, indirect = {}, []
        for ins in insns:
            ops = ins.operands
            imm = ops[0].type == X86_OP_IMM if ops else False
            if ins.mnemonic == "call":
                if imm:
                    callees.setdefault(ops[0].imm, "direct")
                else:
                    indirect.append({"site": "%X" % ins.address, "text": ("%s %s" % (ins.mnemonic, ins.op_str))[:80]})
            elif ins.mnemonic == "jmp" and imm and not va <= ops[0].imm < end and self.is_boundary(ops[0].imm):
                callees.setdefault(ops[0].imm, "tail")
        callees = {t: k for t, k in callees.items() if t != va}
        return {"name": "FUN_%08x" % va, "size": len(insns), "subsystem": tag, "how": "closure",
                "roots": [tag], "path": [], "walked": True, "boundary": None,
                "leaf": not callees and not indirect, "source": "pdata", "scope": "in",
                "callees": {"%08X" % t: k for t, k in sorted(callees.items())},
                "indirect": indirect, "tables": 0, "vtables": [], "protected": False, "error": "",
                "unresolved": False, "overspill": None}


def crawl(roots, universe, pdata, tag):
    """Walk the closure from roots, in discovery order. Adds the missing rows to universe (in place).
    Returns (members in order, rows added)."""
    members, added, seen = [], [], set()
    queue = list(roots)
    while queue:
        va = queue.pop(0)
        if va in seen:
            continue
        seen.add(va)
        if va in MODEL_VAS:
            continue  # the table models it (CRT, engine, import thunk): never a member, never walked
        if len(members) >= MAX_MEMBERS:
            raise SystemExit("closure: more than %d members (a root reaches the whole engine); stop it" % MAX_MEMBERS)
        key = "%08X" % va
        if key not in universe:
            row = pdata.decode_row(va, tag)
            if row is None:
                raise SystemExit("closure: %s is called but is not a .pdata function start, and has no universe row"
                                 % hexva(key))
            universe[key] = row
            added.append(key)
        members.append(va)
        for c, kind in sorted(universe[key]["callees"].items(), key=lambda kv: kv[0]):
            if kind in DIRECT_KINDS and int(c, 16) not in seen:
                queue.append(int(c, 16))
    return members, added


def load_universe():
    with open(UNIVERSE, encoding="utf8") as f:
        return json.load(f)


def save_universe(doc):
    with open(UNIVERSE, "w") as f:
        json.dump(doc, f, indent=1, sort_keys=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--image", required=True, help="the unpacked PES image (file offset = VA - 0x140000000)")
    ap.add_argument("--spec-set", required=True, help="spec subdirectory the closure translates under (e.g. core)")
    ap.add_argument("--tag", required=True, help="closure tag: names the set file and the subsystem of new rows")
    ap.add_argument("--roots-file", help="one root VA per line (added after the VAs given on the command line)")
    ap.add_argument("--subsystem", help="subsystem of the rows this run adds (default: the tag)")
    ap.add_argument("--regenerate", action="store_true",
                    help="rewrite the committed specs of members too (default: only missing specs are written)")
    ap.add_argument("--dry-run", action="store_true", help="report the closure; write nothing")
    ap.add_argument("roots", nargs="*")
    args = ap.parse_args(argv)
    roots = [int(r.split("#")[0].strip(), 16) for r in args.roots]
    if args.roots_file:
        with open(args.roots_file, encoding="utf8") as f:
            roots += [int(line.split("#")[0].strip(), 16) for line in f if line.split("#")[0].strip()]
    if not roots:
        ap.error("no root VAs")
    doc = load_universe()
    universe = doc["functions"]
    with open(args.image, "rb") as img:
        pdata = ClosurePdata(img)
        # Pre-existing rows keep their subsystem (the layer's directory); new rows take --subsystem, else the tag.
        members, added = crawl(roots, universe, pdata, args.subsystem or args.tag)
    rows = [("%08X" % va, SETS.DEFAULT_STATUS, universe["%08X" % va]["subsystem"], "seed" if va in roots else "callee")
            for va in members]
    print("closure %s: %d members from %d roots (%d universe rows added, %d already in the universe)"
          % (args.tag, len(members), len(roots), len(added), len(members) - len(added)))
    if args.dry_run:
        for key, status, sub, role in rows:
            print("  0x%s\t%s\t%s" % (key, sub, role))
        return 0
    doc["functions"] = universe
    doc.setdefault("meta", {}).setdefault("closures", {})[args.tag] = {
        "roots": ["0x%08X" % r for r in roots], "members": len(members)}
    save_universe(doc)
    set_path = os.path.join(DIFFH, "specgen", "sets", "closure-%s.txt" % args.tag)
    SETS.write_set(set_path, [(("0x" + key), status, sub, role) for key, status, sub, role in rows])
    spec_dir = os.path.join(DIFFH, "specs", args.spec_set)
    os.makedirs(spec_dir, exist_ok=True)
    out = tempfile.mkdtemp(prefix="closure-%s-" % args.tag, dir=os.environ.get("GF_CLOSURE_TMP") or None)
    try:
        specgen_generate(argparse.Namespace(set=set_path, mode="pilot", image=args.image, out_dir=out,
                                            pilot_set=None))
        kept = differ = 0
        for va in members:
            gen_path = os.path.join(out, "%X.json" % va)
            dst = os.path.join(spec_dir, "%X.json" % va)
            if os.path.exists(dst) and not args.regenerate:
                # A committed spec is the record's input (its seed, regions and stub refs). Only a missing spec is written.
                kept += 1
                with open(gen_path, encoding="utf8") as g, open(dst, encoding="utf8") as c:
                    differ += json.load(g) != json.load(c)
                continue
            shutil.copyfile(gen_path, dst)
        if kept:
            print("closure %s: kept %d committed specs (%d differ from the generator's output; --regenerate rewrites them)"
                  % (args.tag, kept, differ))
    finally:
        shutil.rmtree(out, ignore_errors=True)
    print("closure %s: specs -> %s (%d), set file %s, universe %s"
          % (args.tag, spec_dir, len(members), os.path.relpath(set_path, REPO), os.path.relpath(UNIVERSE, REPO)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
