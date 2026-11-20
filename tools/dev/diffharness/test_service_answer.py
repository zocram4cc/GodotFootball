#!/usr/bin/env python3
"""The proof harness's answer to a GF service call (run.py install_hooks, branch service_).

A call to a stubs.json gf-service entry with a declared value returns that value and pops its return, so a replay that reaches
the graphics device allocator (0x141957C50, answer 0: the same answer pes_service_gfx_device_alloc returns on the GF side) runs on.
A service with no declared value stops the replay, loudly. The entry comes from the real table (resolve_stubs), so the test
follows the declared answer and does not pin a constant of its own.
Usage: GF_DIFF_PYTHON=<venv python> python3 test_service_answer.py
"""
import copy
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import run as rn  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import UC_X86_REG_RBX, UC_X86_REG_RSP  # noqa: E402

SERVICE_VA = 0x141957C50  # stubs.json gf-service, gfx_device_alloc
CALLER, STACK = 0x141950000, 0x30000  # next to the service entry: a rel32 call reaches it
MARKER = 0x99  # eax before the call: a service that answered nothing leaves it in rax, and rbx shows it
# mov eax, MARKER (5); call SERVICE_VA (5); mov rbx, rax (3). The return sentinel is the instruction after the last one.
CALL_LEN = 5 + 5 + 3
RET_SENTINEL = CALLER + CALL_LEN


def service_stub(stubs, va=SERVICE_VA):
    return next(s for s in stubs.values() if int(s["va"], 16) == va)


def run_caller(stub):
    """Run the caller with the stub installed at the service entry. -> (rbx after the call, the harness's refusal or None)."""
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(SERVICE_VA & ~0xFFF, 0x1000)
    mu.mem_write(SERVICE_VA, b"\xC3")  # the PES bytes at the entry: never run, the hook answers and pops the return
    mu.mem_map(CALLER, 0x1000)
    code = b"\xB8" + struct.pack("<I", MARKER)
    code += b"\xE8" + struct.pack("<i", SERVICE_VA - (CALLER + 10))
    code += b"\x48\x89\xC3"  # mov rbx, rax
    mu.mem_write(CALLER, code)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.reg_write(UC_X86_REG_RSP, rsp)
    rn.install_hooks(mu, {"stubs": {"service": stub}}, RET_SENTINEL)
    mu.emu_start(CALLER, RET_SENTINEL, count=100)
    return mu.reg_read(UC_X86_REG_RBX), mu._bad


class ServiceAnswer(unittest.TestCase):
    def setUp(self):
        self.stubs = rn.resolve_stubs({"stubs": {}})["stubs"]

    def test_the_declared_answer_is_returned_and_the_call_returns(self):
        rbx, bad = run_caller(service_stub(self.stubs))
        self.assertIsNone(bad)
        self.assertEqual(rbx, int(service_stub(self.stubs)["value"], 16))

    def test_the_answer_is_the_declared_value_not_a_constant(self):
        stub = copy.deepcopy(service_stub(self.stubs))
        stub["value"] = "0x5"
        rbx, bad = run_caller(stub)
        self.assertIsNone(bad)
        self.assertEqual(rbx, 0x5)

    def test_a_service_with_no_declared_answer_stops_the_replay(self):
        stub = copy.deepcopy(service_stub(self.stubs))
        del stub["value"]
        rbx, bad = run_caller(stub)
        self.assertIsNotNone(bad)
        self.assertIn("no declared answer", bad)


if __name__ == "__main__":
    unittest.main()
