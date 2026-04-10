# Sub-GHz Playlist Database — Sources

## Provenance

These playlist files are curated from the
[UberGuidoZ/Flipper](https://github.com/UberGuidoZ/Flipper) repository
(`subplaylist/` directory), commit `c074cebb`.

## License

The original files are distributed under the **GNU General Public License v3.0**
(GPLv3).  A copy of the license is included as `LICENSE` in this directory.

## Adaptations for M1

- **Path remapping**: Flipper Zero uses `/ext/subghz/...` paths. M1 uses
  `/SUBGHZ/...` on the SD card. All paths have been converted accordingly.
- **16-entry limit**: The M1 playlist player loads at most 16 files per playlist
  (firmware memory constraint). Larger UberGuidoZ playlists have been curated
  to the most representative entries.
- **Flipper path compatibility**: The M1 firmware automatically remaps
  `/ext/subghz/...` → `/SUBGHZ/...` at runtime, so unmodified Flipper playlists
  also work when copied directly to the SD card.

## Directory Structure

```
subghz_playlist/
├── Example_Playlist.txt      — Template for creating custom playlists
├── Doorbells/
│   └── DingDong.txt          — Ring various doorbell models
├── Fans/
│   ├── Fans_Off.txt          — Turn off ceiling/exhaust fans
│   └── Fans_On.txt           — Turn on ceiling/exhaust fans (high speed)
├── Tesla/
│   └── Tesla_Charge_Port.txt — Open Tesla charge port (315/433 MHz)
├── LICENSE                   — GPLv3 (from UberGuidoZ/Flipper)
└── SOURCES.md                — This file
```

## Usage

1. Copy the desired `.txt` playlist files to `/SUBGHZ/playlist/` on your M1
   SD card.
2. Ensure the referenced `.sub` files also exist at the paths specified in the
   playlist.  The `.sub` files can be sourced from the
   [UberGuidoZ/Flipper Sub-GHz collection](https://github.com/UberGuidoZ/Flipper/tree/main/Sub-GHz).
3. On the M1, navigate to **Sub-GHz → Playlist**, select
   a `.txt` file, adjust repeat count with L/R, and press OK to play.

## Original Source Categories (Not All Included)

The UberGuidoZ subplaylist collection includes playlists for:

| Category | Files | Included in M1 |
|----------|-------|-----------------|
| Tesla | 2+ (315/433 MHz) | ✅ Curated |
| Doorbells | 1 (82 entries) | ✅ Curated (16) |
| Fans | 2 (on/off, 100+ entries each) | ✅ Curated (14 each) |
| Remote Outlets | 2 (on/off) | ❌ Excluded |
| LEDs | 2 (on/off) | ❌ Excluded |
| Pagers | 2 (Retekess/iBells) | ❌ Excluded |
| KeyFinder | 1 | ❌ Excluded |
| LPD Channels | 1 (69 entries) | ❌ Excluded |
| PMR446 Ring | 1 (16 entries) | ❌ Excluded |
| CAME 12-Bit BF | 433/868 MHz brute-force | ❌ Excluded |
| Misc | 2 | ❌ Excluded |
| Sextoy | 2 | ❌ Excluded |

Excluded playlists can be downloaded from the
[UberGuidoZ repository](https://github.com/UberGuidoZ/Flipper/tree/main/subplaylist)
and used directly — the M1 firmware remaps Flipper paths automatically.
