#!/usr/bin/env python3
"""translate.py: PES function (asm + image bytes) -> C++ (build tree only).

One integrated, deterministic translator. Reads the user's unpacked image
(PES_IMAGE, file offset = VA - 0x140000000) for every constant; bakes them
as VA-cited constexpr. Never commits PES bytes (only VAs + offsets in tools).

Bytes beat the decompile: signedness from Jcc/CMOVcc/SETcc opcodes, float
polarity from COMISS vs UCOMISS + JP presence, CMOV (not branches).

Memory model (mirrors run.py): flat PesMem with baked image runs + caller
regions + stack segment. RSP is a plain u64 (PUSH/POP/CALL adjust it).
CALL intra-set = C++ sibling call with RSP push/pop. Tail JMP = sibling
call + return. Switch (LEA [0x140000000] + table + ADD + JMP reg) = baked
table + switch statement. Deterministic: sorted labels/tables, %a floats.

Usage: translate.py --image IMG --asm FILE --va 0xVA --out OUT.cpp
         [--pilot VA ...] [--header OUT.h]
"""
import argparse
import os
import re
import struct
import sys

BASE = 0x140000000

GPR = {"rax": 0, "rcx": 1, "rdx": 2, "rbx": 3, "rsp": 4, "rbp": 5,
       "rsi": 6, "rdi": 7, "r8": 8, "r9": 9, "r10": 10, "r11": 11,
       "r12": 12, "r13": 13, "r14": 14, "r15": 15}
SUB = {"eax": ("rax", 32, 0), "ebx": ("rbx", 32, 0), "ecx": ("rcx", 32, 0),
       "edx": ("rdx", 32, 0), "esi": ("rsi", 32, 0), "edi": ("rdi", 32, 0),
       "ebp": ("rbp", 32, 0), "esp": ("rsp", 32, 0),
       "r8d": ("r8", 32, 0), "r9d": ("r9", 32, 0), "r10d": ("r10", 32, 0),
       "r11d": ("r11", 32, 0), "r12d": ("r12", 32, 0), "r13d": ("r13", 32, 0),
       "r14d": ("r14", 32, 0), "r15d": ("r15", 32, 0),
       "ax": ("rax", 16, 0), "bx": ("rbx", 16, 0), "cx": ("rcx", 16, 0),
       "dx": ("rdx", 16, 0), "si": ("rsi", 16, 0), "di": ("rdi", 16, 0),
       "bp": ("rbp", 16, 0), "sp": ("rsp", 16, 0),
       "r8w": ("r8", 16, 0), "r9w": ("r9", 16, 0),
       "r10w": ("r10", 16, 0), "r11w": ("r11", 16, 0),
       "r12w": ("r12", 16, 0), "r13w": ("r13", 16, 0),
       "r14w": ("r14", 16, 0), "r15w": ("r15", 16, 0),
       "al": ("rax", 8, 0), "bl": ("rbx", 8, 0), "cl": ("rcx", 8, 0),
       "dl": ("rdx", 8, 0), "sil": ("rsi", 8, 0), "dil": ("rdi", 8, 0),
       "bpl": ("rbp", 8, 0), "spl": ("rsp", 8, 0),
       "r8b": ("r8", 8, 0), "r9b": ("r9", 8, 0), "r10b": ("r10", 8, 0),
       "r11b": ("r11", 8, 0), "r12b": ("r12", 8, 0), "r13b": ("r13", 8, 0),
       "r14b": ("r14", 8, 0), "r15b": ("r15", 8, 0),
       "ah": ("rax", 8, 8), "bh": ("rbx", 8, 8),
       "ch": ("rcx", 8, 8), "dh": ("rdx", 8, 8)}
REGSET = set(GPR) | set(SUB) | {"xmm%d" % i for i in range(16)}
PARENT = dict(GPR)
for _r in REGSET:
    if _r in GPR:
        PARENT[_r] = _r
    elif _r in SUB:
        PARENT[_r] = SUB[_r][0]
    else:
        PARENT[_r] = _r

CCMAP = {
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
        return CCMAP.get(m)
    if m.startswith("CMOV"):
        return CCMAP.get("J" + m[4:])
    if m.startswith("SET"):
        return CCMAP.get("J" + m[3:])
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

class Image:
    """Byte source: the user's unpacked image. Records every read VA."""
    def __init__(self, path):
        self.path = path
        self.f = open(path, "rb")
        self.size = os.fstat(self.f.fileno()).st_size

    def read(self, va, n):
        off = va - BASE
        if off < 0 or off + n > self.size:
            raise SystemExit("xlate: image unreadable at %#x" % va)
        self.f.seek(off)
        blob = self.f.read(n)
        if len(blob) != n:
            raise SystemExit("xlate: image unreadable at %#x" % va)
        return blob


def reg_parent(name):
    n = name.lower()
    if n in GPR:
        return GPR[n], 64, 0
    p, bits, sh = SUB[n]
    return GPR[p], bits, sh


def reg_read(name):
    """C u64 expression reading an integer register."""
    i, bits, sh = reg_parent(name)
    if bits == 64:
        return "s.gpr[%d]" % i
    if bits == 32:
        return "((uint64_t)(uint32_t)s.gpr[%d])" % i
    if bits == 16:
        return "((uint64_t)(uint16_t)s.gpr[%d])" % i
    return "((uint64_t)(uint8_t)(s.gpr[%d] >> %d))" % (i, sh)


def reg_write(name, expr):
    """C statement writing an integer register (x86 width discipline)."""
    i, bits, sh = reg_parent(name)
    if bits == 64:
        return "s.gpr[%d] = (uint64_t)(%s);" % (i, expr)
    if bits == 32:
        # 32-bit writes zero-extend.
        return "s.gpr[%d] = (uint64_t)(uint32_t)(%s);" % (i, expr)
    if bits == 16:
        return ("s.gpr[%d] = (s.gpr[%d] & 0xFFFFFFFFFFFF0000ULL) | "
                "((uint64_t)(uint16_t)(%s));") % (i, i, expr)
    if sh:
        return ("s.gpr[%d] = (s.gpr[%d] & ~(0xFFULL << %d)) | "
                "(((uint64_t)(uint8_t)(%s)) << %d);") % (i, i, sh, expr, sh)
    return ("s.gpr[%d] = (s.gpr[%d] & 0xFFFFFFFFFFFFFF00ULL) | "
            "((uint64_t)(uint8_t)(%s));") % (i, i, expr)


def sext(expr, bits):
    sh = 64 - bits
    return "((int64_t)(((uint64_t)(%s) << %d) >> %d))" % (expr, sh, sh)


def parse_mem(inner):
    # Split on + and - (but not on a leading sign or 0x digits). "rdi - 8"
    # -> ["rdi", "-8"]; "rcx+rax*4-0x10" works the same.
    tmp = re.sub(r"\s*-\s*", "+-", inner.strip().lower())
    parts = [p.strip() for p in tmp.split("+") if p.strip() != ""]
    base = None
    index = None
    scale = 1
    nums = []
    ok = True
    for p in parts:
        mm = re.fullmatch(r"([a-z][a-z0-9]*)\*0x([0-9a-f]+)", p)
        if mm and mm.group(1) in REGSET:
            index, scale = mm.group(1), int(mm.group(2), 16)
            continue
        mm = re.fullmatch(r"([a-z][a-z0-9]*)\*(\d+)", p)
        if mm and mm.group(1) in REGSET:
            index, scale = mm.group(1), int(mm.group(2))
            continue
        if p in REGSET:
            if base is None:
                base = p
            elif index is None:
                index, scale = p, 1
            else:
                ok = False
            continue
        try:
            nums.append(int(p, 16) if p.startswith("0x") else int(p, 0))
            continue
        except ValueError:
            ok = False
    if not ok:
        return None
    disp = sum(nums)
    absolute = nums[0] if (base is None and index is None and nums) else None
    return {"base": base, "index": index, "scale": scale, "disp": disp,
            "absolute": absolute}


class Emitter:
    """One function body. Collects baked constants + switch tables."""
    def __init__(self, img, va, pilot):
        self.img = img
        self.va = va
        self.pilot = pilot  # set of "0x.." in-set callees
        self.consts = {}  # va -> bytes (image constants, VA-cited)
        self.tables = {}  # table VA -> {"words": [...], "base": va}
        self.siblings = set()  # in-set callees + tail targets referenced
        self.out = []  # body lines (labels included)

    def bake(self, va, n):
        if (va, n) not in self.consts:
            self.consts[(va, n)] = self.img.read(va, n)
        return va

    def addr_expr(self, mp):
        terms = []
        if mp["base"] is not None:
            terms.append("(%s)" % reg_read(mp["base"]))
        if mp["index"] is not None:
            terms.append("((%s)*%dULL)"
                         % (reg_read(mp["index"]), mp["scale"]))
        if mp["disp"]:
            d = mp["disp"]
            terms.append("(0x%XULL)" % (d & 0xFFFFFFFFFFFFFFFF) if d >= 0 else "(0ULL - 0x%XULL)" % (-d))
        return ("(%s)" % " + ".join(terms)) if terms else "0ULL"

    def mem_val(self, memtxt, width, flt):
        """C expression reading memory. width in bits; flt in (None,'f32','f64')."""
        if "[" in memtxt:
            memtxt = memtxt[memtxt.find("[") + 1:memtxt.rfind("]")]
        mp = parse_mem(memtxt)
        if mp is None:
            return "/*BADMEM %s*/0ULL" % memtxt
        if mp["absolute"] is not None:
            va = mp["absolute"]
            if width == 128:
                self.bake(va, 16)
                return "s.rd128(k_c%X)" % va
            n = 4 if (flt == "f32" or width == 32) else width // 8
            if flt == "f64":
                n = 8
            self.bake(va, n)
            if flt == "f32":
                return "k_c%X_f" % va
            if flt == "f64":
                return "PesState::f64(k_c%X_q)" % va
            if n == 8:
                return "k_c%X_q" % va
            if n == 4:
                return "k_c%X_w" % va
            if n == 2:
                return "k_c%X_h" % va
            return "k_c%X_b" % va
        a = self.addr_expr(mp)
        if width == 128:
            return "s.rd128(%s)" % a
        if flt == "f32":
            return "s.rdf(%s)" % a
        if flt == "f64":
            return "s.rdd(%s)" % a
        return "s.rd(%s, %d)" % (a, width // 8)

    def mem_store(self, memtxt, expr, width, flt):
        if "[" in memtxt:
            memtxt = memtxt[memtxt.find("[") + 1:memtxt.rfind("]")]
        mp = parse_mem(memtxt)
        if mp is None:
            return "/*BADMEM-STORE %s*/(void)0;" % memtxt
        if mp["absolute"] is not None:
            # Absolute stores hit writable .data globals (live state): the
            # diff main maps the image VA range writable, seeded from the
            # user's image, so the store is observable like the harness.
            va = mp["absolute"]
            if flt == "f32":
                return "s.wrf(0x%XULL, (float)(%s));" % (va, expr)
            if flt == "f64":
                return "s.wrd(0x%XULL, (double)(%s));" % (va, expr)
            return "s.wr(0x%XULL, (uint64_t)(%s), %d);" % (va, expr,
                                                           width // 8)
        a = self.addr_expr(mp)
        if flt == "f32":
            return "s.wrf(%s, (float)(%s));" % (a, expr)
        if flt == "f64":
            return "s.wrd(%s, (double)(%s));" % (a, expr)
        if width == 128:
            return "s.wr128(%s, %s);" % (a, expr)
        return "s.wr(%s, (uint64_t)(%s), %d);" % (a, expr, width // 8)

    # ---- integer ALU ----
    def flag_zn(self, expr, width):
        m = {8: "0xFF", 16: "0xFFFF", 32: "0xFFFFFFFF",
             64: "0xFFFFFFFFFFFFFFFF"}[width]
        return ["s.zf = (((uint64_t)(%s) & %sULL) == 0);" % (expr, m),
                "s.sf = ((((uint64_t)(%s)) >> %d) & 1) != 0);" % (expr,
                                                                  width - 1)]

    def emit_alu(self, mnem, ops):
        """Integer ALU. Returns [str] or None to fall through."""
        m = mnem.upper()
        if m == "MOV":
            return self.do_mov(ops, 0)
        if m in ("MOVSX", "MOVSXD"):
            return self.do_mov(ops, "sx")
        if m == "MOVZX":
            return self.do_mov(ops, "zx")
        if m == "LEA":
            return self.do_lea(ops)
        if m == "XCHG":
            return self.do_xchg(ops)
        if m in ("ADD", "SUB", "CMP", "TEST", "AND", "OR", "XOR", "ADC",
                 "SBB"):
            return self.do_logical(m, ops)
        if m in ("INC", "DEC", "NEG", "NOT"):
            return self.do_unary(m, ops)
        if m in ("SHL", "SHR", "SAR"):
            return self.do_shift(m, ops)
        if m in ("IMUL", "MUL"):
            return self.do_mul(m, ops)
        if m in ("BT", "BTC", "BTR", "BTS"):
            return self.do_bit(m, ops)
        if m in ("CDQE", "CDQ", "CQO", "CWD", "CBW", "CWDE"):
            return self.do_extend(m)
        if m in ("PUSH", "POP"):
            return self.do_stack(m, ops)
        if m == "LEAVE":
            return ["s.gpr[4] = s.gpr[5];",
                    "s.gpr[5] = s.rd(s.gpr[4], 8);",
                    "s.gpr[4] += 8;"]
        if m == "NOP":
            return ["(void)0;"]
        return None

    def op_width(self, dst, src=None):
        """Width from the destination operand (reg name or ptr size)."""
        d = dst.strip().lower()
        if d in GPR:
            return 64
        if d in SUB:
            return SUB[d][1]
        mm = re.match(r"(byte|word|dword|qword|xmmword|ymmword)\s+ptr",
                      d)
        if mm:
            return {"byte": 8, "word": 16, "dword": 32, "qword": 64,
                    "xmmword": 128, "ymmword": 256}[mm.group(1)]
        mm = re.match(r"(byte|word|dword|qword|xmmword)\s*\[", d)
        if mm:
            return {"byte": 8, "word": 16, "dword": 32, "qword": 64,
                    "xmmword": 128}[mm.group(1)]
        if d.startswith("xmm") and "[" not in d:
            return 128
        return 64

    def val_of(self, op, width, sx=False):
        """C value expression for source operand (imm/reg/mem)."""
        o = op.strip()
        if is_imm(o):
            v = imm_val(o)
            return "(%dULL)" % (v & 0xFFFFFFFFFFFFFFFF) if v >= 0 else \
                "(0ULL - %dULL)" % (-v)
        if o.lower() in REGSET and "xmm" not in o.lower():
            e = reg_read(o)
            if sx and width < 64:
                e = sext(e, width)
            return e
        # memory
        return self.mem_val(o, width, None)

    def do_mov(self, ops, ext):
        dst, src = ops[0], ops[1]
        w = self.op_width(dst, src)
        if "xmm" in dst.lower() or "xmm" in src.lower():
            return None  # vector path handles these
        if w == 128:
            # integer xmmword transfer (MOVAPS/MOVUPS on int data)
            return self.do_mov128(dst, src)
        if "[" in dst:
            inner = dst[dst.find("["):]
            if ext == "sx" or ext == "zx":
                v = self.val_of(src, w)
                return [self.mem_store(inner, v, w, None)]
            v = self.val_of(src, w)
            return [self.mem_store(inner, v, w, None)]
        if "[" in src:
            inner = src[src.find("["):]
            v = self.mem_val(inner, w, None)
            if ext == "sx":
                # source width: from src ptr size or 32 for MOVSXD
                sw = self.op_width(src)
                if sw == 64:
                    sw = 32
                v = sext(v, sw)
            return [reg_write(dst, v)]
        v = self.val_of(src, w)
        if ext == "sx":
            sw = self.op_width(src)
            v = sext(v, sw)
        return [reg_write(dst, v)]

    def do_mov128(self, dst, src):
        if "[" in dst:
            inner = dst[dst.find("["):]
            if "xmm" in src.lower():
                x = int(src.lower()[3:])
                return ["s.wr128(%s, s.xmm[%d][0] | "
                        "((unsigned __int128)s.xmm[%d][1] << 64));"
                        % (self.addr_expr(parse_mem(inner[1:-1])), x, x)]
            return [self.mem_store(inner, self.val_of(src, 128), 128,
                                   None)]
        if "[" in src:
            inner = src[src.find("["):]
            x = int(dst.lower()[3:])
            return ["{ unsigned __int128 _v = %s; s.xmm[%d][0] = "
                    "(uint64_t)_v; s.xmm[%d][1] = (uint64_t)(_v >> 64); }"
                    % (self.mem_val(inner, 128, None), x, x)]
        xs, xd = int(src.lower()[3:]), int(dst.lower()[3:])
        return ["s.xmm[%d][0] = s.xmm[%d][0]; s.xmm[%d][1] = s.xmm[%d][1];"
                % (xd, xs, xd, xs)]

    def do_lea(self, ops):
        dst, src = ops[0], ops[1]
        inner = src[src.find("["):]
        mp = parse_mem(inner[1:-1])
        return [reg_write(dst, self.addr_expr(mp))]

    def do_xchg(self, ops):
        a, b = ops[0], ops[1]
        w = self.op_width(a)
        ta, tb = self.val_of(a, w), self.val_of(b, w)
        out = []
        if "[" in a:
            out.append("{ uint64_t _t = %s; %s; %s; }" % (
                ta, self.mem_store(a[a.find("["):], tb, w, None).rstrip(";"),
                reg_write(b, "_t").rstrip(";")))
        else:
            out.append("{ uint64_t _t = %s; %s %s }" % (
                ta, reg_write(a, tb).rstrip(";") + ";",
                reg_write(b, "_t").rstrip(";") + ";"))
        return out
    def do_logical(self, m, ops):
        dst = ops[0]
        w = self.op_width(dst, ops[1] if len(ops) > 1 else None)
        if w == 128:
            return None
        mm = {8: "0xFF", 16: "0xFFFF", 32: "0xFFFFFFFF",
              64: "0xFFFFFFFFFFFFFFFF"}[w]
        a = self.val_of(dst, w)
        out = []
        if m in ("CMP", "TEST"):
            b = self.val_of(ops[1], w)
            if m == "CMP":
                # r = a - b; CF = a<b unsigned; OF = signed overflow
                out.append("{ uint64_t _a = (uint64_t)(%s), _b = "
                           "(uint64_t)(%s), _r = (_a - _b) & %sULL;"
                           % (a, b, mm))
                out.append("  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);"
                           % (w - 1))
                out.append("  s.cf = (_a & %sULL) < (_b & %sULL);"
                           % (mm, mm))
                out.append("  s.of = (((_a ^ _b) & (_a ^ _r)) >> %d) & 1; }"
                           % (w - 1))
            else:
                out.append("{ uint64_t _r = ((uint64_t)(%s) & "
                           "(uint64_t)(%s)) & %sULL;"
                           % (a, b, mm))
                out.append("  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);"
                           % (w - 1))
                out.append("  s.cf = false; s.of = false; }")
            return out
        if len(ops) > 1 and m in ("ADC", "SBB"):
            b = self.val_of(ops[1], w)
            c = "s.cf ? 1ULL : 0ULL"
            if m == "ADC":
                out.append("{ uint64_t _a = (uint64_t)(%s), _b = "
                           "(uint64_t)(%s), _c = %s, _r = "
                           "(_a + _b + _c) & %sULL;" % (a, b, c, mm))
                out.append("  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);"
                           % (w - 1))
                out.append("  s.cf = (_r < (_a & %sULL)) || "
                           "(_c && _r == (_a & %sULL));" % (mm, mm))
                out.append("  s.of = (((_a ^ ~_b) & (_a ^ _r)) >> %d) & 1; }"
                           % (w - 1))
            else:
                out.append("{ uint64_t _a = (uint64_t)(%s), _b = "
                           "(uint64_t)(%s), _c = %s, _r = "
                           "(_a - _b - _c) & %sULL;" % (a, b, c, mm))
                out.append("  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);"
                           % (w - 1))
                out.append("  s.cf = ((_a & %sULL) < "
                           "(((_b + _c)) & %sULL));" % (mm, mm))
                out.append("  s.of = (((_a ^ _b) & (_a ^ _r)) >> %d) & 1; }"
                           % (w - 1))
        elif len(ops) > 1:
            b = self.val_of(ops[1], w)
            op = {"ADD": "+", "SUB": "-", "AND": "&", "OR": "|",
                  "XOR": "^"}[m]
            if m in ("AND", "OR", "XOR"):
                out.append("{ uint64_t _r = ((uint64_t)(%s) %s "
                           "(uint64_t)(%s)) & %sULL;" % (a, op, b, mm))
                out.append("  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);"
                           % (w - 1))
                out.append("  s.cf = false; s.of = false;")
            elif m == "ADD":
                out.append("{ uint64_t _a = (uint64_t)(%s), _b = "
                           "(uint64_t)(%s), _r = (_a + _b) & %sULL;"
                           % (a, b, mm))
                out.append("  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);"
                           % (w - 1))
                out.append("  s.cf = (_r < (_a & %sULL));" % mm)
                out.append("  s.of = (((_a ^ ~_b) & (_a ^ _r)) >> %d) & 1;"
                           % (w - 1))
            else:  # SUB
                out.append("{ uint64_t _a = (uint64_t)(%s), _b = "
                           "(uint64_t)(%s), _r = (_a - _b) & %sULL;"
                           % (a, b, mm))
                out.append("  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);"
                           % (w - 1))
                out.append("  s.cf = ((_a & %sULL) < (_b & %sULL));" % (mm,
                                                                        mm))
                out.append("  s.of = (((_a ^ _b) & (_a ^ _r)) >> %d) & 1;"
                           % (w - 1))
        if m not in ("CMP", "TEST"):
            if "[" in dst:
                inner = dst[dst.find("["):]
                out.append("  " + self.mem_store(inner, "_r", w, None))
            else:
                out.append("  " + reg_write(dst, "_r"))
            out.append("}")
        else:
            out.append("}")
        return out
    def do_unary(self, m, ops):
        dst = ops[0]
        w = self.op_width(dst)
        mm = {8: "0xFF", 16: "0xFFFF", 32: "0xFFFFFFFF",
              64: "0xFFFFFFFFFFFFFFFF"}[w]
        a = self.val_of(dst, w)
        if m == "INC":
            out = ["{ uint64_t _a = (uint64_t)(%s), _r = (_a + 1) & %sULL;"
                   % (a, mm),
                   "  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);" % (w - 1),
                   "  s.of = (_a == %sULL);" % (mm,)]
        elif m == "DEC":
            out = ["{ uint64_t _a = (uint64_t)(%s), _r = (_a - 1) & %sULL;"
                   % (a, mm),
                   "  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);" % (w - 1),
                   "  s.of = (_a == 0);"]
        elif m == "NEG":
            out = ["{ uint64_t _a = (uint64_t)(%s), _r = (0ULL - _a) & %sULL;"
                   % (a, mm),
                   "  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);" % (w - 1),
                   "  s.cf = (_a != 0);",
                   "  s.of = (_a == (1ULL << %d));" % (w - 1)]
        else:  # NOT: no flags
            out = ["{ uint64_t _r = (~(uint64_t)(%s)) & %sULL;" % (a, mm)]
        if "[" in dst:
            out.append("  " + self.mem_store(dst[dst.find("["):], "_r", w,
                                              None))
        else:
            out.append("  " + reg_write(dst, "_r"))
        out.append("}")
        return out

    def do_shift(self, m, ops):
        dst = ops[0]
        w = self.op_width(dst)
        mm = {8: "0xFF", 16: "0xFFFF", 32: "0xFFFFFFFF",
              64: "0xFFFFFFFFFFFFFFFF"}[w]
        a = self.val_of(dst, w)
        c = self.val_of(ops[1], 8) if len(ops) > 1 else "1ULL"
        if m == "SHL":
            r = "((_a << (_c & %d)) & %sULL)" % (w - 1 if w < 64 else 63,
                                                 mm)
        elif m == "SHR":
            r = "((_a & %sULL) >> (_c & %d))" % (mm,
                                                 w - 1 if w < 64 else 63)
        else:
            r = "((uint64_t)(((int64_t)((uint64_t)(_a << %d)) >> %d) >> " \
                "(_c & %d)) & %sULL)" % (64 - w, 64 - w,
                                         w - 1 if w < 64 else 63, mm)
        out = ["{ uint64_t _a = (uint64_t)(%s), _c = (uint64_t)(%s) & 0xFF;"
               % (a, c),
               "  uint64_t _r = %s;" % r,
               "  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);" % (w - 1),
               "  s.cf = false; s.of = false;"]
        if "[" in dst:
            out.append("  " + self.mem_store(dst[dst.find("["):], "_r", w,
                                              None))
        else:
            out.append("  " + reg_write(dst, "_r"))
        out.append("}")
        return out

    def do_mul(self, m, ops):
        if m == "MUL" and len(ops) == 1:
            # unsigned: EDX:EAX = EAX * op (32-bit) or RDX:RAX (64-bit)
            o = ops[0]
            w = self.op_width(o)
            b = self.val_of(o, w)
            if w <= 32:
                return ["{ uint64_t _p = (uint64_t)(uint32_t)s.gpr[0] * "
                        "(uint64_t)(uint32_t)(%s);" % b,
                        "  s.gpr[0] = (s.gpr[0] & 0xFFFFFFFF00000000ULL) | "
                        "(_p & 0xFFFFFFFFULL);",
                        "  s.gpr[2] = (s.gpr[2] & 0xFFFFFFFF00000000ULL) | "
                        "((_p >> 32) & 0xFFFFFFFFULL);",
                        "  s.cf = s.of = ((_p >> 32) != 0); }"]
            return ["{ unsigned __int128 _p = (unsigned __int128)s.gpr[0] * "
                    "(uint64_t)(%s);" % b,
                    "  s.gpr[0] = (uint64_t)_p; s.gpr[2] = "
                    "(uint64_t)(_p >> 64);",
                    "  s.cf = s.of = ((_p >> 64) != 0); }"]
        if m == "IMUL" and len(ops) == 1:
            o = ops[0]
            w = self.op_width(o)
            b = self.val_of(o, w)
            if w <= 32:
                return ["{ int64_t _p = (int64_t)(int32_t)s.gpr[0] * "
                        "(int64_t)(int32_t)(%s);" % b,
                        "  s.gpr[0] = (s.gpr[0] & 0xFFFFFFFF00000000ULL) | "
                        "(_p & 0xFFFFFFFFULL);",
                        "  s.gpr[2] = (s.gpr[2] & 0xFFFFFFFF00000000ULL) | "
                        "((_p >> 32) & 0xFFFFFFFFULL);",
                        "  s.cf = s.of = false; }"]
            return ["{ __int128 _p = (__int128)(int64_t)s.gpr[0] * "
                    "(int64_t)(%s);" % b,
                    "  s.gpr[0] = (uint64_t)_p; s.gpr[2] = "
                    "(uint64_t)(_p >> 64);",
                    "  s.cf = s.of = false; }"]
        if m == "IMUL" and len(ops) == 3:
            dst = ops[0]
            w = self.op_width(dst)
            mm = {16: "0xFFFF", 32: "0xFFFFFFFF",
                  64: "0xFFFFFFFFFFFFFFFF"}[w]
            a = self.val_of(ops[1], w)
            b = self.val_of(ops[2], w)
            if w == 64:
                e = "(uint64_t)((__int128)(int64_t)(%s) * " \
                    "(int64_t)(%s))" % (a, b)
            elif w == 32:
                e = "(uint64_t)(uint32_t)((int64_t)(int32_t)(%s) * " \
                    "(int64_t)(int32_t)(%s))" % (a, b)
            else:
                e = "(uint64_t)(uint16_t)((int64_t)(int16_t)(%s) * " \
                    "(int64_t)(int16_t)(%s))" % (a, b)
            if "[" in dst:
                return [self.mem_store(dst[dst.find("["):], e, w, None)]
            return [reg_write(dst, e)]
        if m == "IMUL" and len(ops) == 2:
            dst = ops[0]
            w = self.op_width(dst)
            a = self.val_of(ops[0], w)
            b = self.val_of(ops[1], w)
            if w == 64:
                e = "(uint64_t)((__int128)(int64_t)(%s) * " \
                    "(int64_t)(%s))" % (a, b)
            elif w == 32:
                e = "(uint64_t)(uint32_t)((int64_t)(int32_t)(%s) * " \
                    "(int64_t)(int32_t)(%s))" % (a, b)
            else:
                e = "(uint64_t)(uint16_t)((int64_t)(int16_t)(%s) * " \
                    "(int64_t)(int16_t)(%s))" % (a, b)
            if "[" in dst:
                return [self.mem_store(dst[dst.find("["):], e, w, None)]
            return [reg_write(dst, e)]
        return ["/*UNHANDLED %s %s*/(void)0;" % (m, ops)]

    def do_bit(self, m, ops):
        dst = ops[0]
        w = self.op_width(dst)
        a = self.val_of(dst, w)
        b = self.val_of(ops[1], 8)
        out = ["{ uint64_t _a = (uint64_t)(%s), _b = (uint64_t)(%s) & %d;"
               % (a, b, w - 1),
               "  s.cf = ((_a >> _b) & 1);"]
        if m == "BTC":
            out.append("  uint64_t _r = _a ^ (1ULL << _b);")
        elif m == "BTR":
            out.append("  uint64_t _r = _a & ~(1ULL << _b);")
        elif m == "BTS":
            out.append("  uint64_t _r = _a | (1ULL << _b);")
        else:
            out.append("  uint64_t _r = _a;")
        if m != "BT":
            if "[" in dst:
                out.append("  " + self.mem_store(dst[dst.find("["):],
                                                  "_r", w, None) + " }")
            else:
                out.append("  " + reg_write(dst, "_r") + " }")
        else:
            out.append("}")
        return [" ".join(out)]

    def do_extend(self, m):
        if m == "CDQE":
            return ["s.gpr[0] = (uint64_t)(int64_t)(int32_t)s.gpr[0];"]
        if m == "CDQ":
            return ["s.gpr[2] = ((int64_t)(int32_t)s.gpr[0] < 0) ? "
                    "0xFFFFFFFFFFFFFFFFULL : 0ULL;"]
        if m == "CQO":
            return ["s.gpr[2] = ((int64_t)s.gpr[0] < 0) ? "
                    "0xFFFFFFFFFFFFFFFFULL : 0ULL;"]
        if m == "CWD":
            return ["s.gpr[2] = (s.gpr[2] & 0xFFFFFFFFFFFF0000ULL) | "
                    "(((int16_t)s.gpr[0] < 0) ? 0xFFFFULL : 0ULL;"]
        if m in ("CBW", "CWDE"):
            return ["s.gpr[0] = (uint64_t)(int64_t)(int16_t)s.gpr[0];"]
        return ["/*UNHANDLED %s*/(void)0;" % m]

    def do_stack(self, m, ops):
        if m == "PUSH":
            v = self.val_of(ops[0], 64)
            return ["s.gpr[4] -= 8;", "s.wr(s.gpr[4], (uint64_t)(%s), 8);"
                    % v]
        v = ops[0]
        if "[" in v:
            return ["{ uint64_t _v = s.rd(s.gpr[4], 8); s.gpr[4] += 8; %s }"
                    % self.mem_store(v[v.find("["):], "_v", 64, None)]
        return [reg_write(v, "s.rd(s.gpr[4], 8)").rstrip(";") +
                " s.gpr[4] += 8;"]

    # ---- SSE scalar (lane 0; upper lanes preserved) ----
    def emit_sse(self, mnem, ops):
        m = mnem.upper()
        if m in ("MOVSS", "MOVSD", "MOVD"):
            return self.do_movs(m, ops)
        if m in ("ADDSS", "SUBSS", "MULSS", "DIVSS", "MAXSS", "MINSS"):
            return self.do_ssarith(m, ops)
        if m in ("COMISS", "UCOMISS"):
            return self.do_comiss(ops)
        if m in ("CVTTSS2SI", "CVTSI2SS", "CVTSS2SI", "CVTSS2SD",
                 "CVTSD2SS"):
            return self.do_cvt1(m, ops)
        if m in ("MOVAPS", "MOVUPS", "MOVAPD", "MOVUPD", "MOVDQ", "MOVDQU",
                 "MOVQ"):
            return self.do_mov128v(ops)
        if m in ("ANDPS", "ORPS", "XORPS", "ANDPD", "ORPD", "XORPD",
                 "ANDNPS", "ANDNPD"):
            return self.do_bit128(m, ops)
        if m in ("CVTPS2PD", "CVTPD2PS", "CVTDQ2PS", "CVTPS2DQ",
                 "CVTPD2DQ", "CVTDQ2PD"):
            return self.do_cvtv(m, ops)
        if m in ("SHUFPS", "UNPCKLPS", "UNPCKHPS", "BLENDVPS"):
            return ["/*UNHANDLED-VEC %s %s*/(void)0;" % (m, ops)]
        return None

    def x_lane(self, op):
        o = op.strip().lower()
        if o in GPR or o in SUB:
            return "(s.x_f32_from_int(%s))" % reg_read(o)
        if o.startswith("xmm") and "[" not in o:
            return "s.x_f32(%d)" % int(o[3:])
        inner = o[o.find("["):] if "[" in o else o
        return "(%s)" % self.mem_val(inner, 32, "f32")

    def set_lane(self, op, expr):
        o = op.strip().lower()
        if o.startswith("xmm") and "[" not in o:
            return "s.set_x_f32(%d, (float)(%s));" % (int(o[3:]), expr)
        inner = o[o.find("["):] if "[" in o else o
        return self.mem_store(inner, expr, 32, "f32")

    def do_movs(self, m, ops):
        dst, src = ops[0], ops[1]
        d, s = dst.strip().lower(), src.strip().lower()
        if m == "MOVD":
            # int<->lane0 bit transfer
            if d.startswith("xmm") and "[" not in d:
                x = int(d[3:])
                if s in GPR or s in SUB:
                    return ["s.xmm[%d][0] = (s.xmm[%d][0] & "
                            "0xFFFFFFFF00000000ULL) | ((uint64_t)(uint32_t)"
                            "(%s));" % (x, x, reg_read(s))]
                inner = src[src.find("["):]
                return ["{ uint32_t _b = (uint32_t)%s; s.xmm[%d][0] = "
                        "(s.xmm[%d][0] & 0xFFFFFFFF00000000ULL) | _b; }"
                        % (self.mem_val(inner, 32, None), x, x)]
            v = self.val_of(src, 32)
            if "[" in dst:
                return [self.mem_store(dst[dst.find("["):], v, 32, None)]
            return [reg_write(dst, v)]
        if m == "MOVSD":
            # full 64-bit transfer (low quad), upper quad preserved
            if d.startswith("xmm") and "[" not in d:
                x = int(d[3:])
                if s.startswith("xmm") and "[" not in s:
                    return ["s.xmm[%d][0] = s.xmm[%d][0];"
                            % (x, int(s[3:]))]
                inner = src[src.find("["):]
                return ["s.xmm[%d][0] = %s;" % (x, self.mem_val(inner, 64,
                                                                None))]
            inner = dst[dst.find("["):]
            x = int(s[3:])
            return ["s.wr(%s, s.xmm[%d][0], 8);"
                    % (self.addr_expr(parse_mem(inner[1:-1])), x)]
        # MOVSS: lane-0 float, upper preserved; mem forms transfer 32 bits
        if d.startswith("xmm") and "[" not in d:
            x = int(d[3:])
            if s.startswith("xmm") and "[" not in s:
                return ["s.set_x_f32(%d, s.x_f32(%d));"
                        % (x, int(s[3:]))]
            inner = src[src.find("["):]
            return ["s.set_x_f32(%d, %s);"
                    % (x, self.mem_val(inner, 32, "f32"))]
        inner = dst[dst.find("["):]
        return ["s.wrf(%s, s.x_f32(%d));"
                % (self.addr_expr(parse_mem(inner[1:-1])), int(s[3:]))]

    def do_ssarith(self, m, ops):
        dst = ops[0]
        d = dst.strip().lower()
        if not (d.startswith("xmm") and "[" not in d):
            return ["/*UNHANDLED %s %s*/(void)0;" % (m, ops)]
        x = int(d[3:])
        a = "s.x_f32(%d)" % x
        b = self.x_lane(ops[1])
        op = {"ADDSS": "+", "SUBSS": "-", "MULSS": "*",
              "DIVSS": "/"}[m] if m in ("ADDSS", "SUBSS", "MULSS",
                                        "DIVSS") else None
        if op:
            return ["s.set_x_f32(%d, (float)((%s) %s (%s)));" % (x, a, op,
                                                                  b)]
        if m == "MAXSS":
            return ["s.set_x_f32(%d, (float)(((%s) >= (%s)) ? (%s) : (%s)));"
                    % (x, a, b, a, b)]
        return ["s.set_x_f32(%d, (float)(((%s) <= (%s)) ? (%s) : (%s)));"
                % (x, a, b, a, b)]

    def do_comiss(self, ops):
        return ["s.cmp_ss((float)(%s), (float)(%s));"
                % (self.x_lane(ops[0]), self.x_lane(ops[1]))]

    def do_cvt1(self, m, ops):
        dst, src = ops[0], ops[1]
        d, s = dst.strip().lower(), src.strip().lower()
        if m == "CVTTSS2SI":
            # x86 truncation with out-of-range saturation (0x80000000).
            # width from dst reg.
            w = 64 if d in GPR else SUB.get(d, (None, 32, 0))[1]
            e = self.x_lane(src)
            if w == 64:
                cv = ("([](float _f){ if (__builtin_isnan(_f) || _f >= "
                      "9.223372036854776e18f || _f < -9.223372036854776e18f)"
                      " return (int64_t)INT64_MIN; return (int64_t)_f; }(%s))")
            else:
                cv = ("([](float _f){ if (__builtin_isnan(_f) || _f >= "
                      "2147483648.0f || _f < -2147483648.0f) return "
                      "(int32_t)INT32_MIN; return (int32_t)_f; }(%s))")
            if "[" in dst:
                inner = dst[dst.find("["):]
                sw = self.op_width(dst)
                return [self.mem_store(inner, cv % e, sw, None)]
            return [reg_write(dst, cv % e)]
        if m == "CVTSI2SS":
            x = int(d[3:])
            sw = self.op_width(src)
            se = "((int64_t)(%s))" % self.val_of(src, 64) if sw == 64 else \
                "((int32_t)(%s))" % self.val_of(src, 32)
            return ["s.set_x_f32(%d, (float)(%s));" % (x, se)]
        if m == "CVTSS2SD":
            x = int(d[3:])
            return ["s.set_x_f64(%d, (double)(%s));" % (x,
                                                        self.x_lane(src))]
        # CVTSD2SS: lane double -> lane float
        x = int(d[3:])
        if s.startswith("xmm") and "[" not in s:
            return ["s.set_x_f32(%d, (float)s.x_f64(%d));"
                    % (x, int(s[3:]))]
        inner = src[src.find("["):]
        return ["s.set_x_f32(%d, (float)%s);"
                % (x, self.mem_val(inner, 64, "f64"))]

    def do_mov128v(self, ops):
        dst, src = ops[0], ops[1]
        d, s = dst.strip().lower(), src.strip().lower()
        dx, sx = d.startswith("xmm") and "[" not in d, \
            s.startswith("xmm") and "[" not in s
        if dx:
            x = int(d[3:])
            if sx:
                y = int(s[3:])
                return ["s.xmm[%d][0] = s.xmm[%d][0]; "
                        "s.xmm[%d][1] = s.xmm[%d][1];" % (x, y, x, y)]
            inner = src[src.find("["):]
            return ["{ unsigned __int128 _v = %s; s.xmm[%d][0] = "
                    "(uint64_t)_v; s.xmm[%d][1] = (uint64_t)(_v >> 64); }"
                    % (self.mem_val(inner, 128, None), x, x)]
        inner = dst[dst.find("["):]
        y = int(s[3:])
        return ["s.wr128(%s, ((unsigned __int128)s.xmm[%d][1] << 64) | "
                "s.xmm[%d][0]);" % (self.addr_expr(
                    parse_mem(inner[1:-1])), y, y)]

    def do_bit128(self, m, ops):
        dst = ops[0]
        d = dst.strip().lower()
        if not (d.startswith("xmm") and "[" not in d):
            return ["/*UNHANDLED %s %s*/(void)0;" % (m, ops)]
        x = int(d[3:])
        s = ops[1].strip().lower()
        if s.startswith("xmm") and "[" not in s and int(s[3:]) == x:
            if m in ("XORPS", "XORPD"):
                return ["s.xmm[%d][0] = 0; s.xmm[%d][1] = 0;" % (x, x)]
            if m in ("ORPS", "ORPD", "ANDPS", "ANDPD"):
                return ["(void)0;  // %s self-reg identity" % m]
        if s.startswith("xmm") and "[" not in s:
            y = int(s[3:])
            e = "(s.xmm[%d][0] | ((unsigned __int128)s.xmm[%d][1] << 64))" \
                % (y, y)
        else:
            inner = ops[1][ops[1].find("["):]
            e = "(%s)" % self.mem_val(inner, 128, None)
        op = {"ANDPS": "&", "ANDPD": "&", "ORPS": "|", "ORPD": "|",
              "XORPS": "^", "XORPD": "^"}[m]
        if m in ("ANDNPS", "ANDNPD"):
            e = "(~((unsigned __int128)s.xmm[%d][0] | " \
                "((unsigned __int128)s.xmm[%d][1] << 64)) & %s)" % (x, x,
                                                                     e)
            op = "&"
            cur = ("((unsigned __int128)0)")
        else:
            cur = ("((unsigned __int128)s.xmm[%d][0] | "
                   "((unsigned __int128)s.xmm[%d][1] << 64))") % (x, x)
        return ["{ unsigned __int128 _v = (%s) %s (%s); s.xmm[%d][0] = "
                "(uint64_t)_v; s.xmm[%d][1] = (uint64_t)(_v >> 64); }"
                % (cur, op, e, x, x)]

    def do_cvtv(self, m, ops):
        dst, src = ops[0], ops[1]
        x = int(dst.strip().lower()[3:])
        s = src.strip().lower()
        if m == "CVTPS2PD":
            if s.startswith("xmm") and "[" not in s:
                y = int(s[3:])
                lo = ("(double)(float)(uint32_t)s.xmm[%d][0]" % y)
                hi = ("(double)(float)(uint32_t)(s.xmm[%d][0] >> 32)" % y)
            else:
                inner = src[src.find("["):]
                e = self.mem_val(inner, 64, None)
                lo = "(double)(float)(uint32_t)(%s)" % e
                hi = "(double)(float)(uint32_t)((%s) >> 32)" % e
            return ["{ double _lo = %s, _hi = %s; uint64_t _u0, _u1;"
                    % (lo, hi),
                    "  memcpy(&_u0, &_lo, 8); memcpy(&_u1, &_hi, 8);",
                    "  s.xmm[%d][0] = _u0; s.xmm[%d][1] = _u1; }" % (x, x)]
        if m == "CVTPD2PS":
            if s.startswith("xmm") and "[" not in s:
                y = int(s[3:])
                lo = "(float)s.x_f64(%d)" % y
                hi = ("(float)([&](){ double _d; uint64_t _u = s.xmm[%d][1];"
                      " memcpy(&_d, &_u, 8); return _d; }())" % y)
            else:
                inner = src[src.find("["):]
                e = self.mem_val(inner, 128, None)
                lo = ("(float)([&](){ double _d; uint64_t _u = "
                      "(uint64_t)(%s); memcpy(&_d, &_u, 8); return _d; }())"
                      % e)
                hi = ("(float)([&](){ double _d; uint64_t _u = "
                      "(uint64_t)((%s) >> 64); memcpy(&_d, &_u, 8); "
                      "return _d; }())" % e)
            return ["{ uint32_t _lo = PesState::bits(%s), "
                    "_hi = PesState::bits(%s);" % (lo, hi),
                    "  s.xmm[%d][0] = ((uint64_t)_hi << 32) | _lo; "
                    "s.xmm[%d][1] = 0; }" % (x, x)]
        if m == "CVTDQ2PS":
            if s.startswith("xmm") and "[" not in s:
                y = int(s[3:])
                lanes = ["(float)(int32_t)(uint32_t)(s.xmm[%d][%d] >> %d)"
                         % (y, (i // 2), (i % 2) * 32) for i in range(4)]
            else:
                inner = src[src.find("["):]
                e = self.mem_val(inner, 128, None)
                lanes = ["(float)(int32_t)(uint32_t)((%s) >> %d)"
                         % (e, i * 32) for i in range(4)]
            return ["{ uint32_t _l0 = PesState::bits(%s), _l1 = "
                    "PesState::bits(%s), _l2 = PesState::bits(%s), _l3 = "
                    "PesState::bits(%s);" % tuple(lanes),
                    "  s.xmm[%d][0] = ((uint64_t)_l1 << 32) | _l0; "
                    "s.xmm[%d][1] = ((uint64_t)_l3 << 32) | _l2; }"
                    % (x, x)]
        return ["/*UNHANDLED-VEC %s %s*/(void)0;" % (m, ops)]

    # ---- branches ----
    def emit_branch(self, mnem, ops, addr, ctx):
        m = mnem.upper()
        if m == "RET":
            return ["return;"]
        if m == "CALL":
            tgt = "0x%X" % imm_val(ops[0])
            stub = (ctx.get("stubs") or {}).get(tgt)
            if stub is not None:
                # Spec-stubbed callee (out-of-set/live): mirror run.py's
                # stub semantics as a fixed return. Pilot specs use
                # "return" (RAX = value); "return_xmm0" also handled.
                v = stub.get("value", 0)
                vv = int(v, 16) if isinstance(v, str) else int(v)
                if stub.get("do", "return") == "return_xmm0":
                    return ["{ uint32_t _b = 0x%08XU; s.set_x_f32(0, "
                            "PesState::f32(_b)); }" % (vv & 0xFFFFFFFF)]
                return ["s.gpr[0] = 0x%XULL;" % (vv & 0xFFFFFFFFFFFFFFFF)]
            if tgt in self.pilot:
                self.siblings.add(tgt)
                fn = "pes_fn_%s" % tgt[2:]
                return ["{ s.gpr[4] -= 8; s.wr(s.gpr[4], 0ULL, 8); %s(s); "
                        "s.gpr[4] += 8; }" % fn]
            return ["/*OUT-OF-SET CALL %s*/ s.gpr[0] = 0;" % tgt]
        if m == "JMP":
            o = ops[0].strip()
            if is_imm(o):
                # In-range target: local goto. Out-of-range target: it is a
                # tail call to a sibling (the pilot set is call-closed);
                # emit a direct call + return, never an undefined goto.
                tgt = "0x%X" % imm_val(o)
                addrs = ctx.get("addrs", set())
                if not addrs or imm_val(o) in addrs:
                    return ["goto L_%X;" % imm_val(o)]
                if tgt in self.pilot:
                    self.siblings.add(tgt)
                    return ["{ pes_fn_%s(s); return; }" % tgt[2:]]
                return ["/*OUT-OF-RANGE JMP %s*/return;" % tgt]
            # JMP reg: switch dispatch (resolved in prepass) or tailjump
            info = ctx.get("jmpreg", {}).get(addr)
            if info and info.get("table") is not None:
                tva = info["table"]
                words = info["words"]
                cases = sorted(set((i, w) for i, w in enumerate(words)))
                self.tables[tva] = {"words": words,
                                    "base": info["base"]}
                idx = reg_read(ops[0])
                lines = ["{ uint32_t _i = (uint32_t)(%s); switch (_i) {"
                         % idx]
                seen = {}
                for i, w in cases:
                    tgt = info["base"] + w
                    if tgt not in seen:
                        seen[tgt] = []
                    seen[tgt].append(i)
                for tgt in sorted(seen):
                    for i in sorted(seen[tgt]):
                        lines.append("  case %d:;" % i)
                    lines.append("    goto L_%X;" % tgt)
                lines.append("  default: goto L_%X;" % info["default"])
                lines.append("  } }")
                return lines
            tgt = info.get("tail") if info else None
            if tgt:
                fn = "pes_fn_%s" % tgt[2:]
                return ["{ %s(s); return; }" % fn]
            return ["/*UNRESOLVED JMP %s*/return;" % o]
        cc = cc_of(m)
        if cc:
            return ["if (s.cond(\"%s\")) goto L_%X;" % (cc,
                                                        imm_val(ops[0]))]
        return None

    def emit_set_cmov(self, mnem, ops):
        m = mnem.upper()
        if m.startswith("SET"):
            cc = cc_of(m)
            d = ops[0].strip().lower()
            e = "(s.cond(\"%s\") ? 1ULL : 0ULL)" % cc
            if d in GPR or d in SUB:
                return [reg_write(d, e)]
            return ["/*SET-MEM*/(void)0;"]
        if m.startswith("CMOV"):
            cc = cc_of(m)
            d = ops[0]
            w = 64 if d.strip().lower() in GPR else SUB.get(
                d.strip().lower(), (None, 32, 0))[1]
            sv = self.val_of(ops[1], w)
            dv = self.val_of(d, w)
            e = "((s.cond(\"%s\")) ? (uint64_t)(%s) : (uint64_t)(%s))" % (
                cc, sv, dv)
            return [reg_write(d, e)]
        return None


def split_ops(s):
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
        m = re.match(r"^([0-9a-f]+):\s*([A-Z0-9]+)\s*(.*)$", l)
        if m:
            out.append((int(m.group(1), 16), m.group(2),
                        split_ops(m.group(3))))
    return out


def capstone_decode(image_path, va):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    img = Image(image_path)
    code = img.read(va, 0x2000)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = False
    out = []
    # Multi-ret functions: keep decoding past RET while a known branch
    # target still lies ahead. Stop at the first RET with nothing pending.
    pending = set()
    for ins in md.disasm(bytes(code), va):
        out.append((ins.address, ins.mnemonic.upper(),
                    split_ops(ins.op_str)))
        m = ins.mnemonic.lower()
        if m in ("jmp", "jz", "jnz", "jc", "jnc", "ja", "jbe", "jl",
                 "jge", "jle", "jg", "js", "jns", "jp", "jnp",
                 "jo", "jno"):
            try:
                t = int(ins.op_str.strip().split(",")[0].strip(), 16)
                if t >= va:
                    pending.add(t)
            except ValueError:
                pass
        if m == "ret":
            pending = {t for t in pending if t > ins.address}
            if not pending:
                break
    return out


def resolve_rip(lines, image_path, va):
    """Rewrite [rip + disp] operands to absolute [0x..] VAs.

    The bridge prints absolute VAs but capstone prints RIP-relative form.
    Resolved with capstone's own instruction sizes: target = next_addr +
    disp. Only touches operands mentioning rip; bridge text is unchanged.
    """
    if not any("rip" in (op.lower()) for _, _, ops in lines for op in ops):
        return lines
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    img = Image(image_path)
    code = img.read(va, 0x2000)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    sizes = {}
    for ins in md.disasm(bytes(code), va):
        sizes[ins.address] = ins.size
        if ins.mnemonic == "ret":
            break
    fixed = []
    for addr, mnem, ops in lines:
        size = sizes.get(addr, 0)
        new_ops = []
        for op in ops:
            lo = op.lower()
            if "rip" in lo:
                m = re.search(r"rip\s*\+\s*(-?0x[0-9a-f]+|-?\d+)", lo)
                if m and size:
                    disp = int(m.group(1), 16) if "0x" in m.group(1) \
                        else int(m.group(1))
                    tgt = addr + size + disp
                    op = re.sub(r"\[rip\s*\+\s*[^]]+\]",
                                "[0x%X]" % tgt, op, flags=re.I)
                    op = re.sub(r"\brip\b", "0x%X" % (addr + size), op,
                                flags=re.I)
            new_ops.append(op)
        fixed.append((addr, mnem, new_ops))
    return fixed


def prepass(lines, img):
    """Labels = in-range branch targets; JMP-reg resolution.

    Switch pattern (all 9 in the set):
      LEA base,[0x140000000]; CMP idx,N; JA default;
      MOV r,[base + idx*4 + disp]; ADD r,base; JMP r
    Resolved by backward scan from each JMP reg. Anything else is a
    tailjump to an in-set VA (recorded; the caller fills the target).
    """
    addrs = {a for a, _, _ in lines}
    byaddr = {a: (mn, ops) for a, mn, ops in lines}
    order = [a for a, _, _ in lines]
    pos = {a: i for i, a in enumerate(order)}
    labels = set()
    jmpreg = {}
    for addr, mnem, ops in lines:
        m = mnem.upper()
        if m in CCMAP and ops and is_imm(ops[0]):
            t = imm_val(ops[0])
            if t in addrs:
                labels.add(t)
        elif m == "JMP" and ops and is_imm(ops[0].strip()):
            t = imm_val(ops[0])
            if t in addrs:
                labels.add(t)
    for addr, mnem, ops in lines:
        if mnem.upper() != "JMP" or not ops or is_imm(ops[0].strip()):
            continue
        r = ops[0].strip().lower()
        i = pos[addr]
        base = None
        tdisp = None
        count = None
        default = None
        for j in range(i - 1, max(-1, i - 12), -1):
            mn2, ops2 = byaddr[order[j]]
            if mn2.upper() == "ADD" and len(ops2) == 2 and \
                    ops2[0].strip().lower() == r:
                base = ops2[1].strip().lower()
                break
        for j in range(i - 1, max(-1, i - 12), -1):
            mn2, ops2 = byaddr[order[j]]
            if mn2.upper() == "MOV" and len(ops2) == 2 and \
                    ops2[0].strip().lower() == r and "[" in ops2[1]:
                inner = ops2[1][ops2[1].find("[") + 1:ops2[1].rfind("]")]
                mp = parse_mem(inner)
                if mp and mp["scale"] == 4 and mp["index"] is not None:
                    tdisp = mp["disp"]
                    break
        baseva = None
        for j in range(i - 1, max(-1, i - 20), -1):
            mn2, ops2 = byaddr[order[j]]
            if mn2.upper() == "LEA" and len(ops2) == 2 and \
                    base is not None and \
                    ops2[0].strip().lower() == base and "[" in ops2[1]:
                inner = ops2[1][ops2[1].find("[") + 1:ops2[1].rfind("]")]
                mp = parse_mem(inner)
                if mp and mp["absolute"] is not None:
                    baseva = mp["absolute"]
                    break
        for j in range(i - 1, max(-1, i - 20), -1):
            mn2, ops2 = byaddr[order[j]]
            if mn2.upper() == "JA" and ops2 and is_imm(ops2[0]):
                default = imm_val(ops2[0])
                if j - 1 >= 0:
                    mn3, ops3 = byaddr[order[j - 1]]
                    if mn3.upper() == "CMP" and len(ops3) == 2 and \
                            is_imm(ops3[1]):
                        count = imm_val(ops3[1]) + 1
                break
        if baseva is not None and tdisp is not None and count is not None:
            tva = baseva + tdisp
            raw = img.read(tva, count * 4)
            words = list(struct.unpack("<%dI" % count, raw))
            jmpreg[addr] = {"table": tva, "words": words, "base": baseva,
                            "default": default}
            for w in words:
                if baseva + w in addrs:
                    labels.add(baseva + w)
            if default in addrs:
                labels.add(default)
        else:
            jmpreg[addr] = {"table": None, "tail": None}
    return labels, jmpreg

def emit_function(em, va, lines, labels, jmpreg, pilot, stubs=None):
    addrs = {a for a, _, _ in lines}
    ctx = {"jmpreg": jmpreg, "addrs": addrs, "stubs": stubs or {}}
    body = []
    for addr, mnem, ops in lines:
        if addr in labels:
            body.append("L_%X:;" % addr)
        m = mnem.upper()
        out = None
        if m in ("MOV", "MOVSX", "MOVSXD", "MOVZX", "LEA", "XCHG", "ADD",
                 "SUB", "CMP", "TEST", "AND", "OR", "XOR", "ADC", "SBB",
                 "INC", "DEC", "NEG", "NOT", "SHL", "SHR", "SAR", "IMUL",
                 "MUL", "BT", "BTC", "BTR", "BTS", "CDQE", "CDQ", "CQO",
                 "CWD", "CBW", "CWDE", "PUSH", "POP", "LEAVE", "NOP"):
            out = em.emit_alu(m, ops)
        elif m in ("MOVSS", "MOVSD", "MOVD", "ADDSS", "SUBSS", "MULSS",
                   "DIVSS", "MAXSS", "MINSS", "COMISS", "UCOMISS",
                   "CVTTSS2SI", "CVTSI2SS", "CVTSS2SI", "CVTSS2SD",
                   "CVTSD2SS", "MOVAPS", "MOVUPS", "MOVAPD", "MOVUPD",
                   "ANDPS", "ORPS", "XORPS", "ANDPD", "ORPD", "XORPD",
                   "ANDNPS", "ANDNPD", "CVTPS2PD", "CVTPD2PS", "CVTDQ2PS",
                   "CVTPS2DQ", "CVTPD2DQ", "CVTDQ2PD"):
            out = em.emit_sse(m, ops)
        elif m in CCMAP or m in ("RET", "CALL", "JMP"):
            out = em.emit_branch(m, ops, addr, ctx)
        elif m.startswith("SET") or m.startswith("CMOV"):
            out = em.emit_set_cmov(m, ops)
        if out is None:
            out = ["/*UNHANDLED %s %s*/(void)0;" % (m, ", ".join(ops))]
        body.extend(out)
    return body


def const_decls(em):
    decls = []
    for (va, n), blob in sorted(em.consts.items()):
        vals = struct.unpack("<%dI" % (len(blob) // 4), blob[:len(blob) // 4 * 4])
        decls.append("// image constant @ 0x%X (%d bytes)" % (va, n))
        if n == 4:
            f = struct.unpack("<f", blob)[0]
            decls.append("static constexpr uint32_t k_c%X_w = 0x%08XU;" % (va, vals[0]))
            decls.append("static constexpr float k_c%X_f = %af; // %r" % (va, f, f))
        elif n == 8:
            decls.append("static constexpr uint64_t k_c%X_q = 0x%016XULL;" % (va, struct.unpack("<Q", blob)[0]))
            # Non-finite doubles (%a renders nan/inf, invalid C++): always
            # reference through the bits. Finite doubles keep a literal too.
            import math as _math
            if _math.isfinite(struct.unpack("<d", blob)[0]):
                decls.append("static constexpr double k_c%X_d = %a; // %r" % (va, struct.unpack("<d", blob)[0], struct.unpack("<d", blob)[0]))
        elif n == 16:
            decls.append("static constexpr unsigned __int128 k_c%X = ((unsigned __int128)0x%016XULL << 64) | 0x%016XULL;" % (va, struct.unpack("<Q", blob[8:])[0], struct.unpack("<Q", blob[:8])[0]))
        else:
            decls.append("static constexpr uint8_t k_c%X[%d] = {%s};" % (va, n, ", ".join("0x%02X" % b for b in blob)))
            if n % 4 == 0:
                decls.append("static constexpr uint32_t k_c%X_w = 0x%08XU;" % (va, vals[0]))
                f = struct.unpack("<f", blob[:4])[0]
                decls.append("static constexpr float k_c%X_f = %af; // %r" % (va, f, f))
    return decls


def translate_one(image_path, asm_text, va, pilot, capstone_fallback=True,
                  stubs=None):
    img = Image(image_path)
    lines = parse_lines(asm_text)
    if not lines and capstone_fallback:
        v = va if isinstance(va, int) else int(va, 16)
        lines = capstone_decode(image_path, v)
    if not lines:
        return None, ["// EMPTY: no instructions at %s" % va]
    v = va if isinstance(va, int) else int(va, 16)
    lines = resolve_rip(lines, image_path, v)
    labels, jmpreg = prepass(lines, img)
    em = Emitter(img, va, pilot)
    body = emit_function(em, va, lines, labels, jmpreg, pilot, stubs)
    # tail jumps to in-set VAs: fill from the sibling set
    out = []
    for b in body:
        out.append(b)
    head = ["// Generated by tools/dev/translate/xlate.py: PES function %s." % va,
            "// Constants baked from the user's image at build time (VA-cited).",
            "// Build tree only; never committed.",
            '#include "pes_state.hpp"',
            '#include "pes_cond.inl"',
            ""]
    head.extend(const_decls(em))
    head.append("")
    head.append("void pes_fn_%s(PesState& s);" % va[2:])
    for sib in sorted(em.siblings):
        if sib != va:
            head.append("void pes_fn_%s(PesState& s);" % sib[2:])
    for (tva, n), blob in sorted(em.consts.items()):
        pass
    src = head + ["", "void pes_fn_%s(PesState& s) {" % va[2:]] + ["  " + b for b in out] + ["}"]
    return src, []


def main():
    ap = __import__("argparse").ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--asm", required=True)
    ap.add_argument("--va", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--pilot", nargs="*", default=[])
    a = ap.parse_args()
    va = "0x" + a.va.lower().replace("0x", "").upper()
    pilot = {"0x" + v.lower().replace("0x", "").upper() for v in a.pilot}
    text = open(a.asm, encoding="utf-8", errors="replace").read()
    src, _ = translate_one(a.image, text, va, pilot)
    if src is None:
        raise SystemExit("xlate: no instructions for %s" % va)
    # resolve tail jumps: JMP out-of-range to in-set VA -> sibling call
    with open(a.out, "w") as f:
        f.write("\n".join(src) + "\n")
    print("wrote %s (%d lines)" % (a.out, len(src)))


if __name__ == "__main__":
    main()
