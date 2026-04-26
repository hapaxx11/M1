#!/usr/bin/env python3
# See COPYING.txt for license details.
"""
gen_keeloq_mfkeys_builtin.py — Embed KeeLoq manufacturer keys into the M1 firmware.

Reads a plaintext key file (RocketGod SubGHz Toolkit format or compact
HEX:TYPE:NAME format) and generates Sub_Ghz/subghz_keeloq_mfkeys_builtin.c
containing the keys as a string constant compiled directly into the firmware
binary.

This is the Flipper-compatible approach: manufacturer keys are baked into the
firmware at build time, never stored on removable media, and never appear in
the public repository.

When no vault file is provided (or the file is absent), generates a stub with
NULL/zero data — keeloq_mfkeys_load() then falls back to the SD card keystore.

The output file replaces Sub_Ghz/subghz_keeloq_mfkeys_builtin.c in-place.
The version committed to the public repository contains the NULL stub.
DO NOT commit the generated version containing real manufacturer keys.

Usage
-----
::

    # With a private key vault — embeds keys into firmware:
    python3 scripts/gen_keeloq_mfkeys_builtin.py \\
        keeloq_keys.txt \\
        Sub_Ghz/subghz_keeloq_mfkeys_builtin.c

    # Regenerate the NULL stub (public/CI without vault):
    python3 scripts/gen_keeloq_mfkeys_builtin.py \\
        Sub_Ghz/subghz_keeloq_mfkeys_builtin.c

Input formats accepted
----------------------
1. **RocketGod SubGHz Toolkit** (``keeloq_keys.txt``)::

       Manufacturer: BFT
       Key (Hex):    0123456789ABCDEF
       Key (Dec):    81985529216486895
       Type:         1
       ------------------------------------

2. **Compact format** (one entry per line, '#' comments ignored)::

       0123456789ABCDEF:1:BFT

Both formats may coexist in the same file.
"""

import argparse
import os
import sys


# ---------------------------------------------------------------------------
# Key file parser (supports RocketGod and compact formats)
# ---------------------------------------------------------------------------

def _parse_compact_line(line):
    """Parse a compact ``HEX:TYPE:NAME`` line.

    Returns ``(key_int, type_int, name_str)`` or ``None`` if invalid.
    """
    line = line.strip()
    if not line or line.startswith('#'):
        return None
    # Skip Flipper keystore headers
    if line.startswith(('Filetype:', 'Version:', 'Encryption:')):
        return None
    parts = line.split(':', 2)
    if len(parts) != 3:
        return None
    hex_str, type_str, name = parts[0].strip(), parts[1].strip(), parts[2].strip()
    if not name:
        return None
    try:
        k = int(hex_str, 16)
        t = int(type_str)
    except ValueError:
        return None
    if not (1 <= t <= 3 and 0 <= k <= 0xFFFF_FFFF_FFFF_FFFF):
        return None
    return k, t, name


def parse_keystore_lines(lines):
    """Parse *lines* containing RocketGod format, compact format, or both.

    Returns a list of ``(key_int, type_int, name_str)`` tuples.
    """
    entries = []
    # RocketGod parser state
    manufacturer = None
    key_hex = None
    key_type = None

    def _rg_try_emit():
        if not (manufacturer and key_hex and key_type is not None):
            return
        try:
            k = int(key_hex, 16)
            t = int(key_type)
        except ValueError:
            return
        if 1 <= t <= 3 and 0 <= k <= 0xFFFF_FFFF_FFFF_FFFF:
            entries.append((k, t, manufacturer))

    for raw in lines:
        stripped = raw.rstrip('\r\n').strip()

        if stripped.startswith('Manufacturer:'):
            _rg_try_emit()
            manufacturer = stripped[len('Manufacturer:'):].strip()
            key_hex = None
            key_type = None
            continue

        if stripped.startswith('Key (Hex):'):
            rest = stripped[len('Key (Hex):'):].strip()
            # Also handle "Key (Hex):   VALUE" with extra spaces
            if rest.startswith(')'):
                rest = rest[1:].lstrip(': ')
            key_hex = rest if rest else None
            continue

        if stripped.startswith('Key (Dec):'):
            continue  # ignored

        if stripped.startswith('Type:') and manufacturer is not None:
            key_type = stripped[len('Type:'):].strip()
            continue

        if stripped.startswith('---') and manufacturer is not None:
            _rg_try_emit()
            manufacturer = None
            key_hex = None
            key_type = None
            continue

        # Compact format (only when not inside a RocketGod block)
        if manufacturer is None and stripped and not stripped.startswith('#'):
            # Skip Flipper keystore header lines
            if stripped.startswith(('Filetype:', 'Version:', 'Encryption:')):
                continue
            result = _parse_compact_line(stripped)
            if result is not None:
                entries.append(result)

    # Flush trailing RocketGod entry not terminated by a separator
    _rg_try_emit()

    return entries


def entries_to_compact_text(entries):
    """Serialise *(key, type, name)* tuples to compact text (one per line)."""
    lines = [f"{k:016X}:{t}:{name}" for k, t, name in entries]
    return '\n'.join(lines) + ('\n' if lines else '')


# ---------------------------------------------------------------------------
# C source generators
# ---------------------------------------------------------------------------

_STUB_TEMPLATE = """\
/* See COPYING.txt for license details. */

/**
 * @file  subghz_keeloq_mfkeys_builtin.c
 * @brief KeeLoq manufacturer keys embedded at build time.
 *
 * This stub version contains no keys and is the version committed to the
 * public repository.  At build time, scripts/gen_keeloq_mfkeys_builtin.py
 * regenerates this file from a private key vault when the
 * KEELOQ_KEY_VAULT environment variable (or GitHub Actions secret) is set.
 *
 * When this file contains real keys:
 *   - keeloq_mfkeys_load() uses them directly from firmware flash.
 *   - No SD card file is consulted (Flipper-compatible behaviour).
 *   - The keys are never visible as a file on the SD card.
 *
 * When this file contains the stub (len == 0):
 *   - keeloq_mfkeys_load() falls back to the SD card keystore.
 *   - This is the behaviour for public/CI builds without a vault.
 *
 * DO NOT commit a version of this file that contains real manufacturer keys.
 * The generator script overwrites this file in-place during private builds;
 * only the NULL stub version ever appears in the public repository.
 */

#include <stddef.h>
#include "subghz_keeloq_mfkeys.h"

/* No keys embedded — keeloq_mfkeys_load() will fall back to the SD card. */
const char * const keeloq_mfkeys_builtin_text = NULL;
const uint32_t     keeloq_mfkeys_builtin_len  = 0;
"""


def _escape_c_string_line(line):
    """Return *line* as a C string literal fragment (with trailing \\n)."""
    line = line.replace('\\', '\\\\').replace('"', '\\"')
    return f'    "{line}\\n"'


def generate_populated_source(entries, vault_path):
    """Generate a C source file embedding *entries* as a string constant."""
    noun = "entry" if len(entries) == 1 else "entries"
    compact_lines = [f"{k:016X}:{t}:{name}" for k, t, name in entries]

    c_string_lines = [_escape_c_string_line(line) for line in compact_lines]
    c_string_body = '\n'.join(c_string_lines)

    vault_basename = os.path.basename(vault_path)

    return f"""\
/* See COPYING.txt for license details. */

/**
 * @file  subghz_keeloq_mfkeys_builtin.c
 * @brief KeeLoq manufacturer keys embedded at build time — {len(entries)} {noun}.
 *
 * AUTO-GENERATED by scripts/gen_keeloq_mfkeys_builtin.py
 * Source: {vault_basename}
 *
 * DO NOT COMMIT this file with real manufacturer keys to the public
 * repository.  The committed version of this file is a NULL stub.
 * This generated version is used only during private/CI builds.
 */

#include <stddef.h>
#include "subghz_keeloq_mfkeys.h"

static const char s_keeloq_builtin_text[] =
{c_string_body}
    ;

const char * const keeloq_mfkeys_builtin_text = s_keeloq_builtin_text;
const uint32_t     keeloq_mfkeys_builtin_len  =
    (uint32_t)(sizeof(s_keeloq_builtin_text) - 1u);
"""


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main(argv=None):
    parser = argparse.ArgumentParser(
        description=(
            "Embed KeeLoq manufacturer keys into M1 firmware at build time.\n\n"
            "With a vault file: generates a populated C source with keys baked\n"
            "  into the firmware binary (Flipper-compatible approach).\n"
            "Without a vault file: generates a NULL stub — keeloq_mfkeys_load()\n"
            "  falls back to the SD card keystore.\n\n"
            "Output always replaces Sub_Ghz/subghz_keeloq_mfkeys_builtin.c.\n"
            "DO NOT commit the populated version to the public repository."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "vault",
        nargs="?",
        default=None,
        help=(
            "Path to the key vault file (RocketGod or compact HEX:TYPE:NAME format). "
            "Omit to generate a NULL stub."
        ),
    )
    parser.add_argument(
        "output",
        nargs="?",
        default=os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            "Sub_Ghz",
            "subghz_keeloq_mfkeys_builtin.c",
        ),
        help=(
            "Output C source file path "
            "(default: Sub_Ghz/subghz_keeloq_mfkeys_builtin.c relative to repo root)"
        ),
    )
    args = parser.parse_args(argv)

    # Determine what to generate
    if args.vault is None or not os.path.isfile(args.vault):
        if args.vault is not None:
            print(
                f"Warning: vault file '{args.vault}' not found — generating NULL stub.",
                file=sys.stderr,
            )
        source = _STUB_TEMPLATE
        noun = "stub (no keys embedded)"
    else:
        try:
            with open(args.vault, encoding="utf-8", errors="replace") as fh:
                lines = fh.readlines()
        except OSError as exc:
            print(f"Error reading vault '{args.vault}': {exc}", file=sys.stderr)
            sys.exit(1)

        entries = parse_keystore_lines(lines)

        if not entries:
            print(
                "Warning: no valid KeeLoq entries found in the vault file — "
                "generating NULL stub.\n"
                "Ensure the file is in RocketGod SubGHz Toolkit format or "
                "compact HEX:TYPE:NAME format.",
                file=sys.stderr,
            )
            source = _STUB_TEMPLATE
            noun = "stub (0 entries parsed from vault)"
        else:
            source = generate_populated_source(entries, args.vault)
            count = len(entries)
            noun = f"{count} {'entry' if count == 1 else 'entries'} embedded"

    # Write output
    try:
        with open(args.output, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(source)
    except OSError as exc:
        print(f"Error writing '{args.output}': {exc}", file=sys.stderr)
        sys.exit(1)

    print(f"Generated '{args.output}' — {noun}.", file=sys.stderr)


if __name__ == "__main__":
    main()
