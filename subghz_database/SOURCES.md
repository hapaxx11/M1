# Sub-GHz Database — Sources & Attribution

## Overview

This directory contains Sub-GHz signal capture files in Flipper Zero `.sub`
format.  Copy files to `SubGHz/` on the M1's SD card to use with the
Sub-GHz Saved feature, or as reference signals for protocol testing.

## Source

All `.sub` files in this directory were imported from the
[UberGuidoZ/Flipper](https://github.com/UberGuidoZ/Flipper) community
repository.

- **Repository**: <https://github.com/UberGuidoZ/Flipper>
- **Commit SHA**: `c074cebb2eba2d2f9ea278d5759552a7809c54b6`
- **Import date**: 2026-04-04
- **License**: **GNU General Public License v3.0** (see `LICENSE` in this
  directory)

## Imported Categories

| M1 Directory       | Source Directory          | Files | Notes |
|--------------------|--------------------------|------:|-------|
| Outlet_Switches/   | Remote_Outlet_Switches/  |   179 | Princeton, Intertechno, RAW — fixed-code smart plugs/power strips |
| Doorbells/         | Doorbells/               |    81 | Princeton, CAME, RAW — wireless doorbell signals |
| Weather_Stations/  | Weather_stations/        |    39 | BinRAW, RAW, TX141THBv2, Nexus-TH — weather sensor data |
| Smart_Home/        | Smart_Home_Remotes/      |    10 | Princeton, RAW — smart home controllers |
| Fans/              | Fans/                    |     4 | Princeton — RF-controlled fans |
| **Total**          |                          | **313** | |

## Categories NOT Imported

The following categories from UberGuidoZ/Flipper were evaluated and
excluded for the reasons noted:

| Category              | Files | Reason |
|-----------------------|------:|--------|
| TouchTunes            | 8,199 | Jukebox-specific, region-locked, very large |
| Gates                 |   981 | Device-specific rolling codes, 132 MB |
| Ceiling_Fans          |   356 | Large, mostly device-specific captures |
| LED                   |   346 | Large, mostly device-specific captures |
| Restaurant_Pagers     |   287 | Niche, device-specific |
| Garages               |   169 | Device-specific rolling codes (LiftMaster, Chamberlain, etc.) |
| Misc                  |   181 | Unclassified grab-bag |
| Multimedia            |    90 | Mixed quality |
| Vehicles              |    35 | Security-sensitive, device-specific |
| Customer_Assist.      |    27 | Niche |
| Gas_Sign              |    23 | Niche |
| Adjustable_Beds       |    22 | Niche |
| Jamming               |     0 | Intentional interference — excluded on principle |
| Settings, Sleep, etc. |   var | Configuration files, not signal data |

## Signal Types

Files use two main encoding modes:

- **Parsed** (e.g. `Protocol: Princeton`) — decoded into protocol + key.
  These are the most portable and useful for replay.
- **RAW** (e.g. `Protocol: RAW` / `Protocol: BinRAW`) — raw IQ captures.
  Larger files, device-specific, but still usable for replay.

## Updating

To refresh from UberGuidoZ/Flipper:

```bash
git clone --depth=1 https://github.com/UberGuidoZ/Flipper.git /tmp/flipper
# Review Sub-GHz/ categories for new content
# Copy selected .sub files, update commit SHA and date above
```

## License

All files in this directory are licensed under the
**GNU General Public License v3.0**.  See the `LICENSE` file for the
full license text.

Original work by [UberGuidoZ](https://github.com/UberGuidoZ) and
community contributors.
