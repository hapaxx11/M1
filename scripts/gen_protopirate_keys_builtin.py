#!/usr/bin/env python3
# See COPYING.txt for license details.
"""Embed optional ProtoPirate Kia and VAG keys into firmware flash.

Kia keystore types follow the upstream ProtoPirate numbering:
  10 = KIA_KEY1 (Kia V3/V4 manufacturer key)
  11 = KIA_KEY2 (Kia V6 AES key A)
  12 = KIA_KEY3 (Kia V6 AES key B)
  13 = KIA_KEY4 (Kia V5 counter mixer key)
Any non-empty subset of types 10-13 is accepted; each embedded key gets its
own availability flag so firmware can gate the matching feature.
"""

import argparse
import re
import sys

KIA_KEY_TYPES = (10, 11, 12, 13)
KIA_RAW_SIZE = 16
KIA_RAW_SIZE_FULL = 32
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
    """Return {type: key} for Kia types 10-13, or None for an encrypted RAW keystore.

    Accepts either a plaintext RAW file containing two 8-byte keys (legacy
    layout: types 11 and 12 in order) or four 8-byte keys (types 10, 11, 12
    and 13 in order), or a plaintext standard Flipper keystore holding any
    non-empty subset of types 10-13, with or without its Flipper file header.
    """
    lines = text.splitlines()
    if lines and lines[0].strip().startswith("Filetype:"):
        filetype, lines = _parse_header(text, (RAW_FILETYPE, KEYSTORE_FILETYPE))
    else:
        filetype = KEYSTORE_FILETYPE
    if lines is None:
        return None

    if filetype == RAW_FILETYPE:
        data = _parse_raw_payload(lines, (KIA_RAW_SIZE, KIA_RAW_SIZE_FULL))
        if len(data) == KIA_RAW_SIZE:
            types = (11, 12)
        else:
            types = KIA_KEY_TYPES
        return {
            key_type: int.from_bytes(data[index : index + 8], "big")
            for key_type, index in zip(types, range(0, len(data), 8))
        }

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
    if not keys:
        raise ValueError(
            "plaintext Sub-GHz keystore must contain at least one Kia key "
            "(types 10-13)"
        )
    return keys


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
    """Generate a C source file containing only the supplied key data.

    kia_keys maps ProtoPirate keystore types (10-13) to keys; missing types
    are embedded as zero with their availability flag cleared.
    """
    kia_keys = kia_keys or {}
    kia_mf = kia_keys.get(10, 0)
    kia_v6 = (kia_keys.get(11, 0), kia_keys.get(12, 0))
    kia_v5 = kia_keys.get(13, 0)
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
    0x{kia_v6[0]:016X}ULL,
    0x{kia_v6[1]:016X}ULL,
}};
M1_KEY_DATA_USED const bool m1_kia_v6_keys_builtin_available = {str(11 in kia_keys and 12 in kia_keys).lower()};

M1_KEY_DATA_USED const uint8_t m1_vag_aut64_keys_builtin[M1_VAG_AUT64_KEY_BYTES] = {{
    {_format_bytes(vag_values)},
}};
M1_KEY_DATA_USED const bool m1_vag_aut64_keys_builtin_available = {str(vag_keys is not None).lower()};

M1_KEY_DATA_USED const uint64_t m1_kia_mf_key_builtin = 0x{kia_mf:016X}ULL;
M1_KEY_DATA_USED const bool m1_kia_mf_key_builtin_available = {str(10 in kia_keys).lower()};
M1_KEY_DATA_USED const uint64_t m1_kia_v5_key_builtin = 0x{kia_v5:016X}ULL;
M1_KEY_DATA_USED const bool m1_kia_v5_key_builtin_available = {str(13 in kia_keys).lower()};
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
