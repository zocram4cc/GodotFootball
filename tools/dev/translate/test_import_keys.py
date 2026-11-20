#!/usr/bin/env python3
"""xlate.ImportKeys: a call through an import slot the table does not name resolves to the named export whose key (the IAT
qword) the slot holds, so the .impdata copy of the import address table routes to the same models as .rdata. Synthetic
tables and keys (no install values are pinned). Plain python3. Run: python3 tools/dev/translate/test_import_keys.py"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xlate  # noqa: E402

NAMED = 0x14000F000            # a named slot (.rdata copy) for TlsAlloc, key KEY
NAMED_OTHER = 0x14000F008      # a named slot for SetLastError, key SHARED
NAMED_TWIN = 0x14000F010       # a named slot for VirtualFree, key SHARED too (two named slots share SHARED)
COPY = 0x15B000E60             # the copy: no table entry, the key NAMED holds
COPY_UNKNOWN = 0x15B000E68     # no table entry, a key no named slot holds
COPY_SHARED = 0x15B000E70      # no table entry, the key two named slots share
KEY = 0x6FFF00AA0001
SHARED = 0x6FFF00AA0002
UNKNOWN = 0x6FFF00AA0003
MODEL = {("kernel32.dll", "TlsAlloc"): {"do": "return", "value": 0, "code_hex": "31c0c3"}}


def qwords(d):
    """A reader over {va: qword}; an unmapped VA reads as zero."""
    def read(va, n):
        return d.get(va, 0).to_bytes(8, "little")[:n]
    return read


class ImportKeysTest(unittest.TestCase):
    def setUp(self):
        self.saved = xlate._IMPORT_TABLES
        slots = {
            "0x%X" % NAMED: {"kind": "export", "module": "kernel32.dll", "export": "TlsAlloc"},
            "0x%X" % NAMED_OTHER: {"kind": "export", "module": "kernel32.dll", "export": "SetLastError"},
            "0x%X" % NAMED_TWIN: {"kind": "export", "module": "kernel32.dll", "export": "VirtualFree"},
        }
        xlate._IMPORT_TABLES = (slots, MODEL)
        self.image = {NAMED: KEY, NAMED_OTHER: SHARED, NAMED_TWIN: SHARED,
                      COPY: KEY, COPY_UNKNOWN: UNKNOWN, COPY_SHARED: SHARED}

    def tearDown(self):
        xlate._IMPORT_TABLES = self.saved

    def test_a_named_slot_is_its_own_entry(self):
        keys = xlate.ImportKeys(qwords(self.image))
        self.assertEqual(keys.entry(NAMED)["export"], "TlsAlloc")

    def test_a_copy_slot_with_a_named_key_resolves_to_that_export(self):
        keys = xlate.ImportKeys(qwords(self.image))
        self.assertEqual(keys.entry(COPY)["export"], "TlsAlloc")

    def test_a_copy_slot_with_an_unknown_key_names_nothing(self):
        keys = xlate.ImportKeys(qwords(self.image))
        self.assertIsNone(keys.entry(COPY_UNKNOWN))

    def test_a_key_two_named_slots_share_names_nothing(self):
        keys = xlate.ImportKeys(qwords(self.image))
        self.assertIsNone(keys.entry(COPY_SHARED))

    def test_a_call_through_a_copy_slot_runs_the_export_model(self):
        keys = xlate.ImportKeys(qwords(self.image))
        lines = xlate.import_model_lines("qword ptr [0x%x]" % COPY, jump=False, keys=keys)
        self.assertIsNotNone(lines)
        self.assertIn("s.gpr[0] = 0ULL;", lines[0])

    def test_without_keys_a_copy_slot_stays_on_the_dispatch_path(self):
        self.assertIsNone(xlate.import_model_lines("qword ptr [0x%x]" % COPY, jump=False))


if __name__ == "__main__":
    unittest.main()
