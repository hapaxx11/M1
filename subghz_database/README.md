# M1 Sub-GHz SD Card Files

This folder is copied to `SubGHz/` on the M1 SD card by the firmware release
packaging step.  It contains signal capture databases and supporting tools for
Sub-GHz features.

## Contents

| Path | Description |
|------|-------------|
| `Doorbells/` | Wireless doorbell signal captures (.sub) |
| `Fans/` | RF-controlled fan signal captures (.sub) |
| `Outlet_Switches/` | Remote outlet/switch signal captures (.sub) |
| `Smart_Home/` | Smart home controller signal captures (.sub) |
| `Weather_Stations/` | Weather sensor signal captures (.sub) |
| `keeloq/` | KeeLoq manufacturer key setup files |
| `playlist/` | Pre-built Sub-GHz playlists |
| `SOURCES.md` | Attribution and license information for bundled signals |

---

## KeeLoq Rolling-Code Replay Setup

The M1 can replay captured KeeLoq, Jarolift, and Star Line rolling-code
signals using counter-mode re-encryption.  To enable this feature you need to
supply the manufacturer master keys — keys that are proprietary to each
manufacturer and cannot be distributed publicly.

### Prerequisites

- A **Flipper Zero** with
  [RocketGod's SubGHz Toolkit](https://github.com/RocketGod-git/RocketGods-SubGHz-Toolkit)
  installed.
- **Python 3** on your PC (any recent version).

### Step-by-step

1. **Export keys from your Flipper Zero**

   On the Flipper, open RocketGod's SubGHz Toolkit and run:
   > **Decrypt KeeLoq Manufacturer Codes**

   This writes `keeloq_keys.txt` to the Flipper's SD card.

2. **Copy `keeloq_keys.txt` to your PC**

   Transfer the file from the Flipper SD card to your computer.

3. **Convert to M1 format**

   The `keeloq/convert_keeloq_keys.py` script (included in this folder on
   the SD card) converts the RocketGod output to the compact format the M1
   firmware reads:

   ```
   python3 convert_keeloq_keys.py keeloq_keys.txt keeloq_mfcodes
   ```

   This produces a file called `keeloq_mfcodes`.

4. **Copy `keeloq_mfcodes` to the SD card**

   Place the file at the following path on the M1 SD card:

   ```
   SUBGHZ/keeloq_mfcodes
   ```

   That is, directly inside the `SubGHz/` folder — **not** inside `keeloq/`.

5. **Done**

   The M1 firmware loads `SUBGHZ/keeloq_mfcodes` automatically.  Any
   captured `.sub` file whose `Manufacture:` field matches a loaded key will
   be replayed via counter-mode re-encryption.

### Key file format reference

The `keeloq_mfcodes` file uses a simple plain-text format — one entry per
line, `#` comments ignored:

```
KEY_HEX:TYPE:MANUFACTURER_NAME
```

| Field | Description |
|-------|-------------|
| `KEY_HEX` | 64-bit master key — exactly 16 uppercase hex digits |
| `TYPE` | Learning type: `1` = Simple, `2` = Normal, `3` = Secure |
| `MANUFACTURER_NAME` | Matched case-insensitively against the `Manufacture:` field in `.sub` files |

Example (illustrative — not real keys):

```
0123456789ABCDEF:1:ExampleCorp
FEDCBA9876543210:2:AnotherBrand
```

The M1 firmware also accepts the RocketGod multi-line format directly, so you
can skip the conversion step and place `keeloq_keys.txt` on the SD card
renamed to `keeloq_mfcodes` if you prefer.

See `keeloq/keeloq_mfcodes.example` for a fully commented template.

---

## Signal Files (.sub)

Bundled signal captures are in Flipper Zero `.sub` format and are compatible
with the M1 Sub-GHz **Saved** feature.  See `SOURCES.md` for attribution and
license information.
