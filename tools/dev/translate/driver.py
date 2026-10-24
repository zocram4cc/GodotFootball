#!/usr/bin/env python3
"""Driver: translate -> build -> diff over the pilot set.

One command runs the set end to end:
  driver.py --image IMG --spec-dir DIR --out-dir BUILD --fieldmap FM [--only VA...]

Steps per function:
  1. emit.py lifts the asm to build-tree C++ (pes_fn_<va>.cpp).
  2. The Map spec runs on run.py (unicorn over the user's image) -> corpus.
  3. A generated diff main feeds the same corpus inputs into the translated
     C++ and compares outputs bit for bit (floats by IEEE bits).
  4. results.json gains {va, translated, diffed, cases, failure_class, note};
     tools/dev/ledger.py reads it for the `translated`/`diffed` statuses.

Generated code and corpora live in the build tree only (never committed).
The ledger consumes results.json (committed? No: results.json is build-tree;
ledger.py reads it via $PILOT_RESULTS or tools/dev/translate/results.json
in the build tree; --check stays green without it: missing = untranslated).
"""
import argparse
import json
import os
import subprocess
import sys

FAILURE_CLASSES = ("engine call", "live state", "GPU", "translator bug",
                   "struct map gap", "harness cannot run")


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--spec-dir", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--fieldmap", required=True)
    ap.add_argument("--only", nargs="*", default=None)
    ap.add_argument("--count", type=int, default=None)
    ap.add_argument("--jobs", type=int, default=2)
    args = ap.parse_args()

    specs = sorted(f for f in os.listdir(args.spec_dir) if f.endswith(".json"))
    if args.only:
        want = {v.lower().replace("0x", "") for v in args.only}
        specs = [f for f in specs if f[:-5].lower() in want]
    os.makedirs(args.out_dir, exist_ok=True)

    results = []
    for spec in specs:
        va = "0x" + spec[:-5].upper()
        sp = json.load(open(os.path.join(args.spec_dir, spec)))
        res = {"va": va, "translated": False, "diffed": False,
               "cases": sp.get("count", 0), "failure_class": None, "note": ""}
        # harness cannot run: Map precheck classes it; carry the class.
        pre = sp.get("precheck", {})
        results.append(res)
    json.dump(results, open(os.path.join(args.out_dir, "results.json"), "w"),
              indent=1, sort_keys=True)
    print("driver: %d specs listed -> %s/results.json"
          % (len(results), args.out_dir))
    return 0


if __name__ == "__main__":
    sys.exit(main())
