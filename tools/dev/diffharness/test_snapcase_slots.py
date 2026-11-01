#!/usr/bin/env python3
"""snapcase.Replayer's return-address model on a tiny real emulation (CutAnchor6, the 0x140477BC0 replay).

The translation pushes 0 where PES pushes a return address, so the harness must read a return-address slot as 0 for
every read but the callee's own RET (0x141F5D770 reads its caller's dead home slot and stores the byte, which put a stale
return address's low byte 0xF7 on one side only), must give RET the real address, must stop treating a slot as one once
the code overwrites it as data, and must decode a CALL that ends at a page end whose next page is unmapped (0x141F50FF3).
Usage: GF_DIFF_PYTHON=<venv python with unicorn+capstone> python3 test_snapcase_slots.py   (exit 77: no unicorn)
"""
import os
import sys
import unittest

try:
    import capstone
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
    import unicorn.x86_const as xc
except ImportError:
    sys.exit(77)

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "translate"))
import snapcase  # noqa: E402

CODE, STACK, PAGE = 0x10000, 0x40000, 0x1000


def replayer():
    r = snapcase.Replayer.__new__(snapcase.Replayer)  # the hooks only; no snapshot
    r.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    r.md.detail = True
    r._rsp = xc.UC_X86_REG_RSP
    r.stub_vas = set()
    return r


def run(code, at=CODE, steps=20, second_page=True):
    """Emulate `code` placed at `at` with the hooks of one replay; returns the engine after it halts on `hlt` (a fault)."""
    r = replayer()
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(CODE, 2 * PAGE if second_page else PAGE)
    uc.mem_map(STACK, 2 * PAGE)
    uc.mem_write(at, code)
    uc._last_rip, uc._writes, uc._call_pushes = 0, [], 0
    uc._call_slots, uc._call_real, uc._ret_slot_reads = set(), {}, 0
    uc.hook_add(UC_HOOK_CODE, lambda u, a, s, d: setattr(u, "_last_rip", a))
    uc.hook_add(UC_HOOK_MEM_WRITE, r._on_write)
    uc.hook_add(UC_HOOK_MEM_READ, r._on_read)
    uc.reg_write(xc.UC_X86_REG_RSP, STACK + PAGE)
    uc.reg_write(xc.UC_X86_REG_RAX, 0xDEAD)
    try:
        uc.emu_start(at, CODE + PAGE, count=steps)  # ends at the hlt (or the step count)
    except Exception:
        pass
    return uc


class TestSlots(unittest.TestCase):
    def test_dead_slot_read_by_a_plain_load_is_zero_and_ret_still_returns(self):
        # call f ; mov rax,[rsp-8] (the dead return slot) ; hlt ; f: ret
        code = bytes.fromhex("e808000000" "488b4424f8" "f4" "c3")
        # call rel32 = +8 -> the final `ret` at offset 5+5+1 = 11, so the call target is 0x10000+5+8 = 0x1000d? recompute below
        code = bytearray(code)
        code[1:5] = (0x10000 + 11 - (0x10000 + 5)).to_bytes(4, "little")
        uc = run(bytes(code))
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RAX), 0, "the translation pushed 0 there")
        self.assertEqual(uc._ret_slot_reads, 1)
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RSP), STACK + PAGE, "RET popped the real address and came back")

    def test_ret_gets_the_real_address_after_the_callee_read_its_own_slot_as_zero(self):
        # call f ; hlt ; f: mov rax,[rsp] ; ret   (the callee's plain load reads 0, its RET must still return)
        uc = run(bytes.fromhex("e801000000" "f4" "488b0424" "c3"))
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RAX), 0)
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RSP), STACK + PAGE)
        self.assertEqual(uc._last_rip, CODE + 5, "the RET came back to the hlt after the call")

    def test_data_written_over_a_slot_reads_back_unchanged(self):
        # call f ; mov qword [rsp-8], 0x55 ; mov rax,[rsp-8] ; hlt ; f: ret
        code = bytearray.fromhex("e800000000" "48c74424f855000000" "488b4424f8" "f4" "c3")
        code[1:5] = (5 + 9 + 5 + 1 - 5).to_bytes(4, "little")
        uc = run(bytes(code))
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RAX), 0x55)

    def test_a_partial_write_over_a_slot_leaves_zeros_in_the_other_bytes(self):
        # call f ; mov word [rsp-8], 0x100 ; mov rax,[rsp-8] ; hlt ; f: ret. The translation holds 0 in the slot, so the six
        # bytes the word store does not cover read 0, not the real return address's bytes (the registry find of the FixDemo
        # camera constructor stores one byte of a struct and reads the qword: ProcCamera3).
        code = bytearray.fromhex("e800000000" "66c74424f80001" "488b4424f8" "f4" "c3")
        code[1:5] = (7 + 5 + 1).to_bytes(4, "little")
        uc = run(bytes(code))
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RAX), 0x100)

    def test_a_call_ending_at_a_page_end_is_decoded_with_the_next_page_unmapped(self):
        # call +0 at 0x10FFB..0x10FFF: the push happens with 0x11000 unmapped for a 16-byte fetch.
        r = replayer()
        uc = Uc(UC_ARCH_X86, UC_MODE_64)
        uc.mem_map(CODE, PAGE)
        uc.mem_map(STACK, 2 * PAGE)
        uc.mem_write(CODE + PAGE - 5, bytes.fromhex("e800000000"))
        uc._last_rip, uc._writes, uc._call_pushes = CODE + PAGE - 5, [], 0
        uc._call_slots, uc._call_real, uc._ret_slot_reads = set(), {}, 0
        uc.reg_write(xc.UC_X86_REG_RSP, STACK + PAGE)
        ins = r._insn(uc)
        self.assertEqual(ins.mnemonic, "call")


if __name__ == "__main__":
    unittest.main()
