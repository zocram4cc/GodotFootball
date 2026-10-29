#!/usr/bin/env python3
"""check_closure_stubs.py: a hook's spec carries the stub models of its whole closure, not only its callee rows.

Usage: python3 tools/dev/diffharness/check_closure_stubs.py --image PES_EXE
ref_restart_cmd_A39A80 (0x140A39A80), with no callee rows, must still carry the models its closure calls:
0x14159F3E0 (__security_check_cookie, reached through the closure) and 0x1415A1780 (sqrtf). Without them the
translated closure calls the cookie check through the dispatch table and aborts (PES_DISPATCH_UNTRANSLATED).
restart_44C880 (0x14044C880) reaches the same two models only through its switch case bodies, which a plain
decode of its body does not reach (a computed jump): the closure walk must follow the ledger's edges there.
Exit 0 green, 1 red.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from specgen import analyze as AN  # noqa: E402
from specgen import classify as C  # noqa: E402
from specgen import sets  # noqa: E402

CASES = [
    (0x140A39A80, {"14159F3E0", "1415A1780"}),  # ref_restart_cmd: the closure's own calls
    (0x14044C880, {"14159F3E0", "1415A1780"}),  # restart_44C880: the same models, reached through switch cases
]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--image", required=True)
    args = ap.parse_args()
    uni = sets.load_universe()
    pilot = [va for va, _ in sets.read_set(os.path.join(HERE, "specgen", "sets", "pilot.txt"))]
    AN.open_image(args.image)
    C.configure({int(va, 16): uni[va]["size"] for va in pilot})
    red = []
    for hook, want in CASES:
        got = {"%X" % s for s in C.reach_stubs(hook)[0]}  # every model the closure reaches (the table decides which)
        missing = sorted(want - got)
        if missing:
            red.append("RED %#x: closure models %s, missing %s" % (hook, sorted(got), missing))
        else:
            print("GREEN %#x: closure models %s" % (hook, sorted(got)))
    for line in red:
        print(line)
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main())
