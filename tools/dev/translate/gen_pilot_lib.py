#!/usr/bin/env python3
"""gen_pilot_lib.py: build-time pilot library generator (cutover path).

Usage: gen_pilot_lib.py --image IMG --out-dir DIR --vas VA... [--spec-dir D]
For each VA: translate (capstone from the image; no bridge cache), write
pes_fn_<va>.cpp into DIR. Also writes pes_pilot.h declaring every TU.
Without --image (or on any failure) writes NOTHING and exits 0: the
caller (CMake) then skips the pespilot target so the build stays green
without PES_EXE. The caller must define PES_PILOT_AVAILABLE when TUs were
generated; GF call sites gate on it (PES path only, GF mode untouched).
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default="")
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--vas", nargs="*", default=[])
    ap.add_argument("--spec-dir", default="")
    args = ap.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)
    if not args.image or not os.path.isfile(args.image):
        return 0
    try:
        import xlate
        pilot = set()
        if args.spec_dir and os.path.isdir(args.spec_dir):
            for f in os.listdir(args.spec_dir):
                if f.endswith(".json"):
                    pilot.add("0x" + f[:-5].upper())
        else:
            pilot = {"0x" + v.lower().replace("0x", "").upper()
                     for v in args.vas}
        decls = []
        for va in args.vas:
            full = "0x" + va.lower().replace("0x", "").upper()
            src, _ = xlate.translate_one(args.image, "", full, pilot)
            if src is None:
                return 0
            with open(os.path.join(args.out_dir,
                                   "pes_fn_%s.cpp" % full[2:]), "w") as f:
                f.write("\n".join(src) + "\n")
            decls.append("void pes_fn_%s(PesState& s);" % full[2:])
        # Indirect calls in the translated TUs dispatch through this table: the lib's own translated set, so a
        # target outside it aborts with its address (pes_dispatch_missing), never silently.
        import gen_dispatch
        gen_dispatch.write(args.out_dir, args.vas)
        with open(os.path.join(args.out_dir, "pes_pilot.h"), "w") as f:
            f.write("// Generated at build time from the user's image.\n"
                    "#pragma once\nstruct PesState;\n" +
                    "\n".join(decls) + "\n")
    except Exception:  # noqa: BLE001 - build must stay green
        return 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
