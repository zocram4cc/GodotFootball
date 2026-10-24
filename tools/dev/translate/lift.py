#!/usr/bin/env python3
"""Lifter: capstone disassembly -> C++ over PesState (one function).

Memory model (matches run.py exactly):
  - Machine memory is one flat byte array; image bytes are baked at their VAs
    (constants read from the user's image at build time, VA-cited).
  - Regions/patch_addrs/regions_init from the spec are applied the same way.
  - [reg+off] accesses hit caller-supplied buffers (the diff-test main fills
    exactly the corpus offsets; everything else is zero, like the harness).
  - Globals [0x14..] read the baked image bytes (live-state caveat documented
    per function: value is the user's image at build time).

State: gpr[16] u64, sub-register views, xmm[16] as u128 (f32x4 + u64x2 +
  f64x2 views, lane-precise), flags ZF/PF/CF/SF/OF, RIP dispatch via labels.

Integer width discipline: every value carries its width (8/16/32/64).
  32-bit writes zero-extend (MOV EAX,... clears top); 8/16-bit preserve.
  SIGNEDNESS COMES FROM THE OPCODE: JL/JGE/SETL/CMOVL = signed 64 (or the
  operand width); JA/JBE = unsigned; MOVSXD/MOVSX sign-extend, MOVZX zero.
Float discipline: scalar ops touch lane 0 only; vector moves (MOVAPS/MOVSD/
  MOVD/MOVUPS/CVTP*) transfer the full 128 bits; COMISS/UCOMISS set ZF/PF/CF
  with NaN -> ZF=PF=CF=1 (JP distinguishes); CVTTSS2SI truncates toward zero
  (x86, not C++ cast on overflow — saturate to 0x80000000 like hardware).

Switch tables (LEA base 0x140000000 + dword table + ADD + JMP reg): baked
  from image bytes at build time into a static const u32 table; the C++
  dispatches `switch (idx)` over the observed case targets with `default:`
  to the JA-fallthrough. Deterministic: sorted emission, fixed float format.
"""
import argparse
import json
import os
import struct
import sys

BASE = 0x140000000

GPR_INDEX = {"rax": 0, "rcx": 1, "rdx": 2, "rbx": 3, "rsp": 4, "rbp": 5,
             "rsi": 6, "rdi": 7, "r8": 8, "r9": 9, "r10": 10, "r11": 11,
             "r12": 12, "r13": 13, "r14": 14, "r15": 15}

# sub -> (parent, bits, kind) kind: zx zero-extends, sx sign-extends, pr preserve
SUB = {"eax": ("rax", 32, "zx"), "ebx": ("rbx", 32, "zx"),
       "ecx": ("rcx", 32, "zx"), "edx": ("rdx", 32, "zx"),
       "esi": ("rsi", 32, "zx"), "edi": ("rdi", 32, "zx"),
       "ebp": ("rbp", 32, "zx"), "esp": ("rsp", 32, "zx"),
       "r8d": ("r8", 32, "zx"), "r9d": ("r9", 32, "zx"),
       "r10d": ("r10", 32, "zx"), "r11d": ("r11", 32, "zx"),
       "r12d": ("r12", 32, "zx"), "r13d": ("r13", 32, "zx"),
       "r14d": ("r14", 32, "zx"), "r15d": ("r15", 32, "zx"),
       "ax": ("rax", 16, "pr"), "bx": ("rbx", 16, "pr"),
       "cx": ("rcx", 16, "pr"), "dx": ("rdx", 16, "pr"),
       "si": ("rsi", 16, "pr"), "di": ("rdi", 16, "pr"),
       "bp": ("rbp", 16, "pr"), "sp": ("rsp", 16, "pr"),
       "r8w": ("r8", 16, "pr"), "r9w": ("r9", 16, "pr"),
       "al": ("rax", 8, "pr"), "bl": ("rbx", 8, "pr"),
       "cl": ("rcx", 8, "pr"), "dl": ("rdx", 8, "pr"),
       "sil": ("rsi", 8, "pr"), "dil": ("rdi", 8, "pr"),
       "bpl": ("rbp", 8, "pr"), "spl": ("rsp", 8, "pr"),
       "r8b": ("r8", 8, "pr"), "r9b": ("r9", 8, "pr"),
       "r10b": ("r10", 8, "pr"), "r11b": ("r11", 8, "pr"),
       "r12b": ("r12", 8, "pr"), "r13b": ("r13", 8, "pr"),
       "r14b": ("r14", 8, "pr"), "r15b": ("r15", 8, "pr"),
       "ah": ("rax", 8, "pr"), "bh": ("rbx", 8, "pr"),
       "ch": ("rcx", 8, "pr"), "dh": ("rdx", 8, "pr"),
       "ax_": ("rax", 16, "pr")}
SUB_SHIFT = {"ah": 8, "bh": 8, "ch": 8, "dh": 8}


def canon_reg(name):
    n = name.lower()
    if n in GPR_INDEX:
        return ("gpr", GPR_INDEX[n], 64, "pr", 0)
    if n in SUB:
        parent, bits, kind = SUB[n]
        return ("gpr", GPR_INDEX[parent], bits, kind,
                SUB_SHIFT.get(n, 0))
    if n.startswith("xmm"):
        return ("xmm", int(n[3:]), 128, "xmm", 0)
    return None


def parse_mem(inner):
    """Copied minimal parser (keep in sync with livein.mem_parts)."""
    parts = [p.strip().lower() for p in inner.split('+')]
    base = None
    index = None
    scale = 1
    nums = []
    ok = True
    for p in parts:
        import re as _re
        mm = _re.fullmatch(r'([a-z][a-z0-9]*)\*0x([0-9a-f]+)', p)
        if mm and (mm.group(1) in GPR_INDEX or mm.group(1) in SUB):
            index = mm.group(1)
            scale = int(mm.group(2), 16)
            continue
        mm = _re.fullmatch(r'([a-z][a-z0-9]*)\*(\d+)', p)
        if mm and (mm.group(1) in GPR_INDEX or mm.group(1) in SUB):
            index = mm.group(1)
            scale = int(mm.group(2))
            continue
        if p in GPR_INDEX or p in SUB:
            if base is None:
                base = p
            elif index is None:
                index = p
                scale = 1
            else:
                ok = False
            continue
        try:
            nums.append(int(p, 16) if p.startswith('0x') else int(p))
            continue
        except ValueError:
            ok = False
    if not ok:
        return None
    disp = sum(nums)
    absolute = nums[0] if (base is None and index is None and nums) else None
    return {"base": base, "index": index, "scale": scale, "disp": disp,
            "absolute": absolute}
