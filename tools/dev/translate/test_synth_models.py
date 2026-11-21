#!/usr/bin/env python3
"""Test (ctest translate_synth_models_test): synth.py's __stdio_common_vsprintf_s model (stdio_format) formats the conversions PES's
writers use. Plain python3, no emulator: the formatter is a pure function over the argument slots.

The case that matters: DemoInfoRef's player name, "PLAYER_%s%02d" with side A and index 19 (19 % 11 = 8), which the capture holds as
UTF-16 PLAYER_A08 (FUN_1406EB250; the emulated writer must produce the same text).

Run: python3 tools/dev/translate/test_synth_models.py"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import synth  # noqa: E402


def format_with(template, args, strings):
    """stdio_format over the given 64-bit argument slots; strings maps a pointer slot to its NUL-free bytes."""
    slots = list(args)
    return synth.stdio_format(template, lambda: slots.pop(0), lambda p: strings[p])


class StdioFormatTest(unittest.TestCase):
    def test_player_name_of_index_19_is_PLAYER_A08(self):
        # side "A" (index >= 11), number 19 % 11 = 8, zero-padded to two digits
        self.assertEqual(format_with(b"PLAYER_%s%02d", [0x1000, 8], {0x1000: b"A"}), b"PLAYER_A08")

    def test_home_side_index_zero_is_PLAYER_H00(self):
        self.assertEqual(format_with(b"PLAYER_%s%02d", [0x1000, 0], {0x1000: b"H"}), b"PLAYER_H00")

    def test_zero_padding_and_plain_width(self):
        self.assertEqual(format_with(b"%02d|%3d", [7, 7], {}), b"07|  7")

    def test_negative_32_bit_value(self):
        self.assertEqual(format_with(b"%d", [0xFFFFFFFF], {}), b"-1")

    def test_literal_percent(self):
        self.assertEqual(format_with(b"100%%", [], {}), b"100%")

    def test_unsupported_conversion_is_refused(self):
        with self.assertRaises(SystemExit):
            format_with(b"%x", [1], {})


if __name__ == "__main__":
    unittest.main()
