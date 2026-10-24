#!/usr/bin/env python3
"""emit_alu.py: integer ALU emission. emit(em, addr, mnem, ops) -> [str] | None.

Covers: MOV LEA XCHG MOVSX MOVZX MOVSXD CDQE CWD CDQ CQO CBW CWDE
  ADD SUB CMP TEST AND OR XOR INC DEC NEG NOT ADC SBB
  SHL SHR SAR ROL ROR MUL IMUL DIV IDIV
  BT BTC BTR BTS PUSH POP LEAVE
Flags: ZF/SF from the truncated result, CF/OF per width. PF untouched
  (no JP reads an integer op's PF in the pilot set).
"""
from emit_fn import imm_val, is_imm
from emit_vals import reg_read, reg_write, sext
from lift import GPR_INDEX, SUB


def is_reg(s):
    return s.lower() in GPR_INDEX or s.lower() in SUB


def is_mem(s):
    return s.startswith("[") or (s.lower().startswith("dword ptr [")
                                 or s.lower().startswith("qword ptr [")
                                 or s.lower().startswith("word ptr [")
                                 or s.lower().startswith("byte ptr [")
                                 or "ptr [" in s.lower())


def mem_inner(op):
    i = op.find("[")
    return op[i:] if i >= 0 else op
