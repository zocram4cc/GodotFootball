#!/usr/bin/env python3
"""The KERNEL32!CreateSemaphoreW import model (import_stubs.json) against the contract its caller relies on.

PES's file-stream constructor 0x141B38980 calls CreateSemaphoreW(0, count, max, 0) through IAT slot 0x14252F1B8 and stores the
result only when it is non-zero (`if (h != 0) *param_1 = h;`). A model that returns 0 leaves the stream with no handle, so the
model must return a non-zero handle. Loads the model by its (module, export) key, as frame_trace.LazyMap does, and runs its bytes
in unicorn. Usage: GF_DIFF_PYTHON=<venv python> python3 test_create_semaphore_model.py
"""
import json
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import frame_trace as ft  # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # noqa: E402
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_RSP  # noqa: E402

CODE, STACK = 0x10000, 0x30000
RET = CODE + 0x800  # the stop address the model returns to (never executed)


def run_create_semaphore(initial, maximum):
    """-> rax after the model runs as CreateSemaphoreW(NULL, initial, maximum, NULL)."""
    model = ft.load_import_models()[("kernel32.dll", "CreateSemaphoreW")]
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(CODE, 0x1000)
    mu.mem_write(CODE, model)
    mu.mem_map(STACK, 0x1000)
    rsp = STACK + 0x800
    mu.mem_write(rsp, struct.pack("<Q", RET))
    mu.reg_write(UC_X86_REG_RCX, 0)
    mu.reg_write(UC_X86_REG_RDX, initial)
    mu.reg_write(UC_X86_REG_R8, maximum)
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.emu_start(CODE, RET, count=1000)
    return mu.reg_read(UC_X86_REG_RAX)


class CreateSemaphoreModel(unittest.TestCase):
    def test_returns_a_non_zero_handle(self):
        # The stream stores the handle only when it is non-zero: a zero here drops the handle silently.
        self.assertNotEqual(run_create_semaphore(0, 0x7FFFFFFF), 0)

    def test_the_handle_does_not_depend_on_the_count(self):
        # The model stands in for a handle; the count is the stream's own choice, not something the handle encodes.
        self.assertEqual(run_create_semaphore(0, 1), run_create_semaphore(1, 0x7FFFFFFF))

    def test_the_harness_code_returns_the_translators_value(self):
        # The translator mirrors the entry's `value` (xlate.import_model_lines, do = return); the harness runs its code_hex.
        entry = next(e for e in json.load(open(os.path.join(HERE, "import_stubs.json")))["imports"]
                     if e["export"] == "CreateSemaphoreW")
        self.assertEqual(run_create_semaphore(0, 1), int(entry["value"], 0))


if __name__ == "__main__":
    unittest.main()
