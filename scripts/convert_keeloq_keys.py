#!/usr/bin/env python3
# See COPYING.txt for license details.
"""
convert_keeloq_keys.py — Convert RocketGod SubGHz Toolkit output to M1 format.

.. note::

    **This script is kept for backward compatibility.**  The recommended
    workflow is now to use ``encrypt_keeloq_keys.py`` instead, which
    produces an encrypted ``keeloq_mfcodes.enc`` file in one step::

        python3 scripts/encrypt_keeloq_keys.py keeloq_keys.txt keeloq_mfcodes.enc

    The plaintext file produced by this script can still be placed on the
    SD card — the firmware accepts it as a fallback and will automatically
    migrate it to the encrypted format on the next boot.

RocketGod's SubGHz Toolkit (https://github.com/RocketGod-git/RocketGods-SubGHz-Toolkit)
decrypts the Flipper Zero KeeLoq manufacturer keystore and exports it as a human-readable
text file (``keeloq_keys.txt``).  This script converts that output to the plain-text
format read by the M1 firmware from ``0:/SUBGHZ/keeloq_mfcodes`` on the SD card.

Input format (RocketGod toolkit ``keeloq_keys.txt``)::

    Manufacturer: BFT
    Key (Hex):    0123456789ABCDEF
    Key (Dec):    81985529216486895
    Type:         1
    ------------------------------------

Output format (M1 ``keeloq_mfcodes``)::

    0123456789ABCDEF:1:BFT

The output file should be copied to ``SUBGHZ/keeloq_mfcodes`` on the M1's SD card.
Once present, KeeLoq, Jarolift, and Star Line ``.sub`` files that include a
``Manufacture:`` field will be replayed automatically using counter-mode re-encryption.

Usage::

    python3 scripts/convert_keeloq_keys.py keeloq_keys.txt keeloq_mfcodes
    python3 scripts/convert_keeloq_keys.py keeloq_keys.txt   # prints to stdout
"""

import argparse
import sys


def _parse_key_hex(line):
    """Return the hex value from a ``Key (Hex):  VALUE`` line, or None."""
    # "Key (Hex)" is 9 chars; the rest starts with "):  " or ":  "
    rest = line[len("Key (Hex)"):].lstrip("):").strip()
    return rest if rest else None


def convert_lines(lines):
    """Parse RocketGod ``keeloq_keys.txt`` lines into M1-format entries.

    Args:
        lines: Iterable of text lines (with or without line endings).

    Returns:
        List of ``AABBCCDDEEFFAABB:TYPE:NAME`` strings, one per valid entry.
        Entries with an invalid hex key, an out-of-range type (1–3), or a
        missing field are silently skipped.
    """
    output = []
    manufacturer = None
    key_hex = None
    key_type = None

    def _try_emit():
        """Append a formatted entry if all three fields are valid."""
        if not (manufacturer and key_hex and key_type is not None):
            return
        try:
            k = int(key_hex, 16)
            t = int(key_type)
        except ValueError:
            return
        if 1 <= t <= 3 and 0 <= k <= 0xFFFFFFFFFFFFFFFF:
            output.append(f"{k:016X}:{t}:{manufacturer}")

    for raw in lines:
        line = raw.rstrip("\r\n").strip()

        if line.startswith("Manufacturer:"):
            # New entry starts — reset pending state.
            manufacturer = line[len("Manufacturer:"):].strip()
            key_hex = None
            key_type = None

        elif line.startswith("Key (Hex):"):
            key_hex = _parse_key_hex(line)

        elif line.startswith("Type:"):
            key_type = line[len("Type:"):].strip()

        elif line.startswith("---"):
            # End-of-entry separator — emit if complete, then reset.
            _try_emit()
            manufacturer = None
            key_hex = None
            key_type = None

    # Emit any final entry not terminated by a separator line.
    _try_emit()

    return output


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=(
            "Convert RocketGod SubGHz Toolkit keeloq_keys.txt to M1 keeloq_mfcodes.\n\n"
            "Run the RocketGod toolkit on your Flipper Zero (Decrypt KeeLoq\n"
            "Manufacturer Codes) to export keeloq_keys.txt, then run this script.\n"
            "Copy the output file to SUBGHZ/keeloq_mfcodes on the M1's SD card."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "input",
        help="RocketGod toolkit output file (keeloq_keys.txt)",
    )
    parser.add_argument(
        "output",
        nargs="?",
        help="Output file path (default: print to stdout)",
    )
    args = parser.parse_args(argv)

    try:
        with open(args.input, encoding="utf-8", errors="replace") as fh:
            lines = fh.readlines()
    except OSError as exc:
        print(f"Error reading '{args.input}': {exc}", file=sys.stderr)
        sys.exit(1)

    entries = convert_lines(lines)

    if not entries:
        print(
            "Warning: no valid KeeLoq entries found.\n"
            "Make sure the input was produced by RocketGod's SubGHz Toolkit "
            "(Decrypt KeeLoq Manufacturer Codes → keeloq_keys.txt).",
            file=sys.stderr,
        )

    text = "\n".join(entries) + ("\n" if entries else "")

    if args.output:
        try:
            with open(args.output, "w", encoding="utf-8") as fh:
                fh.write(text)
            noun = "entry" if len(entries) == 1 else "entries"
            print(f"Wrote {len(entries)} {noun} to '{args.output}'.", file=sys.stderr)
        except OSError as exc:
            print(f"Error writing '{args.output}': {exc}", file=sys.stderr)
            sys.exit(1)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
