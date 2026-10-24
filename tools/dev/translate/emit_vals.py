#!/usr/bin/env python3
"""emit_vals.py: value helpers shared by the emitter backends.

A Val is (c-expr, width, kind): kind in int/f32/f64/u128/addr.
reg_val(em, name) reads; mem_val(em, memtxt, width-hint, float?) reads.
write_reg(name, expr, width) / write_mem(memtxt, expr, width, float?) emit.
"""
from lift import GPR_INDEX, SUB
from lift import SUB_SHIFT


def reg_parent(name):
    n = name.lower()
    if n in GPR_INDEX:
        return GPR_INDEX[n], 64, "zx", 0
    p, bits, kind = SUB[n]
    return GPR_INDEX[p], bits, kind, SUB_SHIFT.get(n, 0)


def reg_read(name):
    """C expression (u64) reading an integer register."""
    i, bits, kind, sh = reg_parent(name)
    if bits == 64:
        return "s.gpr[%d]" % i
    if bits == 32 and kind == "zx":
        return "((uint64_t)(uint32_t)s.gpr[%d])" % i
    if bits == 16:
        return "((uint64_t)(uint16_t)(s.gpr[%d] >> %d))" % (i, sh)
    return "((uint64_t)(uint8_t)(s.gpr[%d] >> %d))" % (i, sh)


def reg_write(name, expr):
    """C statement writing an integer register (width discipline)."""
    i, bits, kind, sh = reg_parent(name)
    if bits == 64:
        return "s.gpr[%d] = (uint64_t)(%s);" % (i, expr)
    if bits == 32 and kind == "zx":
        return "s.gpr[%d] = (uint64_t)(uint32_t)(%s);" % (i, expr)
    m = {16: "0xFFFF", 8: "0xFF"}[bits]
    if sh:
        return ("s.gpr[%d] = (s.gpr[%d] & ~(0xFFULL << %d)) | "
                "(((uint64_t)(uint8_t)(%s)) << %d);") % (i, i, sh, expr, sh)
    return ("s.gpr[%d] = (s.gpr[%d] & ~%sULL) | "
            "((uint64_t)(%s) & %sULL);") % (i, i, m, expr, m)


def sext(expr, bits):
    """Sign-extend a `bits`-wide value to 64."""
    return "((int64_t)(%s << %d) >> %d)" % (expr, 64 - bits, 64 - bits)
