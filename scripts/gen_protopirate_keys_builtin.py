#!/usr/bin/env python3
# See COPYING.txt for license details.
"""Embed optional ProtoPirate Kia V6 and VAG keys into firmware flash."""

import argparse
import re
import sys

KIA_KEY_TYPES = (11, 12)
KIA_RAW_SIZE = 16
VAG_AUT64_SIZE = 48
RAW_FILETYPE = "Flipper SubGhz Keystore RAW File"
KEYSTORE_FILETYPE = "Flipper SubGhz Keystore File"


def _parse_header(text, expected_filetypes):
    lines = text.splitlines()
    if not lines:
        raise ValueError("empty keystore")
    match = re.fullmatch(r"Filetype:\s*(.+)", lines[0].strip())
    if not match or match.group(1) not in expected_filetypes:
        raise ValueError("unexpected Flipper keystore file type")
    filetype = match.group(1)

    version = None
    encryption = None
    encryption_index = None
    for index, line in enumerate(lines[1:], 1):
        match = re.fullmatch(r"Version:\s*(\d+)", line.strip())
        if match:
            version = int(match.group(1))
        match = re.fullmatch(r"Encryption:\s*(\d+)", line.strip())
        if match:
            encryption = int(match.group(1))
            encryption_index = index
            break
    if version != 0 or encryption is None:
        raise ValueError("missing or unsupported keystore version/encryption")
    if encryption == 1:
        return filetype, None
    if encryption != 0:
        raise ValueError("unsupported keystore encryption")
    return filetype, lines[encryption_index + 1 :]


def _parse_raw_payload(lines, expected_sizes):
    nonempty_lines = [line.strip() for line in lines if line.strip()]
    if any(":" in line for line in nonempty_lines):
        data = bytearray()
        for line in nonempty_lines:
            match = re.fullmatch(r"([0-9a-fA-F]{4,8}):\s*(.*)", line)
            if not match:
                raise ValueError("malformed plaintext RAW keystore hexdump")
            offset = int(match.group(1), 16)
            tokens = match.group(2).split()
            if (
                offset != len(data)
                or not tokens
                or any(not re.fullmatch(r"[0-9a-fA-F]{2}", token) for token in tokens)
            ):
                raise ValueError("malformed plaintext RAW keystore hexdump")
            data.extend(bytes.fromhex("".join(tokens)))
        data = bytes(data)
    else:
        payload = "".join(nonempty_lines)
        if not payload or not re.fullmatch(r"(?:[0-9a-fA-F]{2})+", payload):
            raise ValueError("plaintext RAW keystore payload must contain only hex bytes")
        data = bytes.fromhex(payload)

    if len(data) not in expected_sizes:
        expected = " or ".join(str(size) for size in expected_sizes)
        raise ValueError(f"expected exactly {expected} plaintext key bytes")
    return data


def parse_kia_keystore(text):
    """Return the type-11/type-12 keys, or None for an encrypted RAW keystore.

    Accepts either a plaintext RAW file containing the two 8-byte keys in order,
    or a plaintext standard Flipper keystore containing Type 11 and Type 12,
    with or without its Flipper file header.
    """
    lines = text.splitlines()
    if lines and lines[0].strip().startswith("Filetype:"):
        filetype, lines = _parse_header(text, (RAW_FILETYPE, KEYSTORE_FILETYPE))
    else:
        filetype = KEYSTORE_FILETYPE
    if lines is None:
        return None

    if filetype == RAW_FILETYPE:
        data = _parse_raw_payload(lines, (KIA_RAW_SIZE,))
        return tuple(int.from_bytes(data[index : index + 8], "big") for index in (0, 8))

    keys = {}
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        fields = stripped.split(":", 2)
        if len(fields) != 3:
            raise ValueError("malformed plaintext Sub-GHz key entry")
        key_hex, type_text, name = (field.strip() for field in fields)
        try:
            key_type = int(type_text)
            key = int(key_hex, 16)
        except ValueError as exc:
            raise ValueError("malformed plaintext Sub-GHz key entry") from exc
        if (
            not name
            or not 1 <= len(key_hex) <= 16
            or not 1 <= key_type <= 0xFFFF
            or not 0 <= key <= 0xFFFFFFFFFFFFFFFF
        ):
            raise ValueError("invalid plaintext Sub-GHz key entry")
        if key_type in KIA_KEY_TYPES:
            if key_type in keys:
                raise ValueError(f"duplicate Kia key type {key_type}")
            keys[key_type] = key
    if set(keys) != set(KIA_KEY_TYPES):
        raise ValueError("plaintext Sub-GHz keystore must contain one Type 11 and Type 12 key")
    return keys[11], keys[12]


def parse_vag_keystore(text):
    """Return three packed AUT64 keys from plaintext RAW data or an offset dump."""
    lines = text.splitlines()
    if lines and lines[0].strip().startswith("Filetype:"):
        _, lines = _parse_header(text, (RAW_FILETYPE,))
    if lines is None:
        return None
    data = _parse_raw_payload(lines, (VAG_AUT64_SIZE, 64))
    if len(data) == 64 and any(data[VAG_AUT64_SIZE:]):
        raise ValueError("64-byte VAG keystore must have zero-filled trailing padding")
    return data[:VAG_AUT64_SIZE]


def _format_bytes(data):
    return ",\n    ".join(
        ", ".join(f"0x{byte:02X}" for byte in data[offset : offset + 16])
        for offset in range(0, len(data), 16)
    )


def generate_source(kia_keys=None, vag_keys=None):
    """Generate a C source file containing only the supplied key data."""
    kia_values = kia_keys or (0, 0)
    vag_values = vag_keys or bytes(VAG_AUT64_SIZE)
    return f"""\
/* See COPYING.txt for license details. */

#include "subghz_protopirate_keys_builtin.h"

#if defined(__GNUC__)
#define M1_KEY_DATA_USED __attribute__((used, section(".rodata.m1_protopirate_keys")))
#else
#define M1_KEY_DATA_USED
#endif

M1_KEY_DATA_USED const uint64_t m1_kia_v6_keys_builtin[M1_KIA_V6_KEY_COUNT] = {{
    0x{kia_values[0]:016X}ULL,
    0x{kia_values[1]:016X}ULL,
}};
M1_KEY_DATA_USED const bool m1_kia_v6_keys_builtin_available = {str(kia_keys is not None).lower()};

M1_KEY_DATA_USED const uint8_t m1_vag_aut64_keys_builtin[M1_VAG_AUT64_KEY_BYTES] = {{
    {_format_bytes(vag_values)},
}};
M1_KEY_DATA_USED const bool m1_vag_aut64_keys_builtin_available = {str(vag_keys is not None).lower()};
"""


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kia-keystore", help="plaintext Kia Flipper keystore file")
    parser.add_argument("--vag-keystore", help="plaintext VAG Flipper RAW keystore file")
    parser.add_argument("output", help="generated C source destination")
    args = parser.parse_args(argv)

    kia_keys = None
    vag_keys = None
    try:
        if args.kia_keystore:
            with open(args.kia_keystore, encoding="utf-8") as source:
                kia_keys = parse_kia_keystore(source.read())
        if args.vag_keystore:
            with open(args.vag_keystore, encoding="utf-8") as source:
                vag_keys = parse_vag_keystore(source.read())
    except (OSError, UnicodeError, ValueError) as exc:
        print(f"Invalid ProtoPirate keystore: {exc}", file=sys.stderr)
        return 1

    with open(args.output, "w", encoding="utf-8") as output:
        output.write(generate_source(kia_keys, vag_keys))
    if args.kia_keystore and kia_keys is None:
        print("Kia keystore is encrypted; no Kia keys embedded.")
    if args.vag_keystore and vag_keys is None:
        print("VAG keystore is encrypted; no VAG keys embedded.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
