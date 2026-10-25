"""Spec generator CLI: python3 -m specgen (run from tools/dev/diffharness).

  select    --subsystem TAG [TAG ...] --write-set FILE
            the ledger's pure rows of the TAGs, closed over their pure callees (one union; each row carries subsystem and role)
  generate  --set FILE --mode pilot|impure --image IMG --out-dir DIR [--pilot-set FILE]
            one spec per VA of a set file, read from the image

pilot: every callee is in the set (no stubs), the AI pilot's conventions. impure: the out-of-set callees are classified
into stub models and blockers, and the G accessor closure is laid out from the image.
"""
import argparse
import json
import os
import sys

from . import analyze as AN
from . import classify as C
from . import impure as I
from . import rules as R
from . import sets
from . import spec as S


def ledger_callees(uni, va):
    """Direct callees of va in the ledger's walk (the set the impure specs were built from)."""
    return [int(c, 16) for c in uni[va]["callees"]]


def select(args):
    rows = sets.closure_rows(args.subsystem)
    sets.write_set(args.write_set, rows)
    print("selected", len(rows), "rows of", ", ".join(args.subsystem), "closed over pure callees ->", args.write_set)


def generate(args):
    uni = sets.load_universe()
    rows = sets.read_set(args.set)
    impure = args.mode == "impure"
    AN.open_image(args.image)
    if impure:
        pilot = [va for va, _ in sets.read_set(args.pilot_set)]
        C.configure({int(va, 16): uni[va]["size"] for va in pilot})
        # run-real and stub callees of the body are classified first: their sizes join the analysis
        for va, _ in rows:
            for t in sorted(set(C.direct_calls(int(va, 16)))):
                C.classify(t)
        size_of = {int(va, 16): uni[va]["size"] for va in pilot}
        for k, n in C.RUNREAL_SIZE.items():
            size_of.setdefault(k, n)
        for va, _ in rows:
            size_of[int(va, 16)] = uni[va]["size"]
    else:
        size_of = {int(va, 16): uni[va]["size"] for va, _ in rows}
    fns = [dict(va=int(va, 16), size=uni[va]["size"],
                status="impure-sample" if impure else status,
                subsystem="impure-sample" if impure else uni[va]["subsystem"])
           for va, status in rows]
    analysis = AN.analyze_functions(fns, size_of)
    specs = S.build_all(analysis)
    os.makedirs(args.out_dir, exist_ok=True)
    for spec, (va, _status), fn in zip(specs, rows, analysis):
        n = int(va, 16)
        if impure:
            I.mark(spec, I.harness_fields(n, ledger_callees(uni, va)))
            R.closure_fix(spec, n)
            R.drop_dangling(spec)
        else:
            R.case_varied_globals(spec, fn)
            R.audit_outputs(spec, args.image)
        with open(os.path.join(args.out_dir, "%X.json" % n), "w", encoding="utf8") as f:
            json.dump(spec, f, indent=1)
            f.write("\n")
    print("generated", len(specs), "specs ->", args.out_dir)


def main(argv=None):
    ap = argparse.ArgumentParser(prog="python3 -m specgen", description="Spec generator for the differential harness.")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sel = sub.add_parser("select", help="write a set file from the ledger's pure rows")
    sel.add_argument("--subsystem", required=True, nargs="+")
    sel.add_argument("--write-set", required=True)
    sel.set_defaults(run=select)
    gen = sub.add_parser("generate", help="write one spec per VA of a set file")
    gen.add_argument("--set", required=True)
    gen.add_argument("--mode", choices=("pilot", "impure"), required=True)
    gen.add_argument("--image", required=True, help="the unpacked PES image (PES_EXE)")
    gen.add_argument("--out-dir", required=True)
    gen.add_argument("--pilot-set", help="impure mode: the pilot set file (its callees are 'pilot')")
    gen.set_defaults(run=generate)
    args = ap.parse_args(argv)
    if args.cmd == "generate" and args.mode == "impure" and not args.pilot_set:
        ap.error("impure mode needs --pilot-set")
    args.run(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
