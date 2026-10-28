#!/usr/bin/env python3
"""check_runnable_extra.py: a hook's out-of-set callees are judged by their own closure, not by a shared budget.

Usage: python3 tools/dev/translate/check_runnable_extra.py --image PES_IMAGE
restart_driver_445E90 (0x140445E90) calls 20-odd out-of-set functions. Its 9-byte leaf 0x140440FA0 (test/js/cmp/jae,
then a RET: no direct call) must be runnable. The walk that found it used to stop at a shared budget and mark every
unseen seed "closure larger than N", so the leaf was refused for its neighbours' size. Exit 0 green, 1 red.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "diffharness"))
import driver  # noqa: E402
from specgen import analyze as AN  # noqa: E402
from specgen import classify as C  # noqa: E402
from specgen import impure as I  # noqa: E402
from specgen import sets  # noqa: E402

HOOK = 0x140445E90
LEAF = "0x140440FA0"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--image", required=True)
    args = ap.parse_args()
    uni = sets.load_universe()
    pilot = [va for va, _ in sets.read_set(os.path.join(HERE, "..", "diffharness", "specgen", "sets", "pilot.txt"))]
    AN.open_image(args.image)
    C.configure({int(va, 16): uni[va]["size"] for va in pilot})
    rows = json.load(open(os.path.join(HERE, "..", "ledger", "universe.json")))["functions"]
    callees = [int(c, 16) for c in rows["%X" % HOOK]["callees"]]
    spec = {"harness": I.harness_fields(HOOK, callees)}
    extra, blocked = driver.runnable_extra(spec, args.image)
    if LEAF in blocked:
        print("RED: %s blocked: %s" % (LEAF, blocked[LEAF][:160]))
        return 1
    if LEAF not in extra:
        print("RED: %s neither runnable nor blocked" % LEAF)
        return 1
    print("GREEN: %s runnable (%d runnable, %d blocked for %s)" % (LEAF, len(extra), len(blocked), hex(HOOK)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
