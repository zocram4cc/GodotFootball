#!/usr/bin/env python3
"""emit_core.py: per-mnemonic emission. emit_one(em, addr, mnem, ops, ctx).

ctx: {"labels": set(addrs), "addrs": set(all), "pilot": set(vas),
      "switch": per-JMP-reg resolution info filled by prepass}.
Returns list[str] of C++ statements (without the label line; the driver
adds `L_<addr>:` for addresses in labels).
"""
import struct

from emit_fn import cc_of, imm_val, is_imm
from lift import GPR_INDEX, SUB
from lift import SUB_SHIFT

XMM = {"xmm%d" % i for i in range(16)}


def width_of_reg(n):
    n = n.lower()
    if n in GPR_INDEX:
        return 64
    if n in SUB:
        return SUB[n][1]
    if n in XMM:
        return 128
    return 64
