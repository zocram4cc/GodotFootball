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
import json
import argparse
import os
import re
import struct
import sys
import flow

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
        self._secs = None

    def read(self, va, n):
        off = va - BASE
        if off < 0 or off + n > self.size:
            raise SystemExit("xlate: image unreadable at %#x" % va)
        self.f.seek(off)
        blob = self.f.read(n)
        if len(blob) != n:
            raise SystemExit("xlate: image unreadable at %#x" % va)
        return blob

    def writable(self, va):
        """True if the VA lives in a writable image section (.data etc).

        Reads the section table from the image itself (no bridge). The
        unpacked dump keeps the packed layout's section table, so VAs
        map the same way. Unknown VA -> writable (safe: live memory
        read, never a stale baked constant).
        """
        if self._secs is None:
            self._secs = []
            try:
                self.f.seek(0x3C)
                pe = int.from_bytes(self.f.read(4), "little")
                self.f.seek(pe + 6)
                nsec = int.from_bytes(self.f.read(2), "little")
                self.f.seek(pe + 20)
                ohsize = int.from_bytes(self.f.read(2), "little")
                self.f.seek(pe + 24 + 24)
                base = int.from_bytes(self.f.read(8), "little")
                self.f.seek(pe + 24 + ohsize)
                for _ in range(min(nsec, 64)):
                    raw = self.f.read(40)
                    if len(raw) < 40:
                        break
                    vsize, rva = int.from_bytes(raw[8:12], "little"), \
                        int.from_bytes(raw[12:16], "little")
                    rsize = int.from_bytes(raw[16:20], "little")
                    flags = int.from_bytes(raw[36:40], "little")
                    size = max(vsize, rsize)
                    if size and rsize:
                        self._secs.append((base + rva, size,
                                           bool(flags & 0x80000000)))
            except OSError:
                pass
        for sva, ssize, wr in self._secs:
            if sva <= va < sva + ssize:
                return wr
        return True


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
    # Cast to int64 BEFORE the right shift: the >> must be arithmetic.
    # ((int64_t)(bits << sh) >> sh) sign-extends; the old order shifted
    # the unsigned value (logical) and cast after, which zero-extends.
    return "(((int64_t)(((uint64_t)(%s) << %d))) >> %d)" % (expr, sh, sh)


def find_guards(lines):
    """(cmp index, index register parent, bound, jcc index, jcc, target) for
    each CMP reg, imm directly followed by a Jcc to a VA in the body."""
    out = []
    for i in range(len(lines) - 1):
        _, m, ops = lines[i]
        if m != "CMP" or len(ops) != 2 or not is_imm(ops[1]):
            continue
        _, m2, ops2 = lines[i + 1]
        if not m2.startswith("J") or cc_of(m2) is None or not is_imm(ops2[0]):
            continue
        out.append((i, PARENT.get(ops[0].strip().lower()), imm_val(ops[1]),
                    i + 1, m2.upper(), imm_val(ops2[0])))
    return out


def bounded_index(em, p, ia):
    """How many index values (0..n-1) the guard before access ia admits for
    index register parent p, or None when no guard governs the access."""
    lines = em.lines
    k = ia - 1
    while k >= 0 and not (lines[k][2] and
                          PARENT.get(lines[k][2][0].strip().lower()) == p):
        k -= 1
    if k < 0 or lines[k][1] not in ("MOVSXD", "MOVZX", "MOV") or \
            len(lines[k][2]) != 2:
        return None
    src = PARENT.get(lines[k][2][1].strip().lower())
    acc = lines[ia][0]
    count = None
    for _, greg, bound, ji, mn, tgt in em.guards:
        if greg != src or ji >= k:
            continue
        if any(lines[x][2] and
               PARENT.get(lines[x][2][0].strip().lower()) == src
               for x in range(ji + 1, k)):
            continue
        if lines[ji][0] < acc < tgt:  # access on the fall-through path
            if any(lines[x][0] in em.labels for x in range(ji + 1, ia + 1)):
                continue
            if mn in ("JA", "JNBE", "JG", "JNLE"):
                count = bound + 1
            elif mn in ("JAE", "JNB", "JGE", "JNL"):
                count = bound
        elif acc >= tgt:  # access on the taken path
            if any(lines[x][0] in em.labels and lines[x][0] > tgt
                   for x in range(ji + 1, ia + 1)):
                continue
            if mn in ("JB", "JNAE", "JL", "JNGE"):
                count = bound
            elif mn in ("JBE", "JNA", "JLE", "JNG"):
                count = bound + 1
    return count


# Instructions that write registers their operand list does not name; a CALL clobbers the caller-saved set.
IMPLICIT_WRITERS = {"MUL", "DIV", "IDIV", "CDQ", "CQO", "CWD", "CDQE", "CBW", "CWDE", "CPUID", "RDTSC", "LEAVE", "MOVSB",
                    "MOVSW", "MOVSD", "MOVSQ", "STOSB", "STOSW", "STOSD", "STOSQ", "LODSB", "LODSW", "LODSD", "LODSQ",
                    "SCASB", "CMPSB", "SYSCALL"}
NO_REG_WRITE = {"CMP", "TEST", "PUSH", "UCOMISS", "COMISS", "UCOMISD", "COMISD", "BT", "NOP", "RET"}
CALLER_SAVED = ("rax", "rcx", "rdx", "r8", "r9", "r10", "r11")


def written(m, ops):
    """The register parents instruction (m, ops) may write."""
    if m == "CALL":
        return CALLER_SAVED
    if m in IMPLICIT_WRITERS or (m == "IMUL" and len(ops) == 1):
        return ("rax", "rcx", "rdx", "rsi", "rdi")
    if not ops or m in NO_REG_WRITE or m.startswith("J"):
        return ()
    dests = [ops[0]] + (ops[1:2] if m in ("XCHG", "XADD") else [])
    return tuple(r for r in (PARENT.get(o.strip().lower()) for o in dests) if r in GPR) + \
        (("rax",) if m == "CMPXCHG" else ())


def reaching_defs(lines, jmpreg):
    """before[i]: register parent -> frozenset of the instruction indices whose write may reach instruction i (a
    register missing from the map holds a value from outside the function). Forward may-analysis over the CFG."""
    at = {a: i for i, (a, _, _) in enumerate(lines)}
    before = [None] * len(lines)
    before[0] = {}
    work = [0]
    while work:
        i = work.pop()
        a, m, ops = lines[i]
        out = dict(before[i])
        for r in written(m, ops):
            out[r] = frozenset([i])
        succ = []
        info = jmpreg.get(a)
        if m == "JMP":
            if info and info.get("words"):
                succ = [info["base"] + w for w in info["words"]]
            elif ops and is_imm(ops[0]):
                succ = [imm_val(ops[0])]
        elif m != "RET":
            succ = [lines[i + 1][0]] if i + 1 < len(lines) else []
            if m.startswith("J") and ops and is_imm(ops[0]):
                succ.append(imm_val(ops[0]))
        for t in succ:
            j = at.get(t)
            if j is None:
                continue
            old = before[j]
            outside = frozenset([-1])  # a path on which the register was never written here
            new = out if old is None else {r: old.get(r, outside) | out.get(r, outside) for r in set(old) | set(out)}
            if new != old:
                before[j] = new
                work.append(j)
    return before


def image_value(em, d):
    """The image VA instruction d loads into its 64-bit register: a LEA of an image VA, or a MOV of a pointer stored in a
    read-only image slot. None for any other write."""
    if d < 0:
        return None
    _, m, ops = em.lines[d]
    if m not in ("LEA", "MOV") or len(ops) != 2 or "[" not in ops[1] or ops[0].strip().lower() not in GPR:
        return None
    mp = parse_mem(ops[1][ops[1].find("[") + 1:ops[1].rfind("]")])
    if not mp or mp["absolute"] is None or mp["base"] is not None or mp["index"] is not None or mp.get("seg"):
        return None
    va = mp["absolute"]
    if m == "LEA":
        return va if BASE <= va < BASE + em.img.size else None
    if BASE <= va and va + 8 <= BASE + em.img.size and not em.img.writable(va):
        p = struct.unpack("<Q", em.img.read(va, 8))[0]
        return p if BASE <= p < BASE + em.img.size else None
    return None


def reg_defs(em, breg, ia):
    return em.defs[ia].get(breg, frozenset()) if em.defs[ia] else frozenset()


def image_pointer(em, breg, ia):
    """The image VA register parent breg holds before instruction ia when every definition reaching it loads the same one
    (image_value); None otherwise."""
    vals = {image_value(em, d) for d in reg_defs(em, breg, ia)}
    return vals.pop() if len(vals) == 1 and None not in vals else None


def image_origins(em, breg, ia, seen=None):
    """The image VAs register parent breg before instruction ia may be derived from: image_value defs, followed through
    pointer arithmetic, register copies and LEAs (a loop-walked pointer keeps its origin)."""
    seen = set() if seen is None else seen
    out = set()
    for d in reg_defs(em, breg, ia):
        if (breg, d) in seen:
            continue
        seen.add((breg, d))
        v = image_value(em, d)
        if v is not None:
            out.add(v)
            continue
        if d < 0:
            continue
        _, m, ops = em.lines[d]
        if not ops or ops[0].strip().lower() not in GPR:
            continue
        if m in ("ADD", "SUB", "INC", "DEC", "AND"):
            regs = [ops[0]] + (ops[1:2] if m == "ADD" else [])
        elif m == "MOV":
            regs = ops[1:2]
        elif m == "LEA" and len(ops) == 2 and "[" in ops[1]:
            mp = parse_mem(ops[1][ops[1].find("[") + 1:ops[1].rfind("]")])
            regs = [r for r in (mp["base"], mp["index"]) if r] if mp else []
        else:
            continue
        for r in regs:
            p = PARENT.get(r.strip().lower())
            if p in GPR:
                out |= image_origins(em, p, d, seen)
    return out


def parse_mem(inner):
    # Split on + and - (but not on a leading sign or 0x digits). "rdi - 8"
    # -> ["rdi", "-8"]; "rcx+rax*4-0x10" works the same. A segment prefix ("gs:0x58", from "gs:[0x58]" by
    # split_ops) is the thread block's base: it is kept as seg and added to the address, never an absolute VA.
    text = inner.strip().lower()
    seg = None
    if text.startswith("gs:"):
        seg, text = "gs", text[3:]
    tmp = re.sub(r"\s*-\s*", "+-", text)
    parts = [p.strip() for p in tmp.split("+") if p.strip() != ""]
    base = None
    index = None
    scale = 1
    nums = []
    ok = True
    for p in parts:
        if p.split("*")[0] in ("riz", "eiz"):  # zero-index pseudo register (alignment padding) adds nothing
            continue
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
    absolute = nums[0] if (base is None and index is None and nums and seg is None) else None
    return {"base": base, "index": index, "scale": scale, "disp": disp,
            "absolute": absolute, "seg": seg}

# Named import slots (import_slots.py -> import_slots.json) and their models (import_stubs.json, the contract
# the snapshot harness runs). A CALL through a slot with a model is that model's effect, as the OS export has it.
IMPORT_SLOT_OP = re.compile(r"^(?:qword\s+ptr\s+)?\[\s*(0x[0-9a-f]+)\s*\]$")
_IMPORT_TABLES = None


def _import_tables():
    global _IMPORT_TABLES
    if _IMPORT_TABLES is None:
        here = os.path.dirname(os.path.abspath(__file__))
        slots = json.load(open(os.path.join(here, "import_slots.json")))["slots"]
        models = {}
        for e in json.load(open(os.path.join(here, "import_stubs.json")))["imports"]:
            models[(e["module"].lower(), e["export"])] = e
        _IMPORT_TABLES = (slots, models)
    return _IMPORT_TABLES


def absolute_slot(op):
    """The slot VA of an absolute memory operand (qword ptr [0x...]), or None."""
    m = IMPORT_SLOT_OP.match(op.strip().lower())
    return int(m.group(1), 16) if m else None


def import_model_lines(op, jump):
    """C++ for a CALL (jump=False) or JMP (jump=True) through an absolute slot that names a modelled export, or
    None (not a named import). The model runs exactly as the harness runs it. A call pushes its return address
    (0, as an indirect call records) and the model returns to it. A jump has no return address of its own: the
    model's ret returns to the caller, so the translated function returns after it."""
    slot_va = absolute_slot(op)
    if slot_va is None:
        return None
    slots, models = _import_tables()
    slot = slots.get("0x%X" % slot_va)
    if not slot or slot.get("kind") != "export":
        return None
    e = models.get((slot["module"].lower(), slot["export"]))
    if e is None:
        return None
    push_ret = "" if jump else "s.gpr[4] -= 8; s.wr(s.gpr[4], 0ULL, 8);"
    pop_ret = "" if jump else " s.gpr[4] += 8;"
    if e["do"] == "return":
        value = int(str(e["value"]), 0) & 0xFFFFFFFFFFFFFFFF
        return ["{ " + push_ret + " s.gpr[0] = %dULL;%s }" % (value, pop_ret)]
    if e["do"] == "memcpy":
        # Win64 memcpy(rcx=dst, rdx=src, r8=n): push rdi, push rsi, rep movsb (byte writes, forward), rax=dst,
        # rcx=0; the pops restore rsi and rdi (16 bytes of stack), then the ret (8 more) for a call.
        return ["{ " + push_ret +
                " s.gpr[4] -= 8; s.wr(s.gpr[4], s.gpr[7], 8);"
                " s.gpr[4] -= 8; s.wr(s.gpr[4], s.gpr[6], 8);"
                " uint64_t _dst = s.gpr[1], _src = s.gpr[2], _n = s.gpr[8];"
                " for (uint64_t _i = 0; _i < _n; ++_i) s.wr(_dst + _i, s.rd(_src + _i, 1), 1);"
                " s.gpr[0] = _dst; s.gpr[1] = 0;"
                " s.gpr[4] += 16;%s }" % pop_ret]
    if e["do"] == "memmove":
        # Win64 memmove(rcx=dst, rdx=src, r8=n), the code_hex model register for register: push rdi, push rsi; backward
        # byte copy when src < dst < src + n, else forward; rax = dst, rcx = 0, r9 = src + n when dst > src.
        return ["{ " + push_ret +
                " s.gpr[4] -= 8; s.wr(s.gpr[4], s.gpr[7], 8);"
                " s.gpr[4] -= 8; s.wr(s.gpr[4], s.gpr[6], 8);"
                " uint64_t _dst = s.gpr[1], _src = s.gpr[2], _n = s.gpr[8];"
                " s.gpr[0] = _dst; if (_dst > _src) s.gpr[9] = _src + _n;"
                " if (_dst > _src && _dst < _src + _n) { for (uint64_t _i = _n; _i-- > 0;) s.wr(_dst + _i, s.rd(_src + _i, 1), 1); }"
                " else { for (uint64_t _i = 0; _i < _n; ++_i) s.wr(_dst + _i, s.rd(_src + _i, 1), 1); }"
                " s.gpr[1] = 0; s.gpr[4] += 16;%s }" % pop_ret]
    if e["do"] == "qpc":
        # BOOL QueryPerformanceCounter(LARGE_INTEGER *count): stores the import_stubs value at *rcx, returns TRUE.
        value = int(str(e["value"]), 0) & 0xFFFFFFFFFFFFFFFF
        return ["{ " + push_ret + (" s.wr(s.gpr[1], %dULL, 8); s.gpr[0] = 1;%s }" % (value, pop_ret))]
    if e["do"] == "tebread":
        # DWORD read from the thread block at gs:[offset], zero-extended into rax (GetLastError: TEB.LastErrorValue).
        off = int(str(e["offset"]), 0)
        return ["{ " + push_ret + (" s.gpr[0] = (uint64_t)s.rd(s.gs_base + %dULL, 4);%s }" % (off, pop_ret))]
    if e["do"] == "tls_get":
        # TlsGetValue(index): TEB.TlsSlots[index] for index < 64 (x64 TEB +0x1480), else NULL.
        return ["{ " + push_ret + " uint64_t _i = (uint32_t)s.gpr[1];"
                " s.gpr[0] = _i < 64 ? s.rd(s.gs_base + 0x1480ULL + _i * 8, 8) : 0ULL;%s }" % pop_ret]
    if e["do"] == "tls_set":
        # TlsSetValue(index, value): TEB.TlsSlots[index] = value for index < 64; returns TRUE, else FALSE.
        return ["{ " + push_ret + " uint64_t _i = (uint32_t)s.gpr[1];"
                " if (_i < 64) s.wr(s.gs_base + 0x1480ULL + _i * 8, s.gpr[2], 8);"
                " s.gpr[0] = _i < 64 ? 1ULL : 0ULL;%s }" % pop_ret]
    # The three Wine models below mirror the hand-encoded code in import_stubs.json (code_hex) register for register:
    # the harness runs that code, so every register and byte the model leaves must match here (snapdiff compares them).
    if e["do"] == "srw_acquire_shared":
        # RtlAcquireSRWLockShared(rcx=lock): fast path only; [lock] += 0x10000; rax = rdx = old; rcx = new. The slow path
        # (exclusive or waiting bits) is int3 in the harness, so reaching it here is a trap, never a wrong value.
        return ["{ " + push_ret + " uint32_t _v = (uint32_t)s.rd(s.gpr[1], 4);"
                " if ((_v & 0xFFFFu) != 0) __builtin_trap();"
                " uint32_t _n = _v + 0x10000u; s.wr(s.gpr[1], (uint64_t)_n, 4);"
                " s.gpr[0] = _v; s.gpr[2] = _v; s.gpr[1] = _n;%s }" % pop_ret]
    if e["do"] == "srw_release_shared":
        # RtlReleaseSRWLockShared(rcx=lock): fast path only; [lock] -= 0x10000 when bit 0 is clear and the shared count
        # is nonzero; rax = old; rdx = count - 1; r8 = new; rcx unchanged. Any other case is int3 in the harness: a trap.
        return ["{ " + push_ret + " uint32_t _v = (uint32_t)s.rd(s.gpr[1], 4);"
                " if ((_v & 1u) != 0 || (_v >> 16) == 0) __builtin_trap();"
                " uint32_t _n = _v - 0x10000u; s.wr(s.gpr[1], (uint64_t)_n, 4);"
                " s.gpr[0] = _v; s.gpr[2] = (uint64_t)((_v >> 16) - 1u); s.gpr[8] = _n;%s }" % pop_ret]
    if e["do"] == "fls_get":
        # FlsGetValue(ecx=index): the inlined Wine walk. index-1 > 0xFEE or a null FLS vector (TEB+0x17C8) is the failure
        # path (TEB+0x68 = 87, rax = 0); else the block walk (16<<k slots per block), value = block[k][rem+1] (0 for a null
        # block), TEB+0x68 = 0. Registers follow the machine code: rax, rcx, rdx, r8, r10 as the harness leaves them.
        return ["{ " + push_ret +
                " uint32_t _ix = (uint32_t)s.gpr[1]; s.gpr[0] = _ix;"
                " uint32_t _m1 = _ix - 1u; s.gpr[1] = _m1;"
                " bool _inv = _m1 > 0xFEEu; uint64_t _self = 0, _fls = 0; uint32_t _k = 0, _e = _ix, _ax = _ix;"
                " if (!_inv) { _self = s.rd(s.gs_base + 0x30ULL, 8); s.gpr[1] = _self; _fls = s.rd(_self + 0x17C8ULL, 8);"
                "   s.gpr[10] = _fls; _inv = (_fls == 0); }"
                " if (!_inv && _ix > 0xFu) { uint64_t _r8 = 0x10u;"
                "   do { _k += 1; _e -= (uint32_t)_r8; _r8 = (uint32_t)(0x10u << (_k & 31u)); } while (_e >= (uint32_t)_r8);"
                "   s.gpr[8] = _r8; s.gpr[1] = _k; _ax = _e; }"
                " else if (!_inv) { s.gpr[1] = 0; }"
                " if (!_inv) { s.gpr[0] = _ax;"
                "   uint64_t _blk = s.rd(_fls + 0x10ULL + (uint64_t)s.gpr[1] * 8ULL, 8); s.gpr[1] = _blk;"
                "   if (_blk != 0) { _ax += 1u; s.gpr[0] = _ax; s.gpr[1] = s.rd(_blk + (uint64_t)_ax * 8ULL, 8); }"
                "   s.gpr[0] = s.gpr[1]; s.gpr[2] = _self; s.wr(_self + 0x68ULL, 0, 4); }"
                " else { _self = s.rd(s.gs_base + 0x30ULL, 8); s.gpr[2] = _self; s.wr(_self + 0x68ULL, 87, 4); s.gpr[0] = 0; }"
                "%s }" % pop_ret]
    if e["do"] == "setlasterror":
        # RtlSetLastWin32Error(ecx): TEB.LastErrorValue (gs:[0x30]+0x68) = ecx; rax = the TEB self pointer (the machine code).
        return ["{ " + push_ret + " uint64_t _self = s.rd(s.gs_base + 0x30ULL, 8); s.gpr[0] = _self;"
                " s.wr(_self + 0x68ULL, s.gpr[1] & 0xFFFFFFFFULL, 4);%s }" % pop_ret]
    if e["do"] == "isnan":
        # ucrtbase _isnan(xmm0 = double): the code_hex model register for register. The caller's home slot [rsp+8] (the
        # callee's rsp+8 after the return address) takes the bits of x; rcx = |x| bits; rdx = +inf bits; rax = 0 or 1.
        return ["{ " + push_ret + " uint64_t _b = s.xmm[0][0]; s.wr(s.gpr[4] + 8ULL, _b, 8);"
                " s.gpr[1] = _b & 0x7FFFFFFFFFFFFFFFULL; s.gpr[2] = 0x7FF0000000000000ULL;"
                " s.gpr[0] = (s.gpr[1] > s.gpr[2]) ? 1ULL : 0ULL;%s }" % pop_ret]
    if e["do"] == "floorf":
        # floorf(xmm0 = float): roundss xmm0,xmm0,9 (the code_hex model): the low float is the floor, bits 32-127 stay.
        return ["{ " + push_ret + " s.set_x_f32(0, pes_model_floorf(s.x_f32(0)));%s }" % pop_ret]
    if e["do"] == "finite":
        # ucrtbase _finite(xmm0 = double): the code_hex model register for register. The home slot [rsp+8] takes the bits of x;
        # rcx = +inf bits; rax = ((|x| bits - +inf bits) >> 63) = 1 for a finite x, else 0.
        return ["{ " + push_ret + " uint64_t _b = s.xmm[0][0]; s.wr(s.gpr[4] + 8ULL, _b, 8);"
                " s.gpr[1] = 0x7FF0000000000000ULL;"
                " s.gpr[0] = ((_b & 0x7FFFFFFFFFFFFFFFULL) - s.gpr[1]) >> 63;%s }" % pop_ret]
    if e["do"] == "fdtest":
        # ucrtbase _fdtest(rcx=float*): the code_hex model register for register. exp = the word at +2 & 0x7F80 (left in rdx);
        # all-ones exponent: 1 for an infinity (mantissa zero), else 2; else a zero is 0; else the exponent negated in dx
        # (rdx = that word) selects subnormal -2 (exponent 0) or normal -1. rax = the class, zero-extended.
        return ["{ " + push_ret + " uint32_t _hi = (uint32_t)s.rd(s.gpr[1] + 2ULL, 2), _lo = (uint32_t)s.rd(s.gpr[1], 2);"
                " uint32_t _exp = _hi & 0x7F80u; uint64_t _dx = _exp; uint32_t _r;"
                " if (_exp == 0x7F80u) _r = ((_hi & 0x7Fu) == 0 && _lo == 0) ? 1u : 2u;"
                " else if ((_hi & 0x7FFFu) == 0 && _lo == 0) _r = 0u;"
                " else { _dx = (uint32_t)(0u - _exp) & 0xFFFFu; _r = _exp != 0 ? 0xFFFFFFFFu : 0xFFFFFFFEu; }"
                " s.gpr[2] = _dx; s.gpr[0] = _r;%s }" % pop_ret]
    raise SystemExit("import_stubs.json: no translator model for do=%r (%s!%s)"
                     % (e["do"], slot["module"], slot["export"]))


# function VA -> {(site VA, image VA)}: the live reads of read-only image bytes each translated function still carries
# (Emitter.note_live_ro). gen_translated fails the generation on any: the GF runtime maps no image, so such a read returns 0.
LIVE_RO = {}

# Declared read-only tables (ro_tables.json): the extent of each table a register-formed read walks without a guard the
# translator can bound (a loop pointer, a parameter-driven index). Each is baked whole, so the read is the image's bytes.
# An entry with a "bound" note is NOT baked: a runtime binding serves it (the note names it), and the read stays live.
RO_TABLES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ro_tables.json")
_RO_WINDOWS = []


def ro_windows():
    """[(lo, hi, bound note or None)] of the declared tables (RO_TABLES)."""
    if not _RO_WINDOWS:
        with open(RO_TABLES) as f:
            _RO_WINDOWS.extend((int(t["va"], 0), int(t["va"], 0) + int(t["bytes"], 0), t.get("bound"))
                               for t in json.load(f))
    return _RO_WINDOWS


def live_reads(found):
    """The lint's findings (LIVE_RO shape in), as sorted (function VA, site VA, image VA)."""
    return sorted((fn, site, va) for fn, reads in found.items() for site, va in reads)


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
        self.lines = []  # body being emitted (set by emit_function)
        self.labels = set()
        self.guards = []
        self.cur = None  # index of the instruction being emitted
        self.ro_tabs = {}  # (window VA, span) -> bytes of baked table windows
        self.sw_tabs = {}  # window VA -> span: the byte map / dword table of each resolved JMP-reg switch (prepass)
        self.live_ro = set()  # (site VA, read VA): live reads of a read-only image address no bake served (lint, LIVE_RO)

    def note_live_ro(self, mp):
        """The read emitted live below goes through a base or index register that may hold an image VA (image_origins)
        and lands in a read-only section: the GF runtime maps no image there, so it reads 0. Record it for
        gen_translated's lint, unless a runtime binding serves it (a `bound` table in RO_TABLES)."""
        if self.cur is None or mp.get("seg"):
            return
        for reg in (mp["base"], mp["index"]):
            for o in image_origins(self, PARENT.get(reg, reg), self.cur) if reg else ():
                v = o + mp["disp"]
                if not self.img.writable(v) and not any(lo <= v < hi and bound for lo, hi, bound in ro_windows()):
                    self.live_ro.add((self.lines[self.cur][0], v))

    def bake(self, va, n):
        if (va, n) not in self.consts:
            self.consts[(va, n)] = self.img.read(va, n)
        return va

    def addr_expr(self, mp):
        terms = []
        if mp.get("seg") == "gs":
            terms.append("s.gs_base")  # the thread block: a per-thread base the caller sets (PesState::gs_base)
        if mp["base"] is not None:
            terms.append("(%s)" % reg_read(mp["base"]))
        if mp["index"] is not None:
            terms.append("((%s)*%dULL)"
                         % (reg_read(mp["index"]), mp["scale"]))
        if mp["disp"]:
            d = mp["disp"]
            terms.append("(0x%XULL)" % (d & 0xFFFFFFFFFFFFFFFF) if d >= 0 else "(0ULL - 0x%XULL)" % (-d))
        return ("(%s)" % " + ".join(terms)) if terms else "0ULL"

    def bake_window(self, mp, width, flt):
        """A register-formed read of a read-only image table that no guard bounds, whose every origin lies in a declared
        table (RO_TABLES): bake the hull of the declared tables holding it (the harness corpus drives a parameter-indexed
        read over a wide domain, and a wider window only adds image bytes) and read it at the address's offset into it.
        None when the read is not image-derived, lies in no declared table, is served by a runtime binding, or is a
        128-bit read."""
        if self.cur is None or width == 128 or mp.get("seg"):
            return None
        starts = {o + mp["disp"] for reg in (mp["base"], mp["index"]) if reg
                  for o in image_origins(self, PARENT.get(reg, reg), self.cur)}
        if not starts or any(self.img.writable(v) for v in starts):
            return None
        held = [(lo, hi, bound) for lo, hi, bound in ro_windows() if all(lo <= v < hi for v in starts)]
        fit = [(lo, hi) for lo, hi, bound in held if not bound]
        if not fit or len(fit) < len(held):
            return None
        lo, span = min(l for l, _ in fit), max(h for _, h in fit) - min(l for l, _ in fit)
        n = 4 if (flt == "f32" or width == 32) else width // 8
        if flt == "f64":
            n = 8
        self.ro_tabs[(lo, span)] = self.img.read(lo, span)
        rd = "ro_rd(k_t%X_%X, %d, (uint64_t)(%s) - 0x%XULL, %d)" % (lo, span, span, self.addr_expr(mp), lo, n)
        if flt == "f32":
            return "PesState::f32((uint32_t)%s)" % rd
        if flt == "f64":
            return "PesState::f64(%s)" % rd
        return rd

    def bake_ro(self, mp, width, flt):
        """A register-indexed read of a read-only image table whose base is a
        LEA constant and whose index a guard bounds: bake the window the guard
        admits (build-time bytes, VA-cited). None when any part is unknown."""
        if self.cur is None or width == 128 or mp.get("seg"):  # a segment-relative read is never a baked table
            return None
        base = image_pointer(self, PARENT.get(mp["base"], mp["base"]), self.cur)
        count = bounded_index(self, PARENT.get(mp["index"], mp["index"]),
                              self.cur)
        if base is None:
            return None
        n = 4 if (flt == "f32" or width == 32) else width // 8
        if flt == "f64":
            n = 8
        start = base + mp["disp"]
        if count is not None:
            span = count * mp["scale"] + n
        elif start in self.sw_tabs:  # a switch's own table: its size is the prepass's, not a guard's
            span = self.sw_tabs[start]
        else:
            return None
        if not (BASE <= start and start + span <= BASE + self.img.size):
            return None
        if self.img.writable(start) or self.img.writable(start + span - 1):
            return None
        self.ro_tabs[(start, span)] = self.img.read(start, span)
        rd = "ro_rd(k_t%X_%X, %d, (uint64_t)(%s) * %dULL, %d)" % (
            start, span, span, reg_read(mp["index"]), mp["scale"], n)
        if flt == "f32":
            return "PesState::f32((uint32_t)%s)" % rd
        if flt == "f64":
            return "PesState::f64(%s)" % rd
        return rd

    def mem_val(self, memtxt, width, flt):
        """C expression reading memory. width in bits; flt in (None,'f32','f64')."""
        if "[" in memtxt:
            memtxt = memtxt[memtxt.find("[") + 1:memtxt.rfind("]")]
        mp = parse_mem(memtxt)
        if mp is None:
            return "/*BADMEM %s*/0ULL" % memtxt
        if mp["absolute"] is not None:
            va = mp["absolute"]
            if self.img.writable(va):
                # Writable section (.data etc.) or unknown: a live global
                # the harness maps (patch_addrs shadows, region linkage).
                # Never bake: read the same memory the harness provides.
                n = 4 if (flt == "f32" or width == 32) else width // 8
                if flt == "f64":
                    n = 8
                if width == 128:
                    return "s.rd128(0x%XULL)" % va
                if flt == "f32":
                    return "s.rdf(0x%XULL)" % va
                if flt == "f64":
                    return "s.rdd(0x%XULL)" % va
                return "s.rd(0x%XULL, %d)" % (va, n)
            if width == 128:
                self.bake(va, 16)
                return "(k_c%X)" % va
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
        if mp["base"] is not None and mp["index"] is None and self.cur is not None and not mp.get("seg"):
            # A base register holding an image VA makes the address a constant: read it as the absolute operand.
            c = image_pointer(self, PARENT.get(mp["base"], mp["base"]), self.cur)
            if c is not None and BASE <= c + mp["disp"] and c + mp["disp"] + width // 8 <= BASE + self.img.size \
                    and not self.img.writable(c + mp["disp"]):
                return self.mem_val("[0x%X]" % (c + mp["disp"]), width, flt)
        if mp["base"] is not None and mp["index"] is not None:
            baked = self.bake_ro(mp, width, flt)
            if baked is not None:
                return baked
        baked = self.bake_window(mp, width, flt)
        if baked is not None:
            return baked
        self.note_live_ro(mp)
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
        if m in ("MOV", "MOVABS"):
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
        if m in ("ROR", "ROL"):
            return self.do_rotate(m, ops)
        if m in ("XADD", "CMPXCHG"):
            return self.do_xadd(m, ops)
        if m in ("IMUL", "MUL"):
            return self.do_mul(m, ops)
        if m in ("BSF", "BSR"):
            return self.do_bitscan(m, ops)
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
        if o.lower().startswith("xmm") and "[" not in o.lower():
            # Bare xmm source in integer context (MOVD eax,xmm2): the
            # low 32 bits of lane 0.
            return "((uint64_t)(uint32_t)s.xmm[%d][0])" % int(o.lower()[3:])
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
            # MOVZX/MOVSX load as wide as the SOURCE operand (MOVZX eax, byte
            # ptr [..] reads one byte); MOVSXD reads a dword.
            sw = self.op_width(src) if ext else w
            if ext == "sx" and sw == 64:
                sw = 32
            v = self.mem_val(inner, sw, None)
            if ext == "sx":
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
                out.append("  s.of = (((_a ^ ~_b) & (_a ^ _r)) >> %d) & 1;"
                           % (w - 1))
            else:
                out.append("{ uint64_t _a = (uint64_t)(%s), _b = "
                           "(uint64_t)(%s), _c = %s, _r = "
                           "(_a - _b - _c) & %sULL;" % (a, b, c, mm))
                out.append("  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1);"
                           % (w - 1))
                out.append("  s.cf = ((_a & %sULL) < "
                           "(((_b + _c)) & %sULL));" % (mm, mm))
                out.append("  s.of = (((_a ^ _b) & (_a ^ _r)) >> %d) & 1;"
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
        # x86 masks a count to 5 bits (6 for 64-bit operands). A byte or word shifted by 8 to 31 is zeroed (SHL, SHR) or
        # sign-filled (SAR), so the count is never reduced to the operand width.
        if m == "SHL":
            r = "((_a << (_c & %d)) & %sULL)" % (0x1F if w < 64 else 0x3F,
                                                 mm)
        elif m == "SHR":
            r = "((_a & %sULL) >> (_c & %d))" % (mm,
                                                 0x1F if w < 64 else 0x3F)
        else:
            # SAR: sign-extend the w-bit value to 64 first (arithmetic
            # shift of the signed value), then shift right arithmetically.
            r = "((uint64_t)((((int64_t)((uint64_t)(_a) << %d)) >> %d) >> " \
                "(_c & %d)) & %sULL)" % (64 - w, 64 - w,
                                         0x1F if w < 64 else 0x3F, mm)
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

    def do_rotate(self, m, ops):
        dst = ops[0]
        w = self.op_width(dst)
        mm = {8: "0xFF", 16: "0xFFFF", 32: "0xFFFFFFFF",
              64: "0xFFFFFFFFFFFFFFFF"}[w]
        a = self.val_of(dst, w)
        c = self.val_of(ops[1], 8) if len(ops) > 1 else "1ULL"
        # Count masked to 5 bits (6 for 64-bit), then taken modulo the width.
        cmask = 63 if w == 64 else 31
        if m == "ROR":
            rot = "(((_a >> _k) | (_a << (%d - _k))) & %sULL)" % (w, mm)
        else:
            rot = "(((_a << _k) | (_a >> (%d - _k))) & %sULL)" % (w, mm)
        out = ["{ uint64_t _a = (uint64_t)(%s) & %sULL, _n = (uint64_t)(%s) & %dULL;"
               % (a, mm, c, cmask),
               "  uint64_t _k = _n %% %d;" % w,
               "  uint64_t _r = _k ? %s : _a;" % rot]
        msb = "((_r >> %d) & 1)" % (w - 1)
        if m == "ROR":
            out.append("  if (_n) { s.cf = %s != 0;" % msb)
            out.append("    if (_n == 1) s.of = (%s ^ ((_r >> %d) & 1)) != 0; }"
                       % (msb, w - 2))
        else:
            out.append("  if (_n) { s.cf = (_r & 1) != 0;")
            out.append("    if (_n == 1) s.of = (%s ^ (_r & 1)) != 0; }" % msb)
        if "[" in dst:
            out.append("  " + self.mem_store(dst[dst.find("["):], "_r", w, None))
        else:
            out.append("  " + reg_write(dst, "_r"))
        out.append("}")
        return out

    def do_xadd(self, m, ops):
        # LOCK is dropped by emit_function (single-threaded harness).
        dst, src = ops[0], ops[1]
        w = self.op_width(dst, src)
        mm = {8: "0xFF", 16: "0xFFFF", 32: "0xFFFFFFFF",
              64: "0xFFFFFFFFFFFFFFFF"}[w]
        d, sv = self.val_of(dst, w), self.val_of(src, w)
        msb = w - 1
        if m == "XADD":
            # tmp = dst + src; src = old dst; dst = tmp. Flags as ADD.
            out = ["{ uint64_t _a = (uint64_t)(%s) & %sULL, _b = (uint64_t)(%s) & %sULL;"
                   % (d, mm, sv, mm),
                   "  uint64_t _r = (_a + _b) & %sULL;" % mm,
                   "  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1) != 0;" % msb,
                   "  s.cf = _r < _a; s.of = ((((~(_a ^ _b)) & (_a ^ _r)) >> %d) & 1) != 0;" % msb,
                   "  " + reg_write(src, "_a")]
            if "[" in dst:
                out.append("  " + self.mem_store(dst[dst.find("["):], "_r", w, None))
            else:
                out.append("  " + reg_write(dst, "_r"))
            out.append("}")
            return out
        # CMPXCHG: flags as CMP(accumulator, dst); equal: dst = src, else accumulator = dst.
        acc = {8: "al", 16: "ax", 32: "eax", 64: "rax"}[w]
        out = ["{ uint64_t _a = (uint64_t)(%s) & %sULL, _d = (uint64_t)(%s) & %sULL, _s = (uint64_t)(%s) & %sULL;"
               % (self.val_of(acc, w), mm, d, mm, sv, mm),
               "  uint64_t _r = (_a - _d) & %sULL;" % mm,
               "  s.zf = (_r == 0); s.sf = ((_r >> %d) & 1) != 0;" % msb,
               "  s.cf = _a < _d; s.of = ((((_a ^ _d) & (_a ^ _r)) >> %d) & 1) != 0;" % msb,
               "  if (_r == 0) {"]
        if "[" in dst:
            out.append("    " + self.mem_store(dst[dst.find("["):], "_s", w, None))
        else:
            out.append("    " + reg_write(dst, "_s"))
        out.append("  } else { " + reg_write(acc, "_d") + " } }")
        return out

    def src128(self, op):
        """C u128 of an xmm or 128-bit memory source operand."""
        o = op.strip().lower()
        if o.startswith("xmm") and "[" not in o:
            y = int(o[3:])
            return ("((unsigned __int128)s.xmm[%d][0] | "
                    "((unsigned __int128)s.xmm[%d][1] << 64))" % (y, y))
        return "(%s)" % self.mem_val(op[op.find("["):], 128, None)

    def sd_src(self, op):
        """C u64 bits of a scalar double source: xmm lane 0 or a 64-bit memory operand."""
        o = op.strip().lower()
        if o.startswith("xmm") and "[" not in o:
            return "s.xmm[%d][0]" % int(o[3:])
        return "(uint64_t)(%s)" % self.mem_val(op[op.find("["):], 64, None)

    def do_packed(self, m, ops):
        """Packed 32-bit-lane SSE on the 128-bit value (PesState::lane_u32 / put_u32)."""
        dst = ops[0].strip().lower()
        if not (dst.startswith("xmm") and "[" not in dst):
            return ["/*UNHANDLED %s %s*/(void)0;" % (m, ops)]
        x = int(dst[3:])
        d = ("((unsigned __int128)s.xmm[%d][0] | "
             "((unsigned __int128)s.xmm[%d][1] << 64))" % (x, x))
        if m in ("PSLLD", "PSRLD"):
            n = imm_val(ops[1]) & 0xFF
            sh = "<<" if m == "PSLLD" else ">>"
            ex = [("(PesState::lane_u32(_d, %d) %s %d)" % (i, sh, n)) if n < 32 else "0u"
                  for i in range(4)]
            return ["{ unsigned __int128 _d = %s, _o = 0; %s "
                    "s.xmm[%d][0] = (uint64_t)_o; s.xmm[%d][1] = (uint64_t)(_o >> 64); }"
                    % (d, " ".join("_o = PesState::put_u32(_o, %d, %s);" % (i, e)
                                   for i, e in enumerate(ex)), x, x)]
        if m == "PSRLDQ":
            n = imm_val(ops[1]) & 0xFF
            o = "0" if n >= 16 else "(_d >> %d)" % (8 * n)
            return ["{ unsigned __int128 _d = %s, _o = %s; "
                    "s.xmm[%d][0] = (uint64_t)_o; s.xmm[%d][1] = (uint64_t)(_o >> 64); }"
                    % (d, o, x, x)]
        if m in ("ADDPS", "SUBPS", "MULPS", "DIVPS"):
            op = {"ADDPS": "+", "SUBPS": "-", "MULPS": "*", "DIVPS": "/"}[m]
            ex = ["PesState::bits(PesState::lane_f32(_d, %d) %s PesState::lane_f32(_s, %d))"
                  % (i, op, i) for i in range(4)]
        elif m in ("PADDD", "PSUBD"):
            op = "+" if m == "PADDD" else "-"
            ex = ["PesState::lane_u32(_d, %d) %s PesState::lane_u32(_s, %d)" % (i, op, i)
                  for i in range(4)]
        elif m == "PCMPEQD":
            ex = ["(PesState::lane_u32(_d, %d) == PesState::lane_u32(_s, %d) ? 0xFFFFFFFFu : 0u)"
                  % (i, i) for i in range(4)]
        elif m == "PSHUFD":
            imm = imm_val(ops[2]) & 0xFF
            ex = ["PesState::lane_u32(_s, %d)" % ((imm >> (2 * i)) & 3) for i in range(4)]
        elif m == "SHUFPS":
            imm = imm_val(ops[2]) & 0xFF
            pick = [("_d", imm & 3), ("_d", (imm >> 2) & 3),
                    ("_s", (imm >> 4) & 3), ("_s", (imm >> 6) & 3)]
            ex = ["PesState::lane_u32(%s, %d)" % p for p in pick]
        elif m in ("UNPCKLPS", "PUNPCKLDQ"):
            ex = ["PesState::lane_u32(%s, %d)" % p
                  for p in [("_d", 0), ("_s", 0), ("_d", 1), ("_s", 1)]]
        elif m == "CVTTPS2DQ":
            ex = ["(uint32_t)PesState::cvtt_i32((double)PesState::lane_f32(_s, %d))" % i
                  for i in range(4)]
        elif m in ("MINPS", "MAXPS"):
            op = "<" if m == "MINPS" else ">"
            ex = ["PesState::bits(PesState::lane_f32(_d, %d) %s PesState::lane_f32(_s, %d) ? "
                  "PesState::lane_f32(_d, %d) : PesState::lane_f32(_s, %d))" % (i, op, i, i, i)
                  for i in range(4)]
        elif m == "HADDPS":
            ex = ["PesState::bits(PesState::lane_f32(_d, 0) + PesState::lane_f32(_d, 1))",
                  "PesState::bits(PesState::lane_f32(_d, 2) + PesState::lane_f32(_d, 3))",
                  "PesState::bits(PesState::lane_f32(_s, 0) + PesState::lane_f32(_s, 1))",
                  "PesState::bits(PesState::lane_f32(_s, 2) + PesState::lane_f32(_s, 3))"]
        elif m in ("SQRTPS", "RCPPS", "RSQRTPS"):
            # Unary, per lane of the source: IEEE single precision (the diff harness checks the bits).
            f = {"SQRTPS": "__builtin_sqrtf(%s)", "RCPPS": "1.0f / %s",
                 "RSQRTPS": "1.0f / __builtin_sqrtf(%s)"}[m]
            ex = ["PesState::bits(%s)" % (f % ("PesState::lane_f32(_s, %d)" % i)) for i in range(4)]
        elif m in ("CMPEQPS", "CMPLTPS", "CMPLEPS", "CMPUNORDPS", "CMPNEQPS", "CMPNLTPS", "CMPNLEPS", "CMPORDPS"):
            # CMPccPS: a lane becomes all ones when the predicate holds on its two floats; NaN follows the SDM table.
            pred = {"CMPEQPS": "{a} == {b}", "CMPLTPS": "{a} < {b}", "CMPLEPS": "{a} <= {b}",
                    "CMPUNORDPS": "__builtin_isnan({a}) || __builtin_isnan({b})",
                    "CMPNEQPS": "!({a} == {b})", "CMPNLTPS": "!({a} < {b})", "CMPNLEPS": "!({a} <= {b})",
                    "CMPORDPS": "!__builtin_isnan({a}) && !__builtin_isnan({b})"}[m]
            ex = ["((%s) ? 0xFFFFFFFFu : 0u)" % pred.format(a="PesState::lane_f32(_d, %d)" % i,
                                                          b="PesState::lane_f32(_s, %d)" % i)
                  for i in range(4)]
        else:  # UNPCKHPS
            ex = ["PesState::lane_u32(%s, %d)" % p
                  for p in [("_d", 2), ("_s", 2), ("_d", 3), ("_s", 3)]]
        body = " ".join("_o = PesState::put_u32(_o, %d, %s);" % (i, e)
                        for i, e in enumerate(ex))
        return ["{ unsigned __int128 _d = %s, _s = %s, _o = 0; %s "
                "s.xmm[%d][0] = (uint64_t)_o; s.xmm[%d][1] = (uint64_t)(_o >> 64); }"
                % (d, self.src128(ops[1]), body, x, x)]

    def do_div(self, m, ops):
        """DIV / IDIV of 8, 16, 32 or 64 bits. The dividend is AX, DX:AX, EDX:EAX or RDX:RAX; the quotient and the
        remainder go to AL and AH, AX and DX, EAX and EDX, RAX and RDX (32-bit results zero-extend). Flags are
        undefined. A zero divisor or a quotient that does not fit traps (#DE): the harness never reaches one."""
        o = ops[0]
        w = self.op_width(o)
        div = self.val_of(o, w)
        mask = {8: "0xFFULL", 16: "0xFFFFULL", 32: "0xFFFFFFFFULL",
                64: "0xFFFFFFFFFFFFFFFFULL"}[w]
        if m == "DIV":
            num = {8: "(unsigned __int128)(s.gpr[0] & 0xFFFFULL)",
                   16: "(((unsigned __int128)(s.gpr[2] & 0xFFFFULL) << 16) | (s.gpr[0] & 0xFFFFULL))",
                   32: "(((unsigned __int128)(s.gpr[2] & 0xFFFFFFFFULL) << 32) | (s.gpr[0] & 0xFFFFFFFFULL))",
                   64: "(((unsigned __int128)s.gpr[2] << 64) | s.gpr[0])"}[w]
            head = "unsigned __int128 _n = %s, _d = (uint64_t)(%s) & %s;" % (num, div, mask)
            body = ("if (_d == 0) __builtin_trap(); "
                    "unsigned __int128 _q = _n / _d, _r = _n %% _d; "
                    "if ((_q >> %d) != 0) __builtin_trap();" % w)
        else:
            num = {8: "(__int128)(int16_t)(uint16_t)(s.gpr[0] & 0xFFFFULL)",
                   16: "(__int128)(int32_t)(uint32_t)(((uint32_t)(s.gpr[2] & 0xFFFFULL) << 16) | "
                       "(uint32_t)(s.gpr[0] & 0xFFFFULL))",
                   32: "(__int128)(int64_t)(((uint64_t)(uint32_t)s.gpr[2] << 32) | (uint32_t)s.gpr[0])",
                   64: "(__int128)(((unsigned __int128)s.gpr[2] << 64) | s.gpr[0])"}[w]
            ity = {8: "int8_t", 16: "int16_t", 32: "int32_t", 64: "int64_t"}[w]
            head = "__int128 _n = %s, _d = (__int128)(%s)((uint64_t)(%s));" % (num, ity, div)
            body = ("if (_d == 0) __builtin_trap(); "
                    "if (_d == -1 && _n == (__int128)((unsigned __int128)1 << 127)) __builtin_trap(); "
                    "__int128 _q = _n / _d, _r = _n %% _d; "
                    "if (_q < -((__int128)1 << %d) || _q >= ((__int128)1 << %d)) __builtin_trap();"
                    % (w - 1, w - 1))
        write = {8: "s.gpr[0] = (s.gpr[0] & ~0xFFFFULL) | ((uint64_t)_q & 0xFFULL) | (((uint64_t)_r & 0xFFULL) << 8);",
                 16: "s.gpr[0] = (s.gpr[0] & ~0xFFFFULL) | ((uint64_t)_q & 0xFFFFULL); "
                     "s.gpr[2] = (s.gpr[2] & ~0xFFFFULL) | ((uint64_t)_r & 0xFFFFULL);",
                 32: "s.gpr[0] = (uint64_t)(uint32_t)_q; s.gpr[2] = (uint64_t)(uint32_t)_r;",
                 64: "s.gpr[0] = (uint64_t)_q; s.gpr[2] = (uint64_t)_r;"}[w]
        return ["{ %s %s %s }" % (head, body, write)]

    def do_string(self, m, rep):
        """STOS and MOVS, REP-prefixed or single. The direction flag is clear (the ABI): RDI and RSI step up by the
        element size. A REP count runs in RCX and leaves it at zero. Flags are untouched."""
        n = {"B": 1, "W": 2, "D": 4, "Q": 8}[m[-1]]
        if m.startswith("STOS"):
            val = {1: "0xFFULL", 2: "0xFFFFULL", 4: "0xFFFFFFFFULL", 8: "0xFFFFFFFFFFFFFFFFULL"}[n]
            step = "s.wr(s.gpr[7], s.gpr[0] & %s, %d); s.gpr[7] += %d;" % (val, n, n)
        else:
            step = "s.wr(s.gpr[7], s.rd(s.gpr[6], %d), %d); s.gpr[6] += %d; s.gpr[7] += %d;" % (n, n, n, n)
        if rep:
            return ["{ uint64_t _c = s.gpr[1]; while (_c != 0) { %s --_c; } s.gpr[1] = 0; }" % step]
        return ["{ %s }" % step]

    def do_qword(self, m, ops):
        """Qword lanes of an xmm pair (MOVHLPS, MOVLHPS, PUNPCKLQDQ, PUNPCKHQDQ, PADDQ); lane 0 is the low qword."""
        d, s = ops[0].strip().lower(), ops[1].strip().lower()
        if not (d.startswith("xmm") and "[" not in d and s.startswith("xmm") and "[" not in s):
            return ["/*UNHANDLED %s %s*/(void)0;" % (m, ops)]
        x, y = int(d[3:]), int(s[3:])
        if m == "MOVHLPS":
            return ["s.xmm[%d][0] = s.xmm[%d][1];" % (x, y)]
        if m in ("MOVLHPS", "PUNPCKLQDQ"):
            return ["s.xmm[%d][1] = s.xmm[%d][0];" % (x, y)]
        if m == "PUNPCKHQDQ":
            return ["{ uint64_t _dh = s.xmm[%d][1], _sh = s.xmm[%d][1]; s.xmm[%d][0] = _dh; s.xmm[%d][1] = _sh; }"
                    % (x, y, x, x)]
        return ["{ uint64_t _s0 = s.xmm[%d][0], _s1 = s.xmm[%d][1]; s.xmm[%d][0] += _s0; s.xmm[%d][1] += _s1; }"
                % (y, y, x, x)]

    def do_movq_gpr(self, ops):
        """MOVQ between a 64-bit GPR and an xmm register; a GPR loaded into xmm zeroes the upper qword."""
        d, s = ops[0].strip().lower(), ops[1].strip().lower()
        if d.startswith("xmm"):
            x = int(d[3:])
            return ["s.xmm[%d][0] = %s; s.xmm[%d][1] = 0;" % (x, reg_read(s), x)]
        return [reg_write(d, "s.xmm[%d][0]" % int(s[3:]))]

    def do_cmpss(self, ops):
        """CMPNLTSS (CMPSS predicate 5): lane 0 becomes all ones when dst is not less than src, NaN included."""
        x = int(ops[0].strip().lower()[3:])
        return ["{ float _a = %s, _b = %s; uint32_t _m = !(_a < _b) ? 0xFFFFFFFFu : 0u; "
                "s.xmm[%d][0] = (s.xmm[%d][0] & 0xFFFFFFFF00000000ULL) | _m; }"
                % (self.x_lane(ops[0]), self.x_lane(ops[1]), x, x)]

    def do_sd_arith(self, m, ops):
        dst = ops[0].strip().lower()
        if not (dst.startswith("xmm") and "[" not in dst):
            return ["/*UNHANDLED %s %s*/(void)0;" % (m, ops)]
        x = int(dst[3:])
        op = {"ADDSD": "+", "SUBSD": "-", "MULSD": "*", "DIVSD": "/"}[m]
        return ["s.set_x_f64(%d, s.x_f64(%d) %s PesState::f64(%s));"
                % (x, x, op, self.sd_src(ops[1]))]

    def do_comisd(self, ops):
        x = int(ops[0].strip().lower()[3:])
        return ["s.cmp_sd(s.x_f64(%d), PesState::f64(%s));" % (x, self.sd_src(ops[1]))]

    def do_cvt_sd(self, m, ops):
        dst, src = ops[0], ops[1]
        if m == "CVTTSD2SI":
            fn = "PesState::cvtt_i32" if self.op_width(dst) == 32 else "PesState::cvtt_i64"
            return [reg_write(dst, "(uint64_t)%s(PesState::f64(%s))" % (fn, self.sd_src(src)))]
        # CVTSI2SD: lane 0 becomes the double, upper lane kept.
        sw = self.op_width(src)
        v = self.val_of(src, sw, sx=True)
        if "[" in src and sw < 64:
            v = sext(v, sw)
        return ["s.set_x_f64(%d, (double)(int64_t)(%s));" % (int(dst.strip().lower()[3:]), v)]
    def do_bitscan(self, m, ops):
        """BSF / BSR r, r/m (16, 32 or 64 bits). ZF = (source == 0). A zero source leaves the destination
        unchanged (SDM: undefined; the hardware and the harness both keep it). Otherwise the destination
        gets the index of the lowest (BSF) or highest (BSR) set bit."""
        dst, src = ops[0], ops[1]
        w = self.op_width(dst)
        mask = {16: "0xFFFFULL", 32: "0xFFFFFFFFULL", 64: "0xFFFFFFFFFFFFFFFFULL"}[w]
        idx = "__builtin_ctzll(_s)" if m == "BSF" else "(63 - __builtin_clzll(_s))"
        return ["{ uint64_t _s = (uint64_t)(%s) & %s; s.zf = (_s == 0);" % (self.val_of(src, w), mask),
                "  if (_s) { %s } }" % reg_write(dst, "(uint64_t)(%s)" % idx)]

    def do_mul(self, m, ops):
        if m == "MUL" and len(ops) == 1:
            # unsigned: AL*r8->AX, AX*r16->DX:AX, EAX*r32->EDX:EAX, RAX*r64->RDX:RAX. A 32-bit
            # destination zero-extends RAX and RDX (SDM); the 8- and 16-bit forms keep the other bits.
            o = ops[0]
            w = self.op_width(o)
            b = self.val_of(o, w)
            if w == 8:
                return ["{ uint64_t _p = (uint64_t)(uint8_t)s.gpr[0] * (uint64_t)(uint8_t)(%s);" % b,
                        "  s.gpr[0] = (s.gpr[0] & 0xFFFFFFFFFFFF0000ULL) | (_p & 0xFFFFULL);",
                        "  s.cf = s.of = ((_p >> 8) != 0); }"]
            if w == 16:
                return ["{ uint64_t _p = (uint64_t)(uint16_t)s.gpr[0] * (uint64_t)(uint16_t)(%s);" % b,
                        "  s.gpr[0] = (s.gpr[0] & 0xFFFFFFFFFFFF0000ULL) | (_p & 0xFFFFULL);",
                        "  s.gpr[2] = (s.gpr[2] & 0xFFFFFFFFFFFF0000ULL) | ((_p >> 16) & 0xFFFFULL);",
                        "  s.cf = s.of = ((_p >> 16) != 0); }"]
            if w == 32:
                return ["{ uint64_t _p = (uint64_t)(uint32_t)s.gpr[0] * (uint64_t)(uint32_t)(%s);" % b,
                        "  s.gpr[0] = (uint64_t)(uint32_t)_p;",
                        "  s.gpr[2] = (uint64_t)(uint32_t)(_p >> 32);",
                        "  s.cf = s.of = ((_p >> 32) != 0); }"]
            return ["{ unsigned __int128 _p = (unsigned __int128)s.gpr[0] * "
                    "(uint64_t)(%s);" % b,
                    "  s.gpr[0] = (uint64_t)_p; s.gpr[2] = "
                    "(uint64_t)(_p >> 64);",
                    "  s.cf = s.of = ((_p >> 64) != 0); }"]
        if m == "IMUL" and len(ops) == 1:
            # signed: the same widths and writes; CF = OF = the product does not fit the low part (SDM).
            o = ops[0]
            w = self.op_width(o)
            b = self.val_of(o, w)
            if w == 8:
                return ["{ int16_t _p = (int16_t)((int8_t)s.gpr[0]) * (int16_t)((int8_t)(%s));" % b,
                        "  s.gpr[0] = (s.gpr[0] & 0xFFFFFFFFFFFF0000ULL) | ((uint16_t)_p);",
                        "  s.cf = s.of = ((int16_t)(int8_t)_p != _p); }"]
            if w == 16:
                return ["{ int32_t _p = (int32_t)((int16_t)s.gpr[0]) * (int32_t)((int16_t)(%s));" % b,
                        "  s.gpr[0] = (s.gpr[0] & 0xFFFFFFFFFFFF0000ULL) | ((uint16_t)_p);",
                        "  s.gpr[2] = (s.gpr[2] & 0xFFFFFFFFFFFF0000ULL) | ((uint16_t)((uint32_t)_p >> 16));",
                        "  s.cf = s.of = ((int32_t)(int16_t)_p != _p); }"]
            if w == 32:
                return ["{ int64_t _p = (int64_t)((int32_t)s.gpr[0]) * (int64_t)((int32_t)(%s));" % b,
                        "  s.gpr[0] = (uint64_t)(uint32_t)_p;",
                        "  s.gpr[2] = (uint64_t)(uint32_t)((uint64_t)_p >> 32);",
                        "  s.cf = s.of = ((int64_t)(int32_t)_p != _p); }"]
            return ["{ __int128 _p = (__int128)(int64_t)s.gpr[0] * (int64_t)(%s);" % b,
                    "  s.gpr[0] = (uint64_t)_p; s.gpr[2] = (uint64_t)(_p >> 64);",
                    "  s.cf = s.of = ((int64_t)(uint64_t)_p != _p); }"]
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
                "; s.gpr[4] += 8;"]

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
        if m == "MOVQ" and (ops[0].strip().lower() in GPR or ops[1].strip().lower() in GPR):
            return self.do_movq_gpr(ops)
        if m in ("MOVAPS", "MOVUPS", "MOVAPD", "MOVUPD", "MOVDQA", "MOVDQ", "MOVDQU",
                 "MOVQ"):
            return self.do_mov128v(ops)
        if m in ("ANDPS", "ORPS", "XORPS", "ANDPD", "ORPD", "XORPD",
                 "PXOR", "PAND", "POR", "PANDN",
                 "ANDNPS", "ANDNPD"):
            return self.do_bit128(m, ops)
        if m in ("CVTPS2PD", "CVTPD2PS", "CVTDQ2PS", "CVTPS2DQ",
                 "CVTPD2DQ", "CVTDQ2PD"):
            return self.do_cvtv(m, ops)
        if m in ("ADDPS", "SUBPS", "MULPS", "DIVPS", "PADDD", "PSUBD", "PCMPEQD",
                 "PSHUFD", "PSRLDQ", "SHUFPS", "UNPCKLPS", "UNPCKHPS", "PSLLD", "PSRLD",
                 "PUNPCKLDQ", "PUNPCKHDQ", "CVTTPS2DQ", "MINPS", "MAXPS", "HADDPS",
                 "SQRTPS", "RCPPS", "RSQRTPS", "CMPEQPS", "CMPLTPS", "CMPLEPS", "CMPUNORDPS",
                 "CMPNEQPS", "CMPNLTPS", "CMPNLEPS", "CMPORDPS"):
            return self.do_packed(m, ops)
        if m in ("ADDSD", "SUBSD", "MULSD", "DIVSD"):
            return self.do_sd_arith(m, ops)
        if m in ("COMISD", "UCOMISD"):
            return self.do_comisd(ops)
        if m in ("CVTTSD2SI", "CVTSI2SD"):
            return self.do_cvt_sd(m, ops)
        if m in ("MOVHLPS", "MOVLHPS", "PUNPCKLQDQ", "PUNPCKHQDQ", "PADDQ"):
            return self.do_qword(m, ops)
        if m == "CMPNLTSS":
            return self.do_cmpss(ops)
        return None

    def x_lane(self, op):
        o = op.strip().lower()
        if o in GPR or o in SUB:
            # Integer register as float source (MOVD/MOVSS reg): the low
            # 32 bits are IEEE-754 bits, not an integer value.
            return "(PesState::f32((uint32_t)(%s)))" % reg_read(o)
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
                # MOVD xmm, r/m32 zero-extends to 128 bits (SDM): bits 32-127 are cleared.
                if s in GPR or s in SUB:
                    return ["s.xmm[%d][0] = (uint64_t)(uint32_t)(%s); s.xmm[%d][1] = 0;"
                            % (x, reg_read(s), x)]
                inner = src[src.find("["):]
                return ["s.xmm[%d][0] = (uint64_t)(uint32_t)(%s); s.xmm[%d][1] = 0;"
                        % (x, self.mem_val(inner, 32, None), x)]
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
                # A memory source zeroes bits 64-127 (SDM: MOVSD xmm, m64); a register source keeps them.
                return ["s.xmm[%d][0] = %s; s.xmm[%d][1] = 0;"
                        % (x, self.mem_val(inner, 64, None), x)]
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
            # A memory source zeroes bits 32-127 (SDM: MOVSS xmm, m32); a register source keeps them.
            return ["s.set_x_f32(%d, %s); s.xmm[%d][0] &= 0xFFFFFFFFull; s.xmm[%d][1] = 0;"
                    % (x, self.mem_val(inner, 32, "f32"), x, x)]
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
        # Intel MAXSS/MINSS: the destination is kept only when dst > src (MAXSS) or dst < src (MINSS). Equal values
        # (-0.0 and +0.0 compare equal) and NaN take the SOURCE. 1408A40F0: maxss of -0.0 with +0.0 returned -0.0.
        if m == "MAXSS":
            return ["s.set_x_f32(%d, (float)(((%s) > (%s)) ? (%s) : (%s)));" % (x, a, b, a, b)]
        return ["s.set_x_f32(%d, (float)(((%s) < (%s)) ? (%s) : (%s)));" % (x, a, b, a, b)]

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
            if m in ("XORPS", "XORPD", "PXOR"):
                return ["s.xmm[%d][0] = 0; s.xmm[%d][1] = 0;" % (x, x)]
            if m in ("ORPS", "ORPD", "ANDPS", "ANDPD", "POR", "PAND"):
                return ["(void)0;  // %s self-reg identity" % m]
        if s.startswith("xmm") and "[" not in s:
            y = int(s[3:])
            e = "(s.xmm[%d][0] | ((unsigned __int128)s.xmm[%d][1] << 64))" \
                % (y, y)
        else:
            inner = ops[1][ops[1].find("["):]
            e = "(%s)" % self.mem_val(inner, 128, None)
        op = {"ANDPS": "&", "ANDPD": "&", "PAND": "&", "ORPS": "|", "ORPD": "|", "POR": "|",
              "XORPS": "^", "XORPD": "^", "PXOR": "^", "ANDNPS": "&", "ANDNPD": "&", "PANDN": "&"}[m]
        if m in ("ANDNPS", "ANDNPD", "PANDN"):
            e = "(~((unsigned __int128)s.xmm[%d][0] | " \
                "((unsigned __int128)s.xmm[%d][1] << 64)) & %s)" % (x, x,
                                                                     e)
            op = "&"
            cur = "(~(unsigned __int128)0)"
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
                lan0 = "s.xmm[%d][0]" % y
            else:
                inner = src[src.find("["):]
                lan0 = "(%s)" % self.mem_val(inner, 64, None)
            lo = "(double)PesState::f32((uint32_t)%s)" % lan0
            hi = "(double)PesState::f32((uint32_t)(%s >> 32))" % lan0
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
            if not is_imm(ops[0].strip()):
                # A call through a named import slot (import_slots.json) runs the slot's model
                # (import_stubs.json), as the snapshot harness runs it: the export is modelled, not translated.
                model = import_model_lines(ops[0], jump=False)
                if model is not None:
                    return model
                # Indirect call (vtable / register / memory): emit the exact
                # address computation the bytes compute, then look the target
                # up in the generated dispatch table (pes_dispatch.hpp). A
                # target missing from the table fails loudly
                # (PES_DISPATCH_UNTRANSLATED), never silently.
                if "[" in ops[0]:
                    tgt = self.mem_val(ops[0][ops[0].find("["):], 64, None)
                else:
                    tgt = reg_read(ops[0].strip())
                return ["{ uint64_t _t = (uint64_t)(%s); PesDispatch _d = pes_dispatch_lookup(_t);"
                        % tgt,
                        "  if (_d.fn == 0) pes_dispatch_missing(_t);",
                        "  s.gpr[4] -= 8; s.wr(s.gpr[4], 0ULL, 8); _d.fn(s);",
                        "  s.gpr[4] += 8; }"]
            tgt = "0x%X" % imm_val(ops[0])
            stub = (ctx.get("stubs") or {}).get(tgt)
            if stub is not None and stub.get("do") != "unmodellable":
                # Stubbed callee: its modelled effect on the caller (stubs.py),
                # then the call returns, as the harness pops the return.
                return stub_lines(stub["do"])
            if tgt in self.pilot:
                self.siblings.add(tgt)
                fn = "pes_fn_%s" % tgt[2:]
                return ["{ s.gpr[4] -= 8; s.wr(s.gpr[4], 0ULL, 8); %s(s); "
                        "s.gpr[4] += 8; }" % fn]
            return ["/*OUT-OF-SET CALL %s*/ pes_dispatch_missing(0x%X);" % (tgt, imm_val(ops[0]))]
        if m == "JMP":
            o = ops[0].strip()
            if absolute_slot(o) is not None:
                # JMP through an absolute slot: a named export runs its model, then the function returns. Any other
                # slot (an image function's address, as the snapshot holds it) is a tail jump to the target the slot
                # holds now, looked up in the dispatch table like an indirect CALL; an untranslated target aborts.
                model = import_model_lines(o, jump=True)
                if model is not None:
                    return model + ["return;"]
                tgt_expr = self.mem_val(o[o.find("["):], 64, None)
                return ["{ uint64_t _t = (uint64_t)(%s); PesDispatch _d = pes_dispatch_lookup(_t);" % tgt_expr,
                        "  if (_d.fn == 0) pes_dispatch_missing(_t);",
                        "  _d.fn(s); }",
                        "return;"]
            if is_imm(o):
                # In-range target: local goto. Out-of-range target: it is a
                # tail call to a sibling (the pilot set is call-closed);
                # emit a direct call + return, never an undefined goto.
                tgt = "0x%X" % imm_val(o)
                addrs = ctx.get("addrs", set())
                if not addrs or imm_val(o) in addrs:
                    return ["goto L_%X;" % imm_val(o)]
                stub = (ctx.get("stubs") or {}).get(tgt)
                if stub is not None and stub.get("do") != "unmodellable":
                    return stub_lines(stub["do"]) + ["return;"]
                if tgt in self.pilot:
                    self.siblings.add(tgt)
                    return ["{ pes_fn_%s(s); return; }" % tgt[2:]]
                # Never a silent return: the caller would see a normal return where PES jumped into code the layer
                # does not hold. The closure walk (driver.runnable_extra) pulls such targets in; one that remains
                # aborts naming the VA when reached, and gen_translated.silent_tail_sites refuses it at generation.
                # (The `return;` stays a separate line: it is the basic-block terminator flow.py reads.)
                return ["/*OUT-OF-RANGE JMP %s*/ pes_dispatch_missing(%s);" % (tgt, tgt), "return;"]
            # JMP reg: switch dispatch (resolved in prepass) or tailjump
            info = ctx.get("jmpreg", {}).get(addr)
            if info and info.get("table") is not None:
                tva = info["table"]
                words = info["words"]
                self.tables[tva] = {"words": words,
                                    "base": info["base"]}
                # Dispatch on the TABLE ENTRY, not the index: the table load overwrites the index register, so at
                # JMP time the register holds base + table[idx]. Its entry is that register minus base; each distinct
                # entry that is a local label becomes a case. Any other entry falls to default.
                lines = ["{ uint32_t _i = (uint32_t)(%s - 0x%XULL); switch (_i) {"
                         % (reg_read(ops[0]), info["base"])]
                for w in sorted(set(words)):
                    tgt = info["base"] + w
                    if tgt in self.labels:
                        lines.append("  case %uU: goto L_%X;" % (w, tgt))
                lines.append("  default: goto L_%X;" % info["default"])
                lines.append("  } }")
                return lines
            tgt = info.get("tail") if info else None
            if tgt:
                fn = "pes_fn_%s" % tgt[2:]
                return ["{ %s(s); return; }" % fn]
            if "[" in o:
                # JMP through memory (a vtable tail call: `pop rbx ; jmp qword ptr [rax + 0x98]` in 0x1408550A0): a tail
                # jump to the target the memory holds now, looked up in the dispatch table like an indirect CALL; an
                # untranslated target aborts. The return address stays the caller's.
                tgt_expr = self.mem_val(o[o.find("["):], 64, None)
                return ["{ uint64_t _t = (uint64_t)(%s); PesDispatch _d = pes_dispatch_lookup(_t);" % tgt_expr,
                        "  if (_d.fn == 0) pes_dispatch_missing(_t);",
                        "  _d.fn(s); }",
                        "return;"]
            # Loud, like an untaken out-of-body Jcc: a jump the prepass could not resolve trapped when reached, never a
            # silent return (the silent return hid a switch dispatch in 141F5F330 until lockstep found it).
            return ["__builtin_trap(); /* UNRESOLVED JMP %s */" % o]
        if m == "LOOP":
            # LOOP rel8 (64-bit mode): RCX counts down, and the jump is taken while RCX != 0; flags are untouched.
            tgt = imm_val(ops[0])
            if ctx.get("addrs") and tgt not in ctx["addrs"]:
                return ["__builtin_trap(); /* LOOP target is not an instruction of this body */"]
            return ["s.gpr[1] -= 1; if (s.gpr[1] != 0) goto L_%X;" % tgt]
        cc = cc_of(m)
        if cc:
            tgt = imm_val(ops[0])
            if ctx.get("addrs") and tgt not in ctx["addrs"]:
                # No label exists for a target outside the decoded body: trap when taken, never an undefined goto.
                return ["if (s.cond(\"%s\")) __builtin_trap();" % cc]
            return ["if (s.cond(\"%s\")) goto L_%X;" % (cc, tgt)]
        return None

    def emit_set_cmov(self, mnem, ops):
        m = mnem.upper()
        if m.startswith("SET"):
            cc = cc_of(m)
            d = ops[0].strip().lower()
            e = "(s.cond(\"%s\") ? 1ULL : 0ULL)" % cc
            if d in GPR or d in SUB:
                return [reg_write(d, e)]
            return [self.mem_store(d[d.find("["):], e, 8, None)]
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
    # A segment prefix sits outside the brackets in the disassembly ("gs:[0x58]"); callers take the operand from its
    # "[" on, so the prefix moves inside: "[gs:0x58]". parse_mem reads it there.
    return [p.replace("gs:[", "[gs:") for p in parts]


def parse_lines(text):
    out = []
    for l in text.splitlines():
        l = l.strip()
        m = re.match(r"^([0-9a-f]+):\s*([A-Z0-9]+)\s*(.*)$", l)
        if m:
            out.append((int(m.group(1), 16), m.group(2),
                        split_ops(m.group(3))))
    return out


_PDATA = {}


def pdata_of(image_path):
    """The image's Pdata (cached): the one source of function extents (pdata.py)."""
    from pdata import Pdata
    if image_path not in _PDATA:
        _PDATA[image_path] = Pdata(open(image_path, "rb"))  # kept open: decode reads the image
    return _PDATA[image_path]


def capstone_decode(image_path, va):
    """The instructions of the function at va: its .pdata range plus the chunks chained to it, or a leaf's bytes to
    the next .pdata start (pdata.Pdata.decode). Empty if va is not a function boundary."""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = False
    return [(i.address, i.mnemonic.upper(), split_ops(i.op_str))
            for i in pdata_of(image_path).decode(md, va) or []]


def resolve_rip(lines, image_path, va):
    """Rewrite [rip + disp] operands to absolute [0x..] VAs.

    The bridge prints absolute VAs but capstone prints RIP-relative form.
    Resolved with capstone's own instruction sizes: target = next_addr +
    disp. Only touches operands mentioning rip; bridge text is unchanged.
    """
    if not any("rip" in (op.lower()) for _, _, ops in lines for op in ops):
        return lines
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    from pdata import MAX_RANGE
    img = Image(image_path)
    p = pdata_of(image_path)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    sizes = {}
    # Every rip-relative insn needs its size, including ones past an early RET that capstone_decode keeps: a linear
    # pass over each range of the function's extent (never past it), then the decode's own sizes win.
    for b, e in p.ranges_of(va) or [(va, va + MAX_RANGE)]:
        for ins in md.disasm(bytes(img.read(b, min(e - b, MAX_RANGE))), b):
            sizes[ins.address] = ins.size
    for ins in p.decode(md, va) or []:
        sizes[ins.address] = ins.size
    fixed = []
    for addr, mnem, ops in lines:
        size = sizes.get(addr, 0)
        new_ops = []
        for op in ops:
            lo = op.lower()
            if "rip" in lo:
                m = re.search(r"rip\s*([+-])\s*(0x[0-9a-f]+|\d+)", lo)
                if m and size:
                    sign = -1 if m.group(1) == "-" else 1
                    mag = m.group(2)
                    disp = sign * (int(mag, 16) if "0x" in mag
                                   else int(mag))
                    tgt = addr + size + disp
                    op = re.sub(r"\[rip\s*[+-]\s*[^]]+\]",
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
        elif m == "LOOP" and ops and is_imm(ops[0]):
            t = imm_val(ops[0])
            if t in addrs:
                labels.add(t)
    for addr, mnem, ops in lines:
        if mnem.upper() != "JMP" or not ops or is_imm(ops[0].strip()):
            continue
        r = PARENT.get(ops[0].strip().lower(),
                       ops[0].strip().lower())
        i = pos[addr]
        base = None
        tdisp = None
        count = None
        default = None
        same = lambda o: PARENT.get(o.strip().lower(),
                                   o.strip().lower()) == r
        for j in range(i - 1, max(-1, i - 12), -1):
            mn2, ops2 = byaddr[order[j]]
            if mn2.upper() == "ADD" and len(ops2) == 2 and \
                    same(ops2[0]):
                base = ops2[1].strip().lower()
                break
        for j in range(i - 1, max(-1, i - 12), -1):
            mn2, ops2 = byaddr[order[j]]
            if mn2.upper() == "MOV" and len(ops2) == 2 and \
                    same(ops2[0]) and "[" in ops2[1]:
                inner = ops2[1][ops2[1].find("[") + 1:ops2[1].rfind("]")]
                mp = parse_mem(inner)
                if mp and mp["scale"] == 4 and mp["index"] is not None:
                    tdisp = mp["disp"]
                    break
        baseva = None
        basep = PARENT.get(base, base) if base is not None else None
        # The base is the nearest write to its register, however far back: a switch's `lea base,[image]` can sit past
        # any fixed window. 141F5F330 -> 141F5FC00: the LEA is 21 instructions above its JMP, the old window was 20, so
        # the JMP went UNRESOLVED and the translation returned where PES dispatched to a case.
        for j in range(i - 1, -1, -1):
            mn2, ops2 = byaddr[order[j]]
            if basep is None or mn2.upper() in ("CMP", "TEST") or not ops2 or \
                    PARENT.get(ops2[0].strip().lower(), ops2[0].strip().lower()) != basep:
                continue
            if mn2.upper() == "LEA" and len(ops2) == 2 and "[" in ops2[1]:
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
            # Two-level dispatch (only 140A40050 in the set): a byte map
            # [base + idx] feeds the dword table index (MOVZX/MOVSX with a
            # scale-1 index). When present, the dword table has
            # max(bytemap)+1 entries, not the CMP guard count (the guard
            # bounds the byte map; over-reading the dword table pulls in
            # code bytes that occasionally land in-range by accident).
            bmap = None
            for j in range(i - 1, max(-1, i - 16), -1):
                mn2, ops2 = byaddr[order[j]]
                if mn2.upper() in ("MOVZX", "MOVSX", "MOVSXD") and \
                        len(ops2) == 2 and "[" in ops2[1]:
                    inner = ops2[1][ops2[1].find("[") + 1:
                                    ops2[1].rfind("]")]
                    mp = parse_mem(inner)
                    # The byte map is [table base + idx]: its base register must be the one the LEA loaded,
                    # else an unrelated scale-1 load (e.g. [rcx+rax-1]) reads as a map below the image.
                    if mp and mp["base"] is not None and \
                            mp["scale"] == 1 and \
                            mp["absolute"] is None and \
                            PARENT.get(mp["base"].lower(), mp["base"].lower()) == basep:
                        bmap = baseva + mp["disp"]
                        break
            # The windows the switch itself reads (byte map, dword table): the loads that feed the JMP register are baked
            # (Emitter.bake_ro) even where no guard walk finds the index bound (CDQE; an index that is a byte-map value),
            # else the GF runtime, which has no image view, reads 0 and every case falls to default.
            if bmap is not None:
                braw = img.read(bmap, count)
                bvals = list(struct.unpack("<%dB" % count, braw))
                n2 = max(bvals) + 1
                raw = img.read(tva, n2 * 4)
                words = list(struct.unpack("<%dI" % n2, raw))
                tabs = {bmap: count, tva: n2 * 4}
            else:
                raw = img.read(tva, count * 4)
                words = list(struct.unpack("<%dI" % count, raw))
                tabs = {tva: count * 4}
            jmpreg[addr] = {"table": tva, "words": words, "base": baseva,
                            "default": default, "tabs": tabs}
            for w in words:
                if baseva + w in addrs:
                    labels.add(baseva + w)
            if default in addrs:
                labels.add(default)
        else:
            jmpreg[addr] = {"table": None, "tail": None}
    return labels, jmpreg

def stub_lines(do):
    """C++ for one stubbed callee's modelled effect (names: stubs.py DO)."""
    if do == "sqrt_xmm0":
        return ["s.set_x_f32(0, pes_model_sqrtf(s.x_f32(0)));"]
    if do == "logf_xmm0":
        return ["s.set_x_f32(0, pes_model_logf(s.x_f32(0)));"]
    if do == "powf_xmm0":
        return ["s.set_x_f32(0, pes_model_powf(s.x_f32(0), s.x_f32(1)));"]
    if do == "isnan_xmm0":
        return ["s.gpr[0] = (uint64_t)pes_model_isnan(s.x_f64(0));"]
    if do == "floorf_xmm0":
        return ["s.set_x_f32(0, pes_model_floorf(s.x_f32(0)));"]
    if do in ("sinf_xmm0", "cosf_xmm0", "tanf_xmm0", "asinf_xmm0", "atanf_xmm0", "acosf_xmm0"):
        return ["s.set_x_f32(0, pes_model_%s(s.x_f32(0)));" % do[:-5]]
    if do == "atan2f_xmm0":
        return ["s.set_x_f32(0, pes_model_atan2f(s.x_f32(0), s.x_f32(1)));"]
    if do == "noop":
        return ["/* stub: engine-noop, no effect on the success path */"]
    if do == "abort":
        # Tamper trap: reaching it aborts, exactly as the harness errors.
        return ["abort();"]
    raise ValueError("stub_lines: no model for %r" % (do,))


def emit_function(em, va, lines, labels, jmpreg, pilot, stubs=None):
    addrs = {a for a, _, _ in lines}
    ctx = {"jmpreg": jmpreg, "addrs": addrs, "stubs": stubs or {}}
    trace_pc = bool(os.environ.get("GF_XLATE_TRACE"))  # lockstep: one GF_TRACE_PC per instruction (snapdiff build only)
    body = []
    em.lines, em.labels, em.guards = lines, labels, find_guards(lines)
    for info in jmpreg.values():
        em.sw_tabs.update(info.get("tabs", {}))
    em.defs = reaching_defs(lines, jmpreg)
    for idx, (addr, mnem, ops) in enumerate(lines):
        em.cur = idx
        if addr in labels:
            body.append("L_%X:;" % addr)
        if trace_pc:
            body.append("GF_TRACE_PC(0x%X, s);" % addr)
        m = mnem.upper()
        if m.startswith("LOCK "):
            m = m[5:]  # single-threaded harness: LOCK changes atomicity only
        out = None
        if m in ("MOV", "MOVABS", "MOVSX", "MOVSXD", "MOVZX", "LEA", "XCHG", "ADD",
                 "SUB", "CMP", "TEST", "AND", "OR", "XOR", "ADC", "SBB",
                 "INC", "DEC", "NEG", "NOT", "SHL", "SHR", "SAR", "IMUL",
                 "ROR", "ROL", "XADD", "CMPXCHG", "BSF", "BSR",
                 "MUL", "BT", "BTC", "BTR", "BTS", "CDQE", "CDQ", "CQO",
                 "CWD", "CBW", "CWDE", "PUSH", "POP", "LEAVE", "NOP"):
            out = em.emit_alu(m, ops)
        elif m in ("MOVSS", "MOVSD", "MOVD", "ADDSS", "SUBSS", "MULSS",
                   "DIVSS", "MAXSS", "MINSS", "COMISS", "UCOMISS",
                   "CVTTSS2SI", "CVTSI2SS", "CVTSS2SI", "CVTSS2SD",
                   "CVTSD2SS", "MOVAPS", "MOVUPS", "MOVAPD", "MOVUPD",
                   "ANDPS", "ORPS", "XORPS", "ANDPD", "ORPD", "XORPD",
                   "PXOR", "PAND", "POR", "PANDN", "MOVDQA", "MOVDQU",
                   "ANDNPS", "ANDNPD", "CVTPS2PD", "CVTPD2PS", "CVTDQ2PS",
                   "ADDPS", "SUBPS", "MULPS", "DIVPS", "PADDD", "PSUBD", "PCMPEQD",
                   "PSHUFD", "PSRLDQ", "SHUFPS", "UNPCKLPS", "UNPCKHPS",
                   "PSLLD", "PSRLD", "PUNPCKLDQ", "PUNPCKHDQ", "CVTTPS2DQ",
                   "ADDSD", "SUBSD", "MULSD", "DIVSD", "COMISD", "UCOMISD",
                   "CVTTSD2SI", "CVTSI2SD",
                   "CVTPS2DQ", "CVTPD2DQ", "CVTDQ2PD", "MOVQ", "MOVHLPS", "MOVLHPS",
                   "PUNPCKLQDQ", "PUNPCKHQDQ", "PADDQ", "MINPS", "MAXPS", "HADDPS",
                  "SQRTPS", "RCPPS", "RSQRTPS", "CMPNLTSS", "CMPEQPS", "CMPLTPS", "CMPLEPS",
                  "CMPUNORDPS", "CMPNEQPS", "CMPNLTPS", "CMPNLEPS", "CMPORDPS"):
            out = em.emit_sse(m, ops)
        elif m.startswith("REP ") and m[4:] in ("STOSB", "STOSW", "STOSD", "STOSQ",
                                                  "MOVSB", "MOVSW", "MOVSD", "MOVSQ"):
            out = em.do_string(m[4:], rep=True)
        elif m in ("STOSB", "STOSW", "STOSD", "STOSQ", "MOVSB", "MOVSW", "MOVSQ"):
            out = em.do_string(m, rep=False)
        elif m == "INT3":
            # A trap: the harness stops (and excludes) any case that reaches one, so reaching it here is a loud abort.
            out = ["__builtin_trap();"]
        elif m in ("DIV", "IDIV"):
            out = em.do_div(m, ops)
        elif m in CCMAP or m in ("RET", "CALL", "JMP", "LOOP"):
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
            if n % 4 == 2:
                decls.append("static constexpr uint16_t k_c%X_h = 0x%04XU;" % (va, struct.unpack("<H", blob[:2])[0]))
            if n % 2 == 1:
                decls.append("static constexpr uint8_t k_c%X_b = 0x%02XU;" % (va, blob[0]))
    return decls


def ro_decls(em):
    """Baked read-only image windows: build-time bytes of PES's image."""
    decls = []
    for (start, span), blob in sorted(em.ro_tabs.items()):
        decls.append("// image window @ 0x%X (%d bytes, read-only)"
                     % (start, span))
        decls.append("static constexpr uint8_t k_t%X_%X[%d] = {%s};" % (
            start, span, span, ", ".join("0x%02X" % b for b in blob)))
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
    LIVE_RO[va] = em.live_ro
    # tail jumps to in-set VAs: fill from the sibling set
    out = []
    for b in body:
        out.append(b)
    head = ["// Generated by tools/dev/translate/xlate.py: PES function %s." % va,
            "// Constants baked from the user's image at generation time (VA-cited).",
            "// Committed under src/pes/translated; regenerate with tools/dev/translate/gen_translated.py.",
            '#include "pes_state.hpp"',
            '#include "pes_dispatch.hpp"',
            '#include "pes_cond.inl"',
            '#include "pes_stubs.hpp"',
            '#include <cstdlib>',
            ""]
    head.extend(const_decls(em))
    head.extend(ro_decls(em))
    head.append("")
    head.append("void pes_fn_%s(PesState& s);" % va[2:])
    for sib in sorted(em.siblings):
        if sib != va:
            head.append("void pes_fn_%s(PesState& s);" % sib[2:])
    for (tva, n), blob in sorted(em.consts.items()):
        pass
    # Structured control flow where the CFG allows it (flow.py). Loops and unmodelled forms keep their gotos.
    flat = [l for b in out for l in b.split("\n")]
    body = flow.structure_lines(["  " + l for l in flat])
    if body is None:
        body = ["  " + l for l in flat]
    src = head + ["", "void pes_fn_%s(PesState& s) {" % va[2:]] + body + ["}"]
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
