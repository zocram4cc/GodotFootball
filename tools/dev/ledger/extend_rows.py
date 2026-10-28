#!/usr/bin/env python3
"""extend_rows.py: add ledger universe rows for VAs the bridge crawl never reached (no bridge needed).

Usage: python3 tools/dev/ledger/extend_rows.py --image PES_EXE --rows extend_rows.json VA [VA ...]
  --rows  {"0xVA": {"how", "scope", "size", "subsystem"}}: the row's metadata (sizes are the .pdata extents).
  Callees are the image's direct CALL targets (capstone, specgen.classify.direct_calls), how = "direct": the
  same rule universe.py applies to a bridge-listed body. A VA already in universe.json is reported and skipped.
  The file is rewritten with the same layout (json indent=1, keys sorted as before).
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "diffharness"))
from specgen import analyze as AN  # noqa: E402
from specgen import classify as C  # noqa: E402

UNIVERSE = os.path.join(HERE, "universe.json")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--image", required=True)
    ap.add_argument("--rows", required=True)
    ap.add_argument("vas", nargs="+")
    args = ap.parse_args()
    with open(args.rows, encoding="utf8") as f:
        meta = json.load(f)
    with open(UNIVERSE, encoding="utf8") as f:
        doc = json.load(f)
    fns = doc["functions"]
    ordered = list(fns) == sorted(fns)
    AN.open_image(args.image)
    added = []
    for va in args.vas:
        key = "%X" % int(va, 16)
        if key in fns:
            print("skip %s: already in universe.json" % key)
            continue
        m = meta["0x" + key]
        callees = sorted(set(C.direct_calls(int(key, 16))))
        fns[key] = {"boundary": None, "callees": {"%X" % t: "direct" for t in callees}, "how": m["how"],
                    "indirect": [], "scope": m["scope"], "size": m["size"], "subsystem": m["subsystem"]}
        added.append((key, len(callees)))
    if ordered:
        doc["functions"] = {k: fns[k] for k in sorted(fns)}
    with open(UNIVERSE, "w", encoding="utf8") as f:
        json.dump(doc, f, indent=1)
    for key, n in added:
        print("added %s: %d direct callees" % (key, n))
    print("universe.json: %d rows" % len(doc["functions"]))


if __name__ == "__main__":
    main()
