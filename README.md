> **WARNING:** This is custom third-party firmware. Use at your own risk. The developer assumes no responsibility for bricked devices, hardware damage, fires, data loss, or any other issues that may result from flashing this firmware. If you don't want to take that risk, wait for official firmware from Monstatek. If something goes wrong, don't come crying or complaining -- you were warned. Always have an ST-Link programmer available to recover your device. By flashing this firmware, you accept full responsibility for any outcome.

---

# M1 SiN360 Firmware

Custom firmware for the Monstatek M1 multi-protocol security research device, built on STM32H5.

Forked from the original Monstatek M1 firmware (v0.8) with completed feature stubs, new tools, and build system fixes.

## What's New in SiN360 v0.9.0.3

### Completed Features (formerly stubs)
- **USB-UART Bridge** -- Full runtime CDC mode switching with status screen, transparent bridge operation
- **Sub-GHz Radio Settings** -- Configurable frequency band (300-915 MHz), modulation (OOK/ASK/FSK), TX power (4 levels)
- **Sub-GHz Recording** -- Live signal feedback during recording
- **125 kHz RFID Utilities**
  - **Clone Card** -- Quick read-to-write flow for T5577 without saving to SD
  - **Brute Force FC** -- Cycle H10301 facility codes 0-255 with progress bar, pause/resume
- **IR Universal Remote** -- Built-in database with 15 TV brands (Samsung, LG, Sony, Philips, Panasonic, Vizio, TCL/Roku, Hisense, Toshiba, Sharp, Insignia, Sanyo, Magnavox, and 2 generic NEC profiles), 8-14 commands per brand
- **Switch Bank** -- Swap between two firmware banks from Settings menu with confirmation screen, enabling dual-firmware setups with other community developers
- **IR File Loader** -- Load Flipper-compatible .ir files from SD card, browse and transmit any command. Supports NEC, Samsung32, Samsung48, RC5, RC6, SIRCS, Kaseikyo, Denon/Sharp, JVC, BOSE, LG protocols
- **NFC Cyborg Detector** -- Continuous NFC field to illuminate body implant LEDs
- **NFC Mifare Fuzzer** -- Cycles sequential UIDs emulating Mifare Classic 1K to test reader responses
- **NFC Write URL** -- Write custom URLs as NDEF to NTAG tags (NFC business cards)
- **NFC Utils** -- Write UID to magic cards, Wipe T2T tags

### Build System Fixes
- **Fixed CRC generation** -- Replaced broken srec_cat CRC with correct STM32 hardware CRC32 append via Python script
- **SD card updates now work reliably** -- Users with original firmware can update via SD card without CRC failures
- **Clean build output** -- Single firmware binary with correct naming

### Boot Screen
- Shows "SiN360" branding below version number

## Dual Firmware Setup

The M1 has two flash banks. You can run a different firmware on each bank and switch between them. No ST-Link required.

### Initial Setup (no ST-Link needed)

1. Start with any working firmware on your M1
2. Put the first custom firmware .bin on the SD card
3. Settings > Firmware Update > select the file > update completes and reboots
4. You are now on Bank 2 with the first custom firmware
5. Put the second custom firmware .bin on the SD card
6. Settings > Firmware Update > select the file > update completes and reboots
7. You are now on Bank 1 with the second custom firmware
8. Both banks now have custom firmware

#### Switching Between Firmwares

Each firmware may have a different way to switch banks. For SiN360:
- **From Settings menu:** Settings > Switch Bank > OK > OK to confirm > device reboots to other bank
- **From boot (safety fallback):** Hold BACK + DOWN during power-up to rollback to the other bank

For other community firmwares, follow their instructions for switching banks.

### Updating One Firmware

The SD card update always writes to the **inactive** bank. To update a specific firmware:

1. **Switch to the firmware you are NOT updating** (the one you want to keep)
2. SD card update with the new .bin file
3. It writes to the inactive bank and reboots into the updated firmware

Example: To update SiN360 while keeping OtherFW:
1. Switch to OtherFW (Settings > Switch Bank)
2. SD card update with new SiN360 .bin
3. Device reboots into updated SiN360 on the other bank

### Fixing Broken CRC Files

If another developer's firmware .bin has CRC issues (fails SD card update), you can fix it:

```
# Strip the bad CRC (last 4 bytes) and re-append correct CRC
head -c -4 broken_firmware_wCRC.bin > firmware_raw.bin
python3 append_crc.py firmware_raw.bin firmware_fixed.bin
```

Or if you have the raw .bin without any CRC:

```
python3 append_crc.py firmware_raw.bin firmware_fixed.bin
```

## Roadmap

- **Phase 1** (in progress): NFC Tools, Settings menus, Bluetooth config
- **Phase 2**: WiFi attack suite via ESP32-C6 (deauth, beacon spam, evil portal, packet monitor)
- **Phase 3**: BadUSB HID keyboard with DuckyScript interpreter
- **Phase 4**: BadUSB over BLE
- **Phase 5**: Favorites, logging, file manager, OTA updates, Lua scripting engine

## Hardware

- **MCU**: STM32H573VIT6 (Cortex-M33, 2MB Flash, 640KB RAM)
- **Sub-GHz**: Si446x radio (300-915 MHz)
- **NFC**: ST25R3916 (13.56 MHz)
- **LF RFID**: 125 kHz read/write/emulate (EM4100, H10301, T5577)
- **IR**: IRMP/IRSND library (NEC, Samsung32, RC5, SIRCS, Kaseikyo + more)
- **Wireless**: ESP32-C6 co-processor (WiFi + BLE)
- **Display**: ST7586s ERC240160 (128x64)
- **USB**: CDC + MSC composite device
- **Hardware revision**: 2.x

See HARDWARE.md for pin mapping and schematic details.

## Building

### Prerequisites

- ARM GCC toolchain (arm-none-eabi-gcc 14.2+)
- CMake 3.22+
- Ninja build system
- Python 3 (for CRC generation)

### Linux (command line)

```
make
```

Output: ./artifacts/M1_SiN360_v0.9.0.1.bin (SD card update file with CRC)

### VS Code

1. Install extensions: CMake Tools, Cortex-Debug
2. Configure the project (select gcc-14_2_build-release)
3. Build via the Build icon

Output: ./out/build/gcc-14_2_build-release/M1_SiN360_v0.9.0.1_SD.bin

### STM32CubeIDE

Open the project and build in the IDE.

## Flashing

### Via ST-Link (development)

Connect ST-Link V2 to the M1 GPIO header:
- Pin 10 (PA14) -> SWCLK
- Pin 11 (PA13) -> SWDIO
- Pin 8 or 18 (GND) -> GND

```
st-flash write M1_SiN360_v0.9.0.1.bin 0x08000000
```

### Via SD Card (recommended for users)

1. Copy M1_SiN360_v0.9.0.1.bin to your M1's SD card
2. On the M1: Settings > Firmware Update
3. Browse to the file and select it
4. Wait for "UPDATE COMPLETED" and reboot

Works from original Monstatek firmware or any previous SiN360 version.

## Contributing

Contributions are welcome. See CONTRIBUTING.md and the Code of Conduct.

## License

See LICENSE for details.

## Credits

- **Monstatek** -- Original M1 hardware and base firmware
- **SiN360** -- Custom firmware development, feature completion, build fixes
- **IRMP/IRSND** -- IR protocol library by Frank Meyer
- **Flipper Zero community** -- IR code database references