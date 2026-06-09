# Rainbow Table Solver

Host-only utility for extracting, validating, and testing the 32-byte
rainbow / permutation tables used by the Nice FloR-S and Alutech AT-4N
Sub-GHz ciphers.  This tool is **not** compiled into the firmware — it
runs on a development machine and produces the 64-character hex string
stored as a GitHub Actions secret.

## Build

From the repository root:

```bash
cc -O2 -o rainbow_table_solver \
   tools/rainbow_table_solver.c \
   -I Sub_Ghz \
   Sub_Ghz/subghz_nice_flor_s.c \
   Sub_Ghz/subghz_alutech_at_4n.c
```

No external dependencies — only the C standard library and the
cipher sources already in the repo.

## Commands

### `extract` — parse a C array from stdin

Reads an rtl\_433-style C array (decimal or `0x` hex literals,
braces/commas optional) from stdin and prints the 64-character hex
string suitable for pasting into a GitHub Actions secret.

```bash
# Pull the leaf_node array directly from the rtl_433 source:
grep -A4 'leaf_node\[\]' \
    ~/src/rtl_433/src/devices/nice_flor_s.c \
  | ./rainbow_table_solver extract
```

Accepts any of these input formats:

```
{ 25, 5, 63, 97, 203, 109, 69, 10, ... }
{ 0x19, 0x05, 0x3F, 0x61, ... }
25 5 63 97 203 109 69 10 ...
```

### `validate` — check a table against a known vector

```bash
./rainbow_table_solver validate \
    19053F61CB6D450A030740054786B44A299E66C75D76AF653C4D8FAE67941D55 \
    nice_flor_s \
    08FC2E9F526 \
    3AAB665 \
    2813
```

Arguments:

| # | Argument | Description |
|---|----------|-------------|
| 1 | hex table | 64-char hex string (the secret value) |
| 2 | cipher | `nice_flor_s` or `alutech_at_4n` |
| 3 | encrypted payload | hex, from the OTA capture |
| 4 | expected serial | hex |
| 5 | expected counter | decimal |

The example above uses the reference vector from rtl\_433 PR #2238.

### `decrypt` — decrypt a single payload

```bash
./rainbow_table_solver decrypt \
    19053F61CB6D450A030740054786B44A299E66C75D76AF653C4D8FAE67941D55 \
    nice_flor_s \
    08FC2E9F526
```

Prints the decrypted serial and counter.

### `roundtrip` — bulk encrypt/decrypt self-test

```bash
./rainbow_table_solver roundtrip \
    19053F61CB6D450A030740054786B44A299E66C75D76AF653C4D8FAE67941D55 \
    nice_flor_s
```

Encrypts then decrypts all 65 536 counter values for a given serial
(default `0x1234567`) and reports any failures.  Useful for verifying
cipher correctness after changes.

### `convert` — format conversion

```bash
# Hex → C array:
./rainbow_table_solver convert c_array \
    19053F61CB6D450A030740054786B44A299E66C75D76AF653C4D8FAE67941D55

# C array → hex (use extract from stdin instead):
echo '{ 25, 5, 63, ... }' | ./rainbow_table_solver extract
```

## Deriving new tables

### Nice FloR-S

The Nice FloR-S 32-byte table is directly available in the rtl\_433
source as the `leaf_node` array in `src/devices/nice_flor_s.c`.  Use
the `extract` command to convert it.

### Alutech AT-4N

The Alutech AT-4N table has **not yet been sourced** from any public
decoder.  The Flipper/Momentum firmware ships it as an AES-encrypted
Keystore RAW blob — no plaintext equivalent has been found in rtl\_433
or other open-source projects.

If the table is obtained (e.g. from a hardware dump, a new public
decoder, or reverse-engineering the encrypted asset), use this tool to
validate it:

```bash
# Suppose you captured an Alutech OTA transmission and know the
# plaintext serial + counter from a paired remote's LCD display:
./rainbow_table_solver validate \
    <64-char-hex-table> \
    alutech_at_4n \
    <encrypted_payload_hex> \
    <expected_serial_hex> \
    <expected_counter_dec>
```

Then set the GitHub Actions secret:

```bash
# Repository Settings → Secrets → Actions → New repository secret
# Name:  ALUTECH_AT_4N_RAINBOW_TABLE
# Value: <the 64-char hex string>
```

## Workflow integration

The hex string produced by this tool is consumed by:

- `scripts/gen_nice_flor_s_table_builtin.py` (reads
  `NICE_FLOR_S_RAINBOW_TABLE`)
- `scripts/gen_alutech_at_4n_table_builtin.py` (reads
  `ALUTECH_AT_4N_RAINBOW_TABLE`)

These scripts are called during CI to embed the tables into the
firmware build.  See `.github/workflows/build-release.yml`.
