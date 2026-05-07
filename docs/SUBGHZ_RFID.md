# Sub-GHz And RFID Features

This document tracks the current Sub-GHz and 125 kHz RFID feature set in SiN360 firmware.

Use these tools only with devices, tags, and RF environments you own or have permission to test.

## Sub-GHz

The Sub-GHz app uses the onboard Si446x radio for receive, transmit, signal inspection, and simple protocol tools.

### Radio Coverage

- Preset bands for common Sub-GHz work
- Exact custom frequency entry from `300.000` to `928.000` MHz
- OOK and FSK radio modes where supported by the selected tool
- Adjustable TX power
- Region-check toggle for lab use

Custom frequency entry no longer snaps to the nearest preset. The firmware computes and applies the Si446x frequency-control values for the exact frequency, then uses the nearest practical front-end path for the selected range.

### Signal Tools

- RSSI Meter with live signal level and peak tracking
- Frequency Scanner for sweeping configured ranges
- Spectrum Analyzer for visual RF activity
- Record Ready view with live frequency and radio state

### File And Transmit Tools

- Load and transmit supported Sub-GHz files from SD card
- Playlist Player for sequential `.sub` playback
- Repeat count and progress display for playlist playback
- Basic Flipper-style path handling for playlist entries

### Protocol Support

- Princeton decode/transmit support
- Security+ 2.0 decode support
- Security+ 1.0 ternary decode
- CAME / Prastel / Airforce style fixed-code decode
- Nice FLO fixed-code decode
- Linear fixed-code decode
- Holtek decode
- Hormann decode
- Doitrand decode
- Dooya decode
- CAME Atomo decode
- CAME Twee decode
- Ansonic decode
- BETT decode
- Clemsa decode
- Gate TX decode
- Marantec decode
- Mastercode decode
- SMC5326 decode
- Holtek HT12x decode
- Nero Radio decode
- Nice Flor S / Nice One decode
- Alutech AT-4N decode
- Kinggates Stylo 4K decode
- Honeywell WDB decode
- MegaCode decode
- Chamberlain 7/8/9-bit decode
- Linear Delta3 decode
- Feron decode
- GangQi decode
- Hay21 decode
- Marantec24 decode
- Roger decode
- Hollarm decode
- KeeLoq raw frame decode
- Star Line raw frame decode
- FAAC SLH raw frame decode
- Phoenix V2 raw frame decode
- Magellan raw frame decode
- Legrand decode
- Dickert MAHS decode
- Kia raw frame decode
- Scher-Khan raw frame decode
- iDo raw frame decode
- Nero Sketch decode
- Intertechno V3 decode
- Add Manually for Princeton-style keys
- Brute-force helpers for Princeton, CAME, Nice FLO, Linear, and Holtek style fixed-code ranges
- Capture display shows parsed serial, button, counter, event, fixed-code, hopping-code, and manufacturer detail where the protocol exposes it

## 125 kHz RFID

The RFID app supports read, save, write, clone, and emulate flows for a larger LF RFID protocol set.

### Current Protocol Coverage

- EM4100
- EM4100 32-bit variant
- EM4100 16-bit variant
- H10301
- HIDProx
- HIDExt
- IOProxXSF
- AWID
- FDX-A
- FDX-B
- Pyramid
- Indala26
- Idteck
- Keri
- Nexwatch
- Jablotron
- Viking
- Paradox
- PAC/Stanley
- Noralsy
- GProxII
- RadioKey/Securakey
- Gallagher
- Electra

Protocols have read/write/emulate support where the local protocol structure and tag type support it. T5577 writing is available for compatible LF formats.

Gallagher and Electra currently save, write, and emulate raw frames. They work as protocol ports, but they still need friendlier decoded field views.

### RFID Tools

- Read card
- Save card
- Emulate saved card
- Clone compatible cards to T5577
- Blank T5577 writer
- H10301 facility-code brute-force writer
- H10301 RFID fuzzer

## Field Testing Notes

- Test RFID write/emulate with expendable T5577 tags first.
- PSK/direct protocols may need more antenna and timing validation.
- For Sub-GHz custom frequencies, verify output with a spectrum analyzer, SDR, or VNA-assisted setup before relying on a new frequency range.
