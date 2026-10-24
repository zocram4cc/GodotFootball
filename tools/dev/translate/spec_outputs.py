#!/usr/bin/env python3
"""spec_outputs.py: a pilot spec must check every return register its function
can leave with. xmm0 is checked when the body writes it (float return); eax or
rax when the body writes them (integer or pointer return). A spec that omits
one passes whatever that register holds, so it proves less than its name says.

The audit decodes each function from the user's image with the translator's own
decoder and reports the specs that miss a register the body writes. --write adds
the missing outputs (ret_xmm0, ret_eax or ret_rax) to the spec files.

Usage:
  spec_outputs.py --image IMG [--spec-dir DIR] [--write]
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xlate  # noqa: E402

DEF_SPEC_DIR = os.path.normpath(
    os.path.join(HERE, "..", "diffharness", "specs", "pilot"))
# Mnemonics that never write their first operand.
NO_WRITE = {"CMP", "TEST", "COMISS", "UCOMISS", "PUSH", "RET", "NOP", "CALL"}
# Mnemonics that write rax with no register operand, and the width they write.
IMPLICIT_RAX = {"CDQE": 64, "CWDE": 32, "CBW": 32, "MUL": 64, "IMUL": 64,
                "DIV": 64, "IDIV": 64}
ONE_OPERAND_IMPLICIT = {"MUL", "IMUL", "DIV", "IDIV"}


def writes(lines):
    """(writes_xmm0, rax_width or None) over one decoded body."""
    xmm0 = False
    rax = None
    for _, m, ops in lines:
        if m.startswith("J") or m in NO_WRITE:
            continue
        if m in IMPLICIT_RAX and (m not in ONE_OPERAND_IMPLICIT
                                  or len(ops) == 1):
            rax = max(rax or 0, IMPLICIT_RAX[m])
        if not ops:
            continue
        dst = ops[0].strip().lower()
        if dst == "xmm0":
            xmm0 = True
        if xlate.PARENT.get(dst) == "rax":
            rax = max(rax or 0, 64 if dst == "rax" else 32)
    return xmm0, rax


def missing_outputs(spec, lines):
    """The output entries the spec lacks for registers the body writes."""
    xmm0, rax = writes(lines)
    have = {o.get("reg") for o in (spec.get("outputs") or {}).values()}
    need = {}
    if xmm0 and "xmm0" not in have:
        need["ret_xmm0"] = {"reg": "xmm0", "enc": "f32"}
    if rax is not None and not ({"eax", "rax"} & have):
        if rax == 64:
            need["ret_rax"] = {"reg": "rax"}
        else:
            need["ret_eax"] = {"reg": "eax"}
    return need


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--spec-dir", default=DEF_SPEC_DIR)
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    missing = 0
    for fn in sorted(os.listdir(a.spec_dir)):
        if not fn.endswith(".json"):
            continue
        path = os.path.join(a.spec_dir, fn)
        text = open(path, encoding="utf-8").read()
        spec = json.loads(text)
        lines = xlate.capstone_decode(a.image, int(spec["va"], 16))
        need = missing_outputs(spec, lines)
        if not need:
            continue
        missing += 1
        print("%s %s missing %s" % (spec["va"], fn, " ".join(sorted(need))))
        if a.write:
            spec.setdefault("outputs", {}).update(need)
            out = json.dumps(spec, indent=1)
            if text.endswith("\n"):
                out += "\n"
            with open(path, "w", encoding="utf-8") as f:
                f.write(out)
    print("specs missing a written return register: %d" % missing)
    return 0


if __name__ == "__main__":
    sys.exit(main())
