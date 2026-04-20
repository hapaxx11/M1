#!/usr/bin/env python3
# See COPYING.txt for license details.
"""Tests for scripts/convert_keeloq_keys.py."""

import os
import sys
import tempfile
import textwrap
import unittest

# Add scripts/ to path so we can import the module directly.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))
import convert_keeloq_keys as ck


class TestParseKeyHex(unittest.TestCase):
    """Tests for the _parse_key_hex() helper."""

    def test_standard_spacing(self):
        self.assertEqual(
            ck._parse_key_hex("Key (Hex):    0123456789ABCDEF"),
            "0123456789ABCDEF",
        )

    def test_single_space(self):
        self.assertEqual(
            ck._parse_key_hex("Key (Hex): 0123456789ABCDEF"),
            "0123456789ABCDEF",
        )

    def test_no_space(self):
        self.assertEqual(
            ck._parse_key_hex("Key (Hex):0123456789ABCDEF"),
            "0123456789ABCDEF",
        )

    def test_empty_value(self):
        result = ck._parse_key_hex("Key (Hex):   ")
        # Empty string or None — should be falsy
        self.assertFalse(result)


class TestConvertLines(unittest.TestCase):
    """Tests for convert_lines() — the core parsing function."""

    # ------------------------------------------------------------------
    # Helper
    # ------------------------------------------------------------------

    @staticmethod
    def _make_entry(name, key_hex, key_type, dec=None):
        """Build a single RocketGod-format entry as a list of lines.

        ``key_hex`` must be a valid 16-digit uppercase hex string.
        ``dec`` defaults to 0 (the M1 parser ignores the Dec field anyway).
        """
        dec_val = dec if dec is not None else 0
        return [
            f"Manufacturer: {name}\n",
            f"Key (Hex):    {key_hex}\n",
            f"Key (Dec):    {dec_val}\n",
            f"Type:         {key_type}\n",
            "------------------------------------\n",
            "\n",
        ]

    @staticmethod
    def _header():
        return [
            "====================================\n",
            "  Flipper SubGhz KeeLoq Mfcodes\n",
            "  Decrypted by SubGhz Toolkit\n",
            "  RocketGod | betaskynet.com\n",
            "====================================\n",
            "\n",
            "Total Keys: 1\n",
            "\n",
        ]

    # ------------------------------------------------------------------
    # Single entry
    # ------------------------------------------------------------------

    def test_single_entry_type1(self):
        lines = self._make_entry("BFT", "0123456789ABCDEF", 1)
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["0123456789ABCDEF:1:BFT"])

    def test_single_entry_type2(self):
        lines = self._make_entry("CAME", "FEDCBA9876543210", 2)
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["FEDCBA9876543210:2:CAME"])

    def test_single_entry_type3(self):
        lines = self._make_entry("Nice", "AABBCCDDEEFF0011", 3)
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["AABBCCDDEEFF0011:3:Nice"])

    def test_single_entry_with_header(self):
        lines = self._header() + self._make_entry("BFT", "0123456789ABCDEF", 1)
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["0123456789ABCDEF:1:BFT"])

    # ------------------------------------------------------------------
    # Multiple entries
    # ------------------------------------------------------------------

    def test_multiple_entries(self):
        lines = (
            self._make_entry("BFT",  "0123456789ABCDEF", 1) +
            self._make_entry("CAME", "FEDCBA9876543210", 2) +
            self._make_entry("Nice", "AABBCCDDEEFF0011", 3)
        )
        result = ck.convert_lines(lines)
        self.assertEqual(len(result), 3)
        self.assertIn("0123456789ABCDEF:1:BFT", result)
        self.assertIn("FEDCBA9876543210:2:CAME", result)
        self.assertIn("AABBCCDDEEFF0011:3:Nice", result)

    # ------------------------------------------------------------------
    # Key formatting
    # ------------------------------------------------------------------

    def test_key_uppercased_in_output(self):
        lines = self._make_entry("Test", "0123456789abcdef", 1)
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["0123456789ABCDEF:1:Test"])

    def test_key_zero_padded(self):
        lines = self._make_entry("ZeroKey", "0000000000000001", 2)
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["0000000000000001:2:ZeroKey"])

    def test_all_zeros_key(self):
        lines = self._make_entry("Zero", "0000000000000000", 1)
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["0000000000000000:1:Zero"])

    def test_all_ff_key(self):
        lines = self._make_entry("Max", "FFFFFFFFFFFFFFFF", 2)
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["FFFFFFFFFFFFFFFF:2:Max"])

    # ------------------------------------------------------------------
    # Error / edge cases
    # ------------------------------------------------------------------

    def test_empty_input(self):
        result = ck.convert_lines([])
        self.assertEqual(result, [])

    def test_header_only_no_entries(self):
        result = ck.convert_lines(self._header())
        self.assertEqual(result, [])

    def test_invalid_type_0_skipped(self):
        lines = self._make_entry("Bad", "0123456789ABCDEF", 0)
        result = ck.convert_lines(lines)
        self.assertEqual(result, [])

    def test_invalid_type_4_skipped(self):
        lines = self._make_entry("Bad", "0123456789ABCDEF", 4)
        result = ck.convert_lines(lines)
        self.assertEqual(result, [])

    def test_invalid_hex_key_skipped(self):
        lines = [
            "Manufacturer: Bad\n",
            "Key (Hex):    ZZZZZZZZZZZZZZZZ\n",
            "Key (Dec):    0\n",
            "Type:         1\n",
            "------------------------------------\n",
        ]
        result = ck.convert_lines(lines)
        self.assertEqual(result, [])

    def test_missing_key_skipped(self):
        lines = [
            "Manufacturer: NoKey\n",
            "Key (Dec):    0\n",
            "Type:         1\n",
            "------------------------------------\n",
        ]
        result = ck.convert_lines(lines)
        self.assertEqual(result, [])

    def test_missing_type_skipped(self):
        lines = [
            "Manufacturer: NoType\n",
            "Key (Hex):    0123456789ABCDEF\n",
            "Key (Dec):    0\n",
            "------------------------------------\n",
        ]
        result = ck.convert_lines(lines)
        self.assertEqual(result, [])

    def test_missing_manufacturer_skipped(self):
        lines = [
            "Key (Hex):    0123456789ABCDEF\n",
            "Key (Dec):    0\n",
            "Type:         1\n",
            "------------------------------------\n",
        ]
        result = ck.convert_lines(lines)
        self.assertEqual(result, [])

    def test_entry_not_terminated_by_separator(self):
        """Last entry in file without trailing '---' line is still emitted."""
        lines = [
            "Manufacturer: Last\n",
            "Key (Hex):    AABBCCDDEEFF0011\n",
            "Key (Dec):    0\n",
            "Type:         2\n",
            # No "---" separator at end
        ]
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["AABBCCDDEEFF0011:2:Last"])

    def test_comment_and_blank_lines_ignored(self):
        """Lines that are neither field headers nor separators are ignored."""
        lines = [
            "\n",
            "Total Keys: 1\n",
            "\n",
            "Manufacturer: BFT\n",
            "Key (Hex):    0123456789ABCDEF\n",
            "Key (Dec):    81985529216486895\n",
            "Type:         1\n",
            "------------------------------------\n",
            "\n",
        ]
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["0123456789ABCDEF:1:BFT"])

    # ------------------------------------------------------------------
    # Manufacturer name edge cases
    # ------------------------------------------------------------------

    def test_manufacturer_name_with_spaces(self):
        lines = self._make_entry("Scher-Khan Magicar", "0123456789ABCDEF", 1)
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["0123456789ABCDEF:1:Scher-Khan Magicar"])

    def test_manufacturer_name_with_numbers(self):
        lines = self._make_entry("King Gates Stylo 4k", "0123456789ABCDEF", 2)
        result = ck.convert_lines(lines)
        self.assertEqual(result, ["0123456789ABCDEF:2:King Gates Stylo 4k"])


class TestMainFunction(unittest.TestCase):
    """Integration tests for the main() entry point."""

    @staticmethod
    def _toolkit_output():
        return textwrap.dedent("""\
            ====================================
              Flipper SubGhz KeeLoq Mfcodes
              Decrypted by SubGhz Toolkit
              RocketGod | betaskynet.com
            ====================================

            Total Keys: 2

            Manufacturer: BFT
            Key (Hex):    0123456789ABCDEF
            Key (Dec):    81985529216486895
            Type:         1
            ------------------------------------

            Manufacturer: CAME
            Key (Hex):    FEDCBA9876543210
            Key (Dec):    18364758544493064720
            Type:         2
            ------------------------------------

        """)

    def test_stdout_output(self):
        """main() writes M1-format lines to stdout when no output file given."""
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".txt", delete=False
        ) as fh:
            fh.write(self._toolkit_output())
            input_path = fh.name

        try:
            import io
            from unittest.mock import patch

            captured = io.StringIO()
            with patch("sys.stdout", captured):
                ck.main([input_path])

            output = captured.getvalue()
            lines = [l for l in output.splitlines() if l]
            self.assertEqual(len(lines), 2)
            self.assertIn("0123456789ABCDEF:1:BFT", lines)
            self.assertIn("FEDCBA9876543210:2:CAME", lines)
        finally:
            os.unlink(input_path)

    def test_file_output(self):
        """main() writes M1-format lines to the specified output file."""
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".txt", delete=False
        ) as fh:
            fh.write(self._toolkit_output())
            input_path = fh.name

        with tempfile.NamedTemporaryFile(
            mode="w", suffix="_out.txt", delete=False
        ) as fh:
            output_path = fh.name

        try:
            ck.main([input_path, output_path])

            with open(output_path, encoding="utf-8") as fh:
                content = fh.read()

            lines = [l for l in content.splitlines() if l]
            self.assertEqual(len(lines), 2)
            self.assertIn("0123456789ABCDEF:1:BFT", lines)
            self.assertIn("FEDCBA9876543210:2:CAME", lines)
        finally:
            os.unlink(input_path)
            os.unlink(output_path)

    def test_missing_input_exits(self):
        """main() exits with code 1 when the input file does not exist."""
        with self.assertRaises(SystemExit) as ctx:
            ck.main(["/nonexistent/path/keeloq_keys.txt"])
        self.assertEqual(ctx.exception.code, 1)


if __name__ == "__main__":
    unittest.main()
