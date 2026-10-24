#!/usr/bin/env python3
"""translate.py: asm (+image) -> C++ for one pilot function.

Usage: translate.py --image IMG --asm ASMFILE --va 0x.. --out OUT.cpp
  [--pilot VAS...] [--stack-size N]
Reads bridge asm text (or capstone fallback when ASMFILE is missing:
capstone decodes the image bytes at VA up to the next RET with no
unresolved branch target -- the pilot's bodies are all straight-line
with internal branches, closed by RET).

Emits a self-contained TU defining pes_fn_<va>(PesState&). Image constants
are baked as VA-cited constexpr (build tree only, never committed).
"""
import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emitter import Emitter  # noqa: E402
from emit_fn import cc_of, imm_val, is_imm  # noqa: E402
from lift import GPR_INDEX, SUB  # noqa: E402


def split_operands(s):
    if not s.strip():
        return []
    parts, depth, cur = [], 0, ""
    for ch in s:
        if ch == "[":
            depth += 1
        elif ch == "]":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += ch
    parts.append(cur.strip())
    return parts


def parse_lines(text):
    out = []
    for l in text.splitlines():
        l = l.strip()
        m = re.match(r'^([0-9a-f]+):\s*([A-Z0-9]+)\s*(.*)$', l)
        if m:
            out.append((int(m.group(1), 16), m.group(2),
                        split_operands(m.group(3))))
    return out


def capstone_decode(image_path, va):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    with open(image_path, "rb") as f:
        f.seek(va - 0x140000000)
        code = f.read(0x2000)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = False
    out = []
    for ins in md.disasm(code, va):
        out.append((ins.address, ins.mnemonic.upper(),
                    split_operands(ins.op_str))
        if ins.mnemonic == "ret":
            break
    return out
