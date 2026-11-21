#!/usr/bin/env python3
"""The proof harness's guard on the graphics device allocator (boundary_guard.py; owner rulings option A and paired, 21-11-26).

Synthetic arms (hand-encoded; the allocator target is a made-up VA 0x2000): a call whose skipped arm writes only through the returned
pointer passes; an arm that writes elsewhere, pushes, drops the pointer, calls a non-pure callee, or sits behind a jne fails, with the
site's address in the reason. A call to a pure callee (a getter) is allowed and clobbers the volatile registers. A refcount increment
(the AddRef callee, recognised by its bytes at 0x1800) passes only when the same object was stored into the packet earlier in the arm;
an unpaired one, or one on an object the arm did not store, fails. Real sites (PES_EXE set, else skipped): the AddRef bytes at 0x1403DB250;
the getter 0x1421F8690 is pure; the closure function of 1408BED00 (0x1419DC480) and of 1408BE3C0 (0x1419DBAF0) pass every allocator
site (the latter via the paired refcount); 0x14195A634 fails at its call in the skipped arm.
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

BASE, SITE, TARGET = 0x1000, 0x1000, 0x2000
PURE, ADDREF = 0x1900, 0x1800  # synthetic callees: a pure one (zero bytes, is_pure says so) and the AddRef bytes
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
LOAD_RCX_FROM_RDI = bytes.fromhex("488b0f")  # mov rcx, qword ptr [rdi] (a field of another object)
LOAD_RDX_FROM_RSI = bytes.fromhex("488b16")  # mov rdx, qword ptr [rsi]
XCHG_RCX2C_EAX = bytes.fromhex("87412c")  # xchg dword ptr [rcx+0x2c], eax (a flag store into the object; capstone-verified)
STORE_RCX_TO_PACKET20 = bytes.fromhex("48894b20")  # mov qword ptr [rbx+0x20], rcx
STORE_RDX_TO_PACKET20 = bytes.fromhex("48895320")  # mov qword ptr [rbx+0x20], rdx
LOAD_RCX_FROM_PACKET20 = bytes.fromhex("488b4b20")  # mov rcx, qword ptr [rbx+0x20]
ADD_RCX_8 = bytes.fromhex("4883c108")  # add rcx, 8
RET = b"\xc3"


def call(target=TARGET, site=SITE):
    return b"\xe8" + struct.pack("<i", target - (site + 5))


def je(n):
    return b"\x74" + struct.pack("<b", n)


def site_bytes(arm, jump=None):
    """call; mov r8,rax; test rax,rax; je join (over arm); arm; join: ret."""
    jump = je(len(arm)) if jump is None else jump
    return call() + MOV_R8_RAX + TEST_RAX + jump + arm + RET


def packet_site(arm_prefix, final_call=None):
    """call; mov rbx,rax; test rax,rax; je join; arm_prefix; (call final_call, if given); join: ret. The arm starts at SITE+13."""
    arm = arm_prefix + (call(target=final_call, site=SITE + 13 + len(arm_prefix)) if final_call is not None else b"")
    return call() + MOV_RBX_RAX + TEST_RAX + je(len(arm)) + arm + RET


def image(code):
    """The synthetic image: the site's code at BASE, the AddRef callee's bytes at ADDREF, zeros elsewhere."""
    buf = bytearray(0x2000)
    buf[:len(code)] = code
    buf[ADDREF - BASE:ADDREF - BASE + len(g.ADDREF_BYTES)] = g.ADDREF_BYTES
    return bytes(buf)


def machine():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    return md


def reader(code):
    buf = image(code)

    def read(va, n):
        off = va - BASE
        return buf[off:off + n]
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
        arm = call(target=PURE, site=SITE + 13) + MOV_DWORD_RBX
        code = call() + MOV_RBX_RAX + TEST_RAX + je(len(arm)) + arm + RET
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

    # Paired references: the AddRef callee at ADDREF increments [rcx+8]; the object must have been stored into the packet in the arm.

    def test_a_refcount_increment_of_an_object_stored_into_the_packet_in_the_arm_passes(self):
        arm = LOAD_RCX_FROM_RDI + STORE_RCX_TO_PACKET20 + LOAD_RCX_FROM_PACKET20 + ADD_RCX_8
        self.assertIsNone(self.check(packet_site(arm, final_call=ADDREF)))

    def test_an_unpaired_refcount_increment_fails(self):
        arm = LOAD_RCX_FROM_RDI + ADD_RCX_8
        reason = self.check(packet_site(arm, final_call=ADDREF))
        self.assertIn("unpaired", reason or "")
        self.assertIn(hex(SITE), reason)

    def test_a_refcount_increment_of_an_object_the_packet_does_not_hold_fails(self):
        # The arm stores rdx into the packet and increments rcx: a different object, so the increment is unpaired.
        arm = LOAD_RDX_FROM_RSI + STORE_RDX_TO_PACKET20 + LOAD_RCX_FROM_RDI + ADD_RCX_8
        reason = self.check(packet_site(arm, final_call=ADDREF))
        self.assertIn("unpaired", reason or "")

    def test_a_refcount_increment_of_a_packet_slot_written_before_the_arm_fails(self):
        # Reading [rbx+0x20] without storing it in the arm: the object was not stored by this arm's packet writes.
        arm = LOAD_RCX_FROM_PACKET20 + ADD_RCX_8
        reason = self.check(packet_site(arm, final_call=ADDREF))
        self.assertIn("unpaired", reason or "")


    # The non-null arm may exit past the null block (the real 1419DBAF0 shape): the null block is [join, exit) and only the null path runs it.

    def test_an_arm_that_jumps_over_the_null_block_passes(self):
        arm = MOV_DWORD_RBX + b"\xeb\x02"  # jmp +2: past the two-byte null block (nop nop) that starts at the join
        code = call() + MOV_RBX_RAX + TEST_RAX + je(len(arm)) + arm + b"\x90\x90" + RET
        self.assertIsNone(self.check(code))

    def test_an_arm_that_branches_to_a_join_that_is_the_continuation_passes(self):
        arm = MOV_DWORD_RBX + b"\x74\x00"  # je +0: to the join, which both paths reach (no null-only block)
        self.assertIsNone(self.check(call() + MOV_RBX_RAX + TEST_RAX + je(len(arm)) + arm + RET))

    def test_an_arm_that_branches_into_a_null_only_block_that_writes_outside_the_packet_fails(self):
        arm = b"\x74\x02" + b"\xeb\x04"  # je to the join (the null-only block's first byte); jmp past its four bytes
        code = call() + MOV_RBX_RAX + TEST_RAX + je(len(arm)) + arm + b"\x48\x89\x4f\x08" + RET  # mov [rdi+8], rcx
        self.assertIn("outside the packet", self.check(code) or "")


    def test_a_paired_write_into_the_stored_object_passes(self):
        arm = STORE_RCX_TO_PACKET20 + XCHG_RCX2C_EAX
        self.assertIsNone(self.check(packet_site(arm)))

    def test_a_write_into_an_object_the_arm_did_not_store_fails(self):
        reason = self.check(packet_site(XCHG_RCX2C_EAX))
        self.assertIn("outside the packet", reason or "")

    def test_a_write_into_a_different_object_than_the_one_stored_fails(self):
        arm = STORE_RDX_TO_PACKET20 + XCHG_RCX2C_EAX  # stored rdx, wrote into rcx: a different object
        reason = self.check(packet_site(arm))
        self.assertIn("outside the packet", reason or "")
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

    def test_the_AddRef_callee_is_recognised_by_its_bytes(self):
        self.assertEqual(self.read(0x1403DB250, len(g.ADDREF_BYTES)), g.ADDREF_BYTES)

    def test_the_getter_0x1421F8690_is_pure_and_the_closure_function_of_1408BE3C0_is_not(self):
        self.assertTrue(g.is_pure(self.pd, self.md, 0x1421F8690))
        self.assertFalse(g.is_pure(self.pd, self.md, 0x1419DBAF0))

    def test_the_closure_function_of_1408BED00_passes_every_allocator_site(self):
        self.assertEqual(g.check_function(self.pd, self.md, self.read, 0x1419DC480), [])
        calls = [i for i in self.pd.decode(self.md, 0x1419DC480)
                 if i.mnemonic == "call" and i.op_str.strip() == hex(g.GFX_DEVICE_ALLOC)]
        self.assertGreater(len(calls), 0, "the function must contain an allocator call for the pass to mean anything")

    def test_1408BE3C0s_closure_function_passes_with_the_paired_refcount(self):
        # 0x1419DBC0C's arm stores the packet's objects (+0x20, +0x28) and bumps their counts through 0x1403DB250: paired, so it passes.
        self.assertEqual(g.check_function(self.pd, self.md, self.read, 0x1419DBAF0), [])

    def test_1408BED00s_closure_functions_pass_with_a_paired_write_into_the_object(self):
        # 0x1419DC350: the AddRef is paired (the object is stored into the packet at +0x18), and the arm then exchanges the object's own
        # field +0x2C (xchg). Owner ruling (widen, round 4): a write into the paired object is admitted, so the sites pass.
        for va in (0x1419DC350, 0x1419DC480, 0x1419DC540, 0x1419DCCA0):
            self.assertEqual(g.check_function(self.pd, self.md, self.read, va), [], hex(va))

    def test_a_real_site_with_a_call_in_its_skipped_arm_fails_at_that_arm(self):
        head = self.pd.containing(0x14195A634)
        fails = dict(g.check_function(self.pd, self.md, self.read, head))
        self.assertIn(0x14195A634, fails)
        self.assertIn("0x14195a695", fails[0x14195A634])
        self.assertIn("call", fails[0x14195A634])


if __name__ == "__main__":
    unittest.main()
