#!/usr/bin/env python3
"""Instruction emission: one asm line -> C++ statements (methods on Emitter).

Integer flags: ZF/SF/CF/OF (PF only from COMISS/UCOMISS; no JP reads an
integer op's PF in this set). CF/OF per width (b/w/d/q); 32-bit ops use
32-bit carry (x86-64 zeroes the top, carry is 32-bit).
"""
import re


def wmask(width):
    return {8: 0xFF, 16: 0xFFFF, 32: 0xFFFFFFFF,
            64: 0xFFFFFFFFFFFFFFFF}[width]


def wtype(width):
    return {8: "uint8_t", 16: "uint16_t", 32: "uint32_t",
            64: "uint64_t"}[width]


def stype(width):
    return {8: "int8_t", 16: "int16_t", 32: "int32_t",
            64: "int64_t"}[width]
