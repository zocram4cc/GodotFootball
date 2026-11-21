#!/usr/bin/env python3
"""The proof harness's guard on the graphics device allocator (boundary_guard.py; owner ruling option A, 21-11-26).

Synthetic arms (hand-encoded; the target is a made-up VA 0x2000 so the call's rel32 fits): a call whose skipped arm writes only
through the returned pointer passes; an arm that writes elsewhere, pushes, drops the pointer, calls a non-pure callee, or sits
behind a jne fails, with the site's address in the reason. A call to a pure callee (a getter) is allowed and clobbers the volatile
registers. Real sites (PES_EXE set, else skipped): the pure getter 0x1421F8690; the closure functions of 1408BED00 (0x1419DC480) and
1408BE3C0 (0x1419DBAF0), whose allocator sites pass; and the failing site 0x14195A634 (a call inside its skipped arm).
Usage: GF_DIFF_PYTHON=<venv python> PES_EXE=<unpacked image> python3 test_boundary_guard.py
"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import boundary_guard as g  # noqa: E402
import capstone  # noqa: E402
import pdata  # noqa: E402

BASE, SITE, TARGET, PURE = 0x1000, 0x1000, 0x2000, 0x1800
IMAGE_BASE = 0x140000000
PES_EXE = os.environ.get("PES_EXE")

# Hand-encoded instructions (a wrong byte would show as a wrong reason in the tests).
MOV_R8_RAX = bytes.fromhex("4989c0")  # mov r8, rax
MOV_RBX_RAX = bytes.fromhex("4889c3")  # mov rbx, rax (the packet in a non-volatile register)
TEST_RAX = bytes.fromhex("4885c0")  # test rax, rax
MOV_DWORD_RAX = bytes.fromhex("c700") + struct.pack("<I", 0x200004)  # mov dword ptr [rax], 0x200004 (6 bytes)
MOV_DWORD_RBX = bytes.fromhex("c703") + struct.pack("<I", 0x200004)  # mov dword ptr [rbx], 0x200004 (6 bytes)
MOV_WORD_R8_CX = bytes.fromhex("66418948" "04")  # mov word ptr [r8+4], cx (5 bytes)
MOV_RBX8_RCX = bytes.fromhex("48894b08")  # mov qword ptr [rbx+8], rcx (4 bytes): outside the packet
MOV_RAX_RBX = bytes.fromhex("4889d8")  # mov rax, rbx (3 bytes): drops the pointer
MOV_RSP28_RCX = bytes.fromhex("48894c2428")  # mov qword ptr [rsp+0x28], rcx (5 bytes): the stack
RET = b"\xc3"


def call(target=TARGET, site=SITE):
    return b"\xe8" + struct.pack("<i", target - (site + 5))


def je(n):
    return b"\x74" + struct.pack("<b", n)


def site_bytes(arm, jump=None):
    """call; mov r8,rax; test rax,rax; je join (over arm); arm; join: ret."""
    jump = je(len(arm)) if jump is None else jump
    return call() + MOV_R8_RAX + TEST_RAX + jump + arm + RET


def machine():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    return md


def reader(code):
    def read(va, n):
        off = va - BASE
        return code[off:off + n]
    return read


class SyntheticArms(unittest.TestCase):
    def check(self, code, is_pure=None):
        return g.check_site(machine(), reader(code), SITE, TARGET, is_pure)

    def test_an_arm_that_writes_only_through_the_packet_passes(self):
        self.assertIsNone(self.check(site_bytes(MOV_DWORD_RAX + MOV_WORD_R8_CX)))

    def test_an_arm_writing_outside_the_packet_fails_at_the_site(self):
        reason = self.check(site_bytes(MOV_DWORD_RAX + MOV_RBX8_RCX))
        self.assertIsNotNone(reason)
        self.assertIn("outside the packet", reason)
        self.assertIn(hex(SITE), reason)

    def test_an_arm_writing_the_stack_fails(self):
        reason = self.check(site_bytes(MOV_DWORD_RAX + MOV_RSP28_RCX))
        self.assertIn("outside the packet", reason or "")

    def test_an_arm_that_calls_a_non_pure_callee_fails(self):
        reason = self.check(site_bytes(call(target=PURE, site=SITE + 13)), is_pure=lambda va: False)
        self.assertIn("not a pure callee", reason or "")

    def test_an_arm_that_overwrites_the_pointer_before_writing_fails(self):
        reason = self.check(site_bytes(MOV_RAX_RBX + MOV_DWORD_RAX))
        self.assertIn("outside the packet", reason or "")

    def test_a_pure_callee_in_the_arm_passes_and_the_packet_in_rbx_survives_it(self):
        head = call() + MOV_RBX_RAX + TEST_RAX
        arm = call(target=PURE, site=SITE + len(head) + 2) + MOV_DWORD_RBX
        code = head + je(len(arm)) + arm + RET
        self.assertIsNone(self.check(code, is_pure=lambda va: va == PURE))

    def test_a_pure_callee_clobbers_the_packet_held_in_a_volatile_register(self):
        arm = call(target=PURE, site=SITE + 13) + MOV_DWORD_RAX  # the packet was copied to r8/rax: a volatile register
        reason = self.check(site_bytes(arm), is_pure=lambda va: va == PURE)
        self.assertIn("outside the packet", reason or "")

    def test_a_jne_skip_is_refused_loudly_not_guessed(self):
        arm = MOV_DWORD_RAX
        jne = b"\x75" + struct.pack("<b", len(arm))
        reason = self.check(site_bytes(arm, jump=jne))
        self.assertIn("not followed by je/jz", reason or "")


class RealSites(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not PES_EXE:
            raise unittest.SkipTest("PES_EXE is not set")
        cls.img = open(PES_EXE, "rb").read()
        cls.pd = pdata.Pdata(open(PES_EXE, "rb"))
        cls.md = machine()

    def read(self, va, n):
        return self.img[va - IMAGE_BASE:][:n]

    def test_the_getter_0x1421F8690_is_pure_and_a_store_is_not(self):
        self.assertTrue(g.is_pure(self.pd, self.md, 0x1421F8690))
        self.assertFalse(g.is_pure(self.pd, self.md, 0x1419DBAF0))  # the closure function holds the allocator's packet writes

    def test_the_closure_function_of_1408BED00_passes_every_allocator_site(self):
        self.assertEqual(g.check_function(self.pd, self.md, self.read, 0x1419DC480), [])
        calls = [i for i in self.pd.decode(self.md, 0x1419DC480)
                 if i.mnemonic == "call" and i.op_str.strip() == hex(g.GFX_DEVICE_ALLOC)]
        self.assertGreater(len(calls), 0, "the function must contain an allocator call for the pass to mean anything")

    def test_1408BE3C0s_allocator_site_fails_on_the_reference_count_it_bumps_outside_the_packet(self):
        # 0x1419DBC0C is the allocator call; its skipped arm calls 0x1403DB250 (`mov eax,1; lock xadd [rcx],eax`: the object
        # reached through the packet, +0x20 and +0x28, has its count bumped). That write is outside the packet: the proof fails.
        fails = dict(g.check_function(self.pd, self.md, self.read, 0x1419DBAF0))
        self.assertIn(0x1419DBC0C, fails)
        self.assertIn("0x1403db250", fails[0x1419DBC0C])

    def test_a_real_site_with_a_call_in_its_skipped_arm_fails_at_that_arm(self):
        head = self.pd.containing(0x14195A634)
        fails = dict(g.check_function(self.pd, self.md, self.read, head))
        self.assertIn(0x14195A634, fails)
        self.assertIn("0x14195a695", fails[0x14195A634])
        self.assertIn("call", fails[0x14195A634])


if __name__ == "__main__":
    unittest.main()
