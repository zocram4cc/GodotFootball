#!/usr/bin/env python3
"""The global stub table's native services (stubs.json "native_services") and what gen_dispatch emits for them.

The production table sends the allocator's vtable VAs to GF-native targets; the harness table never does (the harness
runs PES's bytes there), and a VA that is both translated and native is an error. Usage: python3 test_native_services.py
"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_dispatch  # noqa: E402
import stubs  # noqa: E402

HEAP_ALLOC, FWD_ALLOC = "0x1403FBC10", "0x141B34CB0"  # the live pool vtables' +0x10 targets (stubs.json why)


class NativeServices(unittest.TestCase):
    def test_table_names_known_ops_and_stays_out_of_the_harness_stub_table(self):
        nat = stubs.natives()
        self.assertIn(int(HEAP_ALLOC, 16), nat)
        self.assertIn(int(FWD_ALLOC, 16), nat)
        self.assertEqual(nat[int(HEAP_ALLOC, 16)], nat[int(FWD_ALLOC, 16)])  # both pools allocate the same way
        self.assertTrue(set(nat.values()) <= stubs.KNOWN_NATIVE_OPS)
        self.assertFalse(set(nat) & set(stubs.load()))  # the harness never sees them

    def test_production_table_routes_native_vas_and_the_harness_table_does_not(self):
        translated = ["0x1403FB410"]
        prod = gen_dispatch.text(translated, gen_dispatch.natives())
        self.assertIn("case 0x1403FBC10ULL: return {pes_native_heap_alloc};", prod)
        self.assertIn("case 0x141B34CB0ULL: return {pes_native_heap_alloc};", prod)
        self.assertIn("case 0x1403FB410ULL: return {pes_fn_1403FB410};", prod)
        harness = gen_dispatch.text(translated)
        self.assertNotIn("pes_native_", harness)
        self.assertNotIn("0x1403FBC10ULL", harness)  # the harness table keeps PES's bytes for the allocator targets

    def test_a_va_both_translated_and_native_is_refused(self):
        with self.assertRaises(SystemExit):
            gen_dispatch.text([HEAP_ALLOC], gen_dispatch.natives())


if __name__ == "__main__":
    unittest.main()
