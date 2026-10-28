#!/usr/bin/env python3
"""check_closure_stubs.py: a hook's spec carries the stub models of its whole closure, not only its callee rows.

Usage: python3 tools/dev/diffharness/check_closure_stubs.py --image PES_EXE
ref_restart_cmd_A39A80 (0x140A39A80), with no callee rows, must still carry the models its closure calls:
0x14159F3E0 (__security_check_cookie, reached through the closure) and 0x1415A1780 (sqrtf). Without them the
translated closure calls the cookie check through the dispatch table and aborts (PES_DISPATCH_UNTRANSLATED).
Exit 0 green, 1 red.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from specgen import analyze as AN  # noqa: E402
from specgen import classify as C  # noqa: E402
from specgen import impure as I  # noqa: E402
from specgen import sets  # noqa: E402

HOOK = 0x140A39A80
WANT = {"14159F3E0", "1415A1780"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--image", required=True)
    args = ap.parse_args()
    uni = sets.load_universe()
    pilot = [va for va, _ in sets.read_set(os.path.join(HERE, "specgen", "sets", "pilot.txt"))]
    AN.open_image(args.image)
    C.configure({int(va, 16): uni[va]["size"] for va in pilot})
    got = set(I.harness_fields(HOOK, [])["stub_refs"])
    missing = sorted(WANT - got)
    if missing:
        print("RED: stub_refs %s, missing %s" % (sorted(got), missing))
        return 1
    print("GREEN: stub_refs %s" % sorted(got))
    return 0


if __name__ == "__main__":
    sys.exit(main())
