#!/usr/bin/env python3
"""emit_fn.py: one asm listing + image -> C++ function body text.

The Emitter (emitter.py) owns operand printers + image constants; this
module owns per-mnemonic emission. Import cycle avoided: emit_fn imports
Emitter lazily inside functions (emitter imports nothing from here).
"""
import re
import struct

MNEM_MAP = {
    "JZ": "z", "JE": "z", "JNZ": "nz", "JNE": "nz",
    "JC": "c", "JB": "c", "JNAE": "c", "JNC": "nc", "JAE": "nc",
    "JNB": "nc", "JA": "a", "JNBE": "a", "JBE": "be", "JNA": "be",
    "JL": "l", "JNGE": "l", "JGE": "ge", "JNL": "ge",
    "JLE": "le", "JNG": "le", "JG": "g", "JNLE": "g",
    "JS": "s", "JNS": "ns", "JP": "p", "JPE": "p",
    "JNP": "np", "JPO": "np", "JO": "o", "JNO": "no",
}


def cc_of(mnem):
    m = mnem.upper()
    if m.startswith("J"):
        return MNEM_MAP.get(m)
    if m.startswith("CMOV"):
        return MNEM_MAP.get("J" + m[4:])
    if m.startswith("SET"):
        return MNEM_MAP.get("J" + m[3:])
    return None


def imm_val(s):
    s = s.strip().lower()
    if s.startswith("-0x"):
        return -int(s[1:], 16)
    if s.startswith("0x"):
        return int(s, 16)
    return int(s, 0)


def is_imm(s):
    try:
        imm_val(s)
        return True
    except (ValueError, AttributeError):
        return False
