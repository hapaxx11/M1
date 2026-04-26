#!/usr/bin/env python3
# See COPYING.txt for license details.
"""
encrypt_keeloq_keys.py — Convert and encrypt KeeLoq manufacturer keys for the M1.

Creates an AES-256-CBC encrypted keystore file (``keeloq_mfcodes.enc``) that
can be copied directly to ``SUBGHZ/keeloq_mfcodes.enc`` on the M1's SD card.
The firmware decrypts the file automatically on load using the same fixed
obfuscation key — the plaintext never needs to touch the SD card.

**This is the recommended workflow.**  Use ``convert_keeloq_keys.py`` only if
you specifically need a plain-text intermediate file.

If a legacy plaintext ``keeloq_mfcodes`` file is already present on the SD
card, the firmware will automatically migrate it to the encrypted format on
the next boot and delete the plaintext.

Input formats accepted
----------------------
1. **RocketGod SubGHz Toolkit** (``keeloq_keys.txt``), as exported by
   RocketGod's SubGHz Toolkit for Flipper Zero (Decrypt KeeLoq Manufacturer
   Codes)::

       Manufacturer: BFT
       Key (Hex):    0123456789ABCDEF
       Key (Dec):    81985529216486895
       Type:         1
       ------------------------------------

2. **M1 compact format** (one entry per line, '#' comments ignored)::

       0123456789ABCDEF:1:BFT

Both formats may coexist in the same file.

Requirements
------------
``pip install cryptography``

Usage
-----
::

    python3 scripts/encrypt_keeloq_keys.py keeloq_keys.txt keeloq_mfcodes.enc
    python3 scripts/encrypt_keeloq_keys.py keeloq_keys.txt   # prints stats only
"""

import argparse
import os
import struct
import sys

# ---------------------------------------------------------------------------
# Product key — must match s_keeloq_product_key[] in
# Sub_Ghz/subghz_keeloq_mfkeys.c.  This key is the same on every M1 unit;
# it provides file-at-rest obfuscation, NOT device binding.
# ---------------------------------------------------------------------------
KEELOQ_PRODUCT_KEY = bytes([
    0xA3, 0x7F, 0x2C, 0x51, 0xD8, 0x4E, 0x96, 0xBB,
    0x13, 0x05, 0xE7, 0x2A, 0x6C, 0xF4, 0x39, 0x80,
    0xD1, 0x5B, 0xC2, 0x47, 0x8E, 0x3A, 0x99, 0x0F,
    0x74, 0x1D, 0x62, 0xF5, 0xAB, 0xC8, 0x3E, 0x56,
])

# Binary file header constants (must match firmware)
FILE_MAGIC = b'\x4D\x31\x4B\x4C'   # "M1KL"
FILE_VERSION = 0x01


def _require_cryptography():
    """Import and return the AES primitives, or print help and exit."""
    try:
        from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
        from cryptography.hazmat.backends import default_backend
        from cryptography.hazmat.primitives import padding
        return Cipher, algorithms, modes, default_backend, padding
    except ImportError:
        print(
            "Error: the 'cryptography' package is required.\n"
            "Install it with:\n"
            "    pip install cryptography",
            file=sys.stderr,
        )
        sys.exit(1)


def encrypt_payload(plaintext_bytes):
    """Encrypt *plaintext_bytes* with AES-256-CBC + PKCS7 padding.

    Returns ``iv + ciphertext`` bytes — exactly the payload that the firmware
    writes after the 9-byte file header.
    """
    Cipher, algorithms, modes, default_backend, padding = _require_cryptography()

    iv = os.urandom(16)

    padder = padding.PKCS7(128).padder()
    padded = padder.update(plaintext_bytes) + padder.finalize()

    cipher = Cipher(
        algorithms.AES(KEELOQ_PRODUCT_KEY),
        modes.CBC(iv),
        backend=default_backend(),
    )
    encryptor = cipher.encryptor()
    ciphertext = encryptor.update(padded) + encryptor.finalize()

    return iv + ciphertext


def build_enc_file(encrypted_payload):
    """Wrap *encrypted_payload* in the M1KL binary file header.

    File layout::

        Bytes 0–3:  Magic "M1KL"
        Byte  4:    Version 0x01
        Bytes 5–8:  payload_len (uint32 LE)
        Bytes 9+:   encrypted_payload (IV + ciphertext)
    """
    payload_len = len(encrypted_payload)
    header = FILE_MAGIC + bytes([FILE_VERSION]) + struct.pack('<I', payload_len)
    return header + encrypted_payload


def parse_compact_line(line):
    """Parse a single compact ``HEX:TYPE:NAME`` line.

    Returns ``(key_int, type_int, name_str)`` or ``None`` if invalid.
    """
    line = line.strip()
    if not line or line.startswith('#'):
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
        line = raw.rstrip('\r\n')

        # RocketGod format detection
        stripped = line.strip()
        if stripped.startswith('Manufacturer:'):
            # Flush any pending RocketGod entry, start a new one
            _rg_try_emit()
            manufacturer = stripped[len('Manufacturer:'):].strip()
            key_hex = None
            key_type = None
            continue

        if stripped.startswith('Key (Hex):'):
            rest = stripped[len('Key (Hex)'):].lstrip('):').strip()
            key_hex = rest if rest else None
            continue

        if stripped.startswith('Type:') and manufacturer is not None:
            key_type = stripped[len('Type:'):].strip()
            continue

        if stripped.startswith('---') and manufacturer is not None:
            _rg_try_emit()
            manufacturer = None
            key_hex = None
            key_type = None
            continue

        # If we're not in a RocketGod block, try compact format
        if manufacturer is None and stripped and not stripped.startswith('#'):
            result = parse_compact_line(stripped)
            if result is not None:
                entries.append(result)

    # Flush trailing RocketGod entry not terminated by separator
    _rg_try_emit()

    return entries


def entries_to_text(entries):
    """Serialise *(key, type, name)* tuples to compact text (one per line)."""
    lines = [f"{k:016X}:{t}:{name}" for k, t, name in entries]
    return '\n'.join(lines) + ('\n' if lines else '')


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=(
            "Convert and encrypt KeeLoq manufacturer keys for the M1 (recommended).\n\n"
            "Produces keeloq_mfcodes.enc — copy it to SUBGHZ/ on the M1's SD card.\n"
            "The firmware decrypts it automatically; the plaintext never touches the card.\n\n"
            "Accepts RocketGod SubGHz Toolkit output, M1 compact format, or both."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "input",
        help="Input file: RocketGod keeloq_keys.txt or compact keeloq_mfcodes",
    )
    parser.add_argument(
        "output",
        nargs="?",
        default="keeloq_mfcodes.enc",
        help="Output .enc file path (default: keeloq_mfcodes.enc in current directory)",
    )
    args = parser.parse_args(argv)

    # Read input
    try:
        with open(args.input, encoding="utf-8", errors="replace") as fh:
            lines = fh.readlines()
    except OSError as exc:
        print(f"Error reading '{args.input}': {exc}", file=sys.stderr)
        sys.exit(1)

    entries = parse_keystore_lines(lines)

    if not entries:
        print(
            "Warning: no valid KeeLoq entries found in the input file.\n"
            "Make sure it was produced by RocketGod's SubGHz Toolkit "
            "(Decrypt KeeLoq Manufacturer Codes → keeloq_keys.txt) "
            "or contains compact HEX:TYPE:NAME lines.",
            file=sys.stderr,
        )
        sys.exit(1)

    noun = "entry" if len(entries) == 1 else "entries"
    print(f"Parsed {len(entries)} {noun}.", file=sys.stderr)

    # Serialise → encrypt → wrap
    plaintext = entries_to_text(entries).encode("ascii")
    encrypted = encrypt_payload(plaintext)
    file_bytes = build_enc_file(encrypted)

    # Write output
    try:
        with open(args.output, "wb") as fh:
            fh.write(file_bytes)
    except OSError as exc:
        print(f"Error writing '{args.output}': {exc}", file=sys.stderr)
        sys.exit(1)

    print(
        f"Wrote {len(file_bytes)} bytes → '{args.output}'.\n"
        f"Copy this file to SUBGHZ/keeloq_mfcodes.enc on the M1's SD card.",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
