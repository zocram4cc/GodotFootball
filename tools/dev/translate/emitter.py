#!/usr/bin/env python3
"""The emitter: asm lines + image -> C++ translation unit text.

Deterministic: labels sorted by address, switch tables sorted, floats %a.
"""
import re
import struct

from lift import canon_reg, parse_mem, GPR_INDEX, SUB

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
        return MNEM_MAP.get(m, "??")
    if m.startswith("CMOV"):
        return MNEM_MAP.get("J" + m[4:], "??")
    if m.startswith("SET"):
        return MNEM_MAP.get("J" + m[3:], "??")
    return "??"


def imm_val(s):
    s = s.strip().lower()
    if s.startswith("0x"):
        return int(s, 16)
    if s.startswith("-0x"):
        return -int(s[1:], 16)
    return int(s, 0)


def is_imm(s):
    try:
        imm_val(s)
        return True
    except ValueError:
        return False


class Emitter:
    def __init__(self, image_path, va, pilot_vas):
        self.image = image_path
        self.va = va
        self.pilot = pilot_vas
        self.consts = {}  # va -> (nbytes, bytes): baked image constants
        self.tables = {}  # addr -> [u32 words]: switch tables
        self.lines = []
        self.need_memset = False

    # ---- memory helpers ----
    def read_image(self, va, n):
        with open(self.image, "rb") as f:
            f.seek(va - 0x140000000)
            blob = f.read(n)
        if len(blob) != n:
            raise SystemExit("emit %s: image unreadable at %#x"
                             % (self.va, va))
        return blob

    def note_const(self, va, n):
        if va not in self.consts:
            self.consts[va] = self.read_image(va, n)

    # ---- operand printers ----
    def reg_reader(self, name):
        """C expression reading the register (u64 / u32 / f32-lane)."""
        n = name.lower()
        if n in GPR_INDEX:
            return "s.gpr[%d]" % GPR_INDEX[n]
        if n in SUB:
            parent, bits, kind = SUB[n]
            i = GPR_INDEX[parent]
            from lift import SUB_SHIFT
            sh = SUB_SHIFT.get(n, 0)
            if bits == 32 and kind == "zx":
                return "((uint64_t)(uint32_t)s.gpr[%d])" % i
            if bits == 16:
                return "((uint64_t)(uint16_t)(s.gpr[%d] >> %d))" % (i, sh)
            return "((uint64_t)(uint8_t)(s.gpr[%d] >> %d))" % (i, sh)
        if n.startswith("xmm"):
            return "s.xmm[%d][0]" % int(n[3:])
        return "/*UNKREG %s*/0ULL" % name

    def addr_expr(self, mp):
        """C expression for the effective address (u64)."""
        terms = []
        if mp["base"] is not None:
            terms.append("(%s)" % self.reg_reader(mp["base"]))
        if mp["index"] is not None:
            sc = mp["scale"]
            terms.append("((%s)*%dULL)"
                         % (self.reg_reader(mp["index"]), sc))
        if mp["disp"]:
            d = mp["disp"]
            terms.append("(%dULL)" % (d & 0xFFFFFFFFFFFFFFFF)
                         if d >= 0 else "(0ULL - %dULL)" % (-d))
        if not terms:
            return "0ULL"
        return "(%s)" % " + ".join(terms)

    def mem_reader(self, memtxt, width, is_float):
        mp = parse_mem(memtxt)
        if mp is None:
            return "/*BADMEM %s*/0ULL" % memtxt
        if mp["absolute"] is not None:
            va = mp["absolute"]
            n = 4 if (is_float or width == 32) else width // 8
            if width == 128:
                n = 16
            elif width == 64 and is_float:
                n = 8
            self.note_const(va, n)
            tag = "k_c%X" % va
            if width == 128:
                return "k_c%X" % va
            if is_float and width == 32:
                return "k_c%X_f" % va
            if is_float and width == 64:
                return "k_c%X_d" % va
            if width == 64:
                return "k_c%X_q" % va
            if width == 32:
                return "k_c%X_w" % va
            if width == 16:
                return "k_c%X_h" % va
            return "k_c%X_b" % va
        a = self.addr_expr(mp)
        if width == 128:
            return "s.rd128(%s)" % a
        if is_float and width == 32:
            return "s.rdf(%s)" % a
        if is_float and width == 64:
            return "s.rdd(%s)" % a
        n = width // 8
        return "s.rd(%s, %d)" % (a, n)
