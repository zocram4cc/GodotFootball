#!/usr/bin/env python3
"""Operand printers + per-mnemonic emission (the lifter's backend).

Conventions (deterministic: sorted tables, %a hex floats, VA-cited consts):
  R64(i)    -> s.gpr[i]                      (u64)
  R32(i)    -> (uint32_t)s.gpr[i]            (zero-extend on write)
  R16(i)    -> (uint16_t)s.gpr[i] / preserve on write
  R8(i,sh)  -> byte sh of s.gpr[i]
  X_LANE(x) -> s.x_f32(x)                    (lane 0)
  MEM(addr-expr, n) -> s.rd(addr, n) / s.rdf(addr) for f32
"""
import struct

WIDTHS = {"b": 1, "w": 2, "d": 4, "q": 8}


def hexf(f):
    """Deterministic float literal: hex float + decimal comment."""
    return "(%af /* %r */)" % (f, f)


def const_bytes(blob):
    return "{%s}" % ", ".join("0x%02X" % b for b in blob)


def bits_of_f32(va, image_path):
    import sys
    sys.path.insert(0, ".")
    with open(image_path, "rb") as f:
        f.seek(va - 0x140000000)
        blob = f.read(4)
    return struct.unpack("<I", blob)[0]
