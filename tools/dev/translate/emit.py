#!/usr/bin/env python3
"""Emit one C++ translation unit per pilot function (build tree only).

Reads:  asm cache (bridge text, one file per VA) OR capstone re-decode of the
        user's image (identical bytes; capstone is used when --asm-dir is
        absent, so the translator never needs the bridge at build time).
        Image bytes for every absolute [0x14..] operand (constants baked as
        VA-cited constexpr, never committed).
Writes: $OUT_DIR/pes_fn_<va>.cpp + pes_pilot.h (all generated, build tree).
Deterministic: sorted labels/tables, fixed float formatting (%a hex floats).

Opcode coverage (the pilot's 83 mnemonics, 249 operand forms): integer ALU
  (MOV/LEA/ADD/SUB/CMP/TEST/AND/OR/XOR/INC/DEC/NEG/SHL/SHR/SAR/IMUL/MUL/
  DIV/IDIV/BT/BTC/BTR/BTS/NOT/NOT/SBB/ADC/XCHG/CDQE/MOVSX/MOVZX/MOVSXD/
  CWD/CDQ/CQO), stack (PUSH/POP with LEA-RSP frame idioms), SSE scalar
  (MOVSS/ADDSS/SUBSS/MULSS/DIVSS/COMISS/UCOMISS/MAXSS/MINSS/MOVD/CVTTSS2SI/
  CVTSI2SS/CVTSI2SS/CVTSS2SD/CVTSD2SS), packed transfers (MOVAPS/MOVUPS/MOVSD/
  MOVAPD/MOVUPD/ANDPS/ORPS/XORPS/CVTPS2PD/CVTPD2PS/CVTDQ2PS/CVTPS2DQ),
  branches (all Jcc + JMP reg = switch dispatch + JMP rel), CMOVcc/SETcc,
  CALL (intra-set: direct C++ call; the return value lands in rax/xmm0),
  RET (multiple returns), NOP/LEA-address math.

Bytes beat the decompile: signedness from Jcc/CMOVcc/SETcc opcodes, float
  polarity from COMISS vs UCOMISS + JP presence, CMOV (not branch) for
  conditional moves, switch tables from image bytes (not decompile guesses).
"""
import argparse
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lift import canon_reg, parse_mem, GPR_INDEX, SUB, BASE  # noqa: E402

JCC = {"jz": "ZF", "jnz": "NZF", "jc": "CF", "jnc": "NCF", "ja": "NCF_NZF",
       "jbe": "CF_ZF", "jl": "SFneOF", "jge": "SFeqOF", "jle": "ZF_SFneOF",
       "jg": "NZF_SFeqOF", "js": "SF", "jns": "NSF", "jp": "PF", "jnp": "NPF",
       "jo": "OF", "jno": "NOF"}


def va_to_off(va):
    return va - BASE


def read_image(image_path, va, n):
    with open(image_path, "rb") as f:
        f.seek(va_to_off(va))
        blob = f.read(n)
    if len(blob) != n:
        raise SystemExit("translate: image unreadable at %#x" % va)
    return blob


def decode_asm(asm_dir, va):
    names = ["%s.json" % va[2:], "%s.json" % va[2:].upper(),
             "%s.json" % va[2:].lower()]
    for nm in names:
        p = os.path.join(asm_dir, nm)
        if os.path.exists(p):
            raw = open(p, encoding="utf-8", errors="replace").read()
            return [l.strip() for l in raw.splitlines()
                    if l.strip() and __import__("re").match(
                        r'^[0-9a-f]+:', l.strip())]
    return None
