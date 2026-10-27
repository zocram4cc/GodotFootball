#!/usr/bin/env python3
"""Tests for frame_trace.run_recorded: one recorded call, emulated at its captured entry, rsp,
return address, registers, XMM, flags, MXCSR and GS base.

The snapshots are synthetic (built here, no game bytes): a few pages holding a known function, so
each test pins one behaviour of the emulator entry. Run by ctest (translate_recorded_call_test) with
the harness Python, which has unicorn; without it the test skips (77).

usage: test_recorded_call.py
"""
import json
import os
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "diffharness"))

import frame_trace as ft  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402

PAGE = 0x1000
CODE = 0x400000        # the recorded function's entry page
RET = 0x401000         # the caller's return address (never executed: the call stops there)
STACK = 0x700000       # the stack page; the recorded rsp sits inside it
TEB = 0x800000         # the game's TEB page: +0x30 holds its own address (NT_TIB.Self)
RSP = STACK + 0x800
PAGE_VAS = (CODE, RET, STACK, TEB)
TRAMP = CODE + 0x100   # a mapped stub a hook patch can jump to (stands in for a trampoline)

LEA_RAX_RCX_RDX = bytes.fromhex("488d0451")   # lea rax,[rcx+rdx*2]
RET_OP = b"\xc3"
# A 14-byte rendezvous patch: jmp [rip+0] (FF 25 + rel32 0) then the 8-byte target it reads.
PATCH_HEAD = bytes.fromhex("ff25" + "00000000")
PATCH_TO_TRAMP = PATCH_HEAD + struct.pack("<Q", TRAMP)
PATCH_TO_ABSENT = PATCH_HEAD + struct.pack("<Q", 0)
MOV_EAX_1_RET = bytes.fromhex("b801000000") + RET_OP   # what a hook's trampoline stands for here


def make_snapshot(d, code, regs=None, xmm=None, flags=0x202, mxcsr=0x1F80, patch=None, tramp=None):
    """Write a rendezvous-style snapshot directory d: index.json, mem.bin, call.bin. The entry holds
    code, or patch (a hook's jump) when given; tramp is code mapped at TRAMP."""
    pages = {va: bytearray(PAGE) for va in PAGE_VAS}
    pages[CODE][:len(code)] = code
    if patch is not None:
        pages[CODE][:len(patch)] = patch
    if tramp is not None:
        pages[CODE][TRAMP - CODE:TRAMP - CODE + len(tramp)] = tramp
    pages[RET][0] = 0x90
    pages[STACK][RSP - STACK:RSP - STACK + 8] = struct.pack("<Q", RET)
    pages[TEB][0x30:0x38] = struct.pack("<Q", TEB)
    mappings, blob = [], bytearray()
    for i, va in enumerate(PAGE_VAS):
        mappings.append({"va": va, "size": PAGE, "perm": "rwxp", "name": "", "off": i * PAGE,
                         "deferred": False, "device": False})
        blob += pages[va]
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "mem.bin"), "wb") as f:
        f.write(bytes(blob))
    with open(os.path.join(d, "index.json"), "w") as f:
        json.dump({"label": "synthetic", "mappings": mappings, "holes": [],
                   "rv": {"seq": 1, "hook": "t", "entry_va": CODE, "calls": 1, "tid": 1,
                          "teb": TEB, "flags": flags, "mxcsr": mxcsr, "qpc_freq": 1,
                          "t_capture": 0, "page_va": 0}}, f)
    gprs = {"rax": 0, "rcx": 0, "rdx": 0, "rbx": 0, "rsp": RSP, "rbp": 0, "rsi": 0, "rdi": 0,
            "r8": 0, "r9": 0, "r10": 0, "r11": 0, "r12": 0, "r13": 0, "r14": 0, "r15": 0}
    gprs.update(regs or {})
    xs = [0] * 32
    for i, (lo, hi) in enumerate(xmm or []):
        xs[2 * i], xs[2 * i + 1] = lo, hi
    order = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
             "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15")
    vals = [gprs[r] for r in order] + xs + [RET]
    with open(os.path.join(d, "call.bin"), "wb") as f:
        f.write(ft.CALL_REC.pack(*vals))


def f64_bits(x):
    return struct.unpack("<Q", struct.pack("<d", x))[0]


def xmm_low_double(uc, n):
    v = uc.reg_read(xc.UC_X86_REG_XMM0 + n)
    if isinstance(v, (bytes, bytearray)):
        v = int.from_bytes(v, "little")
    return struct.unpack("<d", struct.pack("<Q", v & ((1 << 64) - 1)))[0]


class RecordedCall(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.d = os.path.join(self.tmp.name, "snap")

    def tearDown(self):
        self.tmp.cleanup()

    def test_gprs_and_return_are_the_recorded_call(self):
        # The result depends on the recorded rcx and rdx; the return lands on the caller's
        # address with the caller's rsp (rsp + 8 after ret).
        make_snapshot(self.d, LEA_RAX_RCX_RDX + RET_OP, regs={"rcx": 5, "rdx": 7})
        uc, err = ft.run_recorded(self.d, budget=10_000)
        self.assertIsNone(err)
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RAX), 19)
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RSP), RSP + 8)

    def test_xmm_inputs_are_the_recorded_xmm(self):
        # addsd xmm0, xmm1 ; ret. Both operands come from the record.
        make_snapshot(self.d, bytes.fromhex("f20f58c1") + RET_OP,
                      xmm=[(f64_bits(1.5), 0), (f64_bits(2.25), 0)])
        uc, err = ft.run_recorded(self.d, budget=10_000)
        self.assertIsNone(err)
        self.assertAlmostEqual(xmm_low_double(uc, 0), 3.75)

    def test_gs_base_is_the_recorded_teb(self):
        # mov rax, gs:[0x30] ; ret reads the TEB's self pointer through the recorded gs base.
        make_snapshot(self.d, bytes.fromhex("65488b042530000000") + RET_OP)
        uc, err = ft.run_recorded(self.d, budget=10_000)
        self.assertIsNone(err)
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RAX), TEB)

    def test_rflags_are_the_recorded_flags(self):
        # pushfq ; pop rax ; ret: the recorded CF and IF must be visible to the code.
        make_snapshot(self.d, bytes.fromhex("9c58") + RET_OP, flags=0x203)
        uc, err = ft.run_recorded(self.d, budget=10_000)
        self.assertIsNone(err)
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RAX) & 0x203, 0x203)

    def test_mxcsr_is_the_recorded_mxcsr(self):
        # sub rsp,8 ; stmxcsr [rsp] ; mov eax,[rsp] ; add rsp,8 ; ret
        code = bytes.fromhex("4883ec08" + "0fae1c24" + "8b0424" + "4883c408") + RET_OP
        make_snapshot(self.d, code, mxcsr=0x9FC0)
        uc, err = ft.run_recorded(self.d, budget=10_000)
        self.assertIsNone(err)
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RAX) & 0xFFFFFFFF, 0x9FC0)

    def test_hook_patch_is_replaced_by_the_original_bytes(self):
        # The snapshot holds a 14-byte jmp patch at the entry (an rv.dll hook); the mapped stub it
        # jumps to would run the hook, not the function. The original bytes come from the config.
        orig = LEA_RAX_RCX_RDX + RET_OP + b"\x90" * 9  # 14 bytes: the displaced original
        make_snapshot(self.d, b"", regs={"rcx": 5, "rdx": 7}, patch=PATCH_TO_TRAMP,
                      tramp=MOV_EAX_1_RET)
        uc, err = ft.run_recorded(self.d, orig={CODE: orig}, budget=10_000)
        self.assertIsNone(err)
        self.assertEqual(uc.reg_read(xc.UC_X86_REG_RAX), 19)

    def test_patched_entry_without_original_is_refused(self):
        # Without the original bytes the patch would run: into a mapped stub (here, code that
        # returns 1) the replay would be silently wrong. It must be refused, naming the patch.
        make_snapshot(self.d, b"", regs={"rcx": 5, "rdx": 7}, patch=PATCH_TO_TRAMP,
                      tramp=MOV_EAX_1_RET)
        uc, err = ft.run_recorded(self.d, budget=10_000)
        self.assertIsNotNone(err)
        self.assertIn("patch", err)

    def test_patch_to_unmapped_target_is_an_error(self):
        make_snapshot(self.d, b"", regs={"rcx": 5, "rdx": 7}, patch=PATCH_TO_ABSENT)
        uc, err = ft.run_recorded(self.d, budget=10_000)
        self.assertIsNotNone(err)

    def test_no_return_within_budget_is_an_error(self):
        # jmp $ : the call never returns; the budget must stop it and say so.
        make_snapshot(self.d, bytes.fromhex("ebfe"))
        uc, err = ft.run_recorded(self.d, budget=1000)
        self.assertIsNotNone(err)
        self.assertIn("did not return", err)

    def test_unbalanced_return_is_an_error(self):
        # push rcx ; ret with rcx = the return address: control reaches the caller, but the stack
        # is one slot short (rsp = recorded rsp, not rsp + 8). The call did not return cleanly.
        make_snapshot(self.d, bytes.fromhex("51") + RET_OP, regs={"rcx": RET})
        uc, err = ft.run_recorded(self.d, budget=10_000)
        self.assertIsNotNone(err)
        self.assertIn("stack not balanced", err)

    def test_hook_config_gives_the_original_bytes(self):
        # rv.dll config line: hook NAME VA LEN FIRST EVERY HEX
        cfg = os.path.join(self.tmp.name, "hooks.cfg")
        with open(cfg, "w") as f:
            f.write("# comment\nhook f 0x400000 14 0 0 " + "00" * 14 + "\n")
        self.assertEqual(ft.hook_bytes(cfg), {CODE: bytes(14)})


if __name__ == "__main__":
    unittest.main()
