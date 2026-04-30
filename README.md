> **WARNING:** This is custom third-party firmware. Use at your own risk. The developer assumes no responsibility for bricked devices, hardware damage, fires, data loss, or any other issues that may result from flashing this firmware. Always have an ST-Link programmer available to recover your device. By flashing this firmware, you accept full responsibility for any outcome.

---

# M1 SiN360 Firmware

Custom firmware for the Monstatek M1 multi-protocol security research device, built on STM32H5 with an ESP32-C6 wireless coprocessor.

This project started as a fork of the original Monstatek M1 firmware and adds completed feature stubs, a working SD-card update flow, expanded NFC/RFID/IR tools, and a growing WiFi/BLE toolset backed by companion ESP32-C6 firmware.

## Current Release

**SiN360 v0.9.0.7**

This release focuses on WiFi scanner coverage, Evil Portal custom-page capture improvements, BLE foundation work, ESP32 update support, UI polish, and branding refresh.

### Branding

- Firmware version bumped to `0.9.0.7`
- Boot screen now uses SiN360 branding with the firmware version underneath
- Boot and main-menu logos updated with custom monochrome artwork
- Default BLE advertising name changed to `SiN360-M1`

### ESP32-C6 Companion Firmware

The WiFi and BLE features require matching ESP32-C6 companion firmware. The STM32 and ESP32 sides communicate over a 64-byte binary SPI protocol, so both firmwares must be kept in sync.

ESP32 firmware repo:

<https://github.com/sincere360/M1_SiN360_ESP32>

Matching ESP32 `v0.9.0.7` release:

<https://github.com/sincere360/M1_SiN360_ESP32/releases/tag/v0.9.0.7>

Detailed user how-tos are coming later.

### WiFi

- AP scanning with scrollable results
- Station scanning and AP/station target selection
- Packet monitor/sniffers for beacon, probe, deauth, EAPOL, SAE commit, Pwnagotchi detection, and raw packet counts
- Deauth attack with selected AP/station target plumbing
- Beacon Spam from SD-card `.txt`/`.lst` SSID lists
- Beacon Spam modes: all in order, shuffle all, random subset
- Rickroll beacon preset
- AP Clone beacon spam, using selected APs first when marked
- Probe Request Flood
- Evil Portal with captive portal DNS/HTTP, credential capture, JSON/form handling, and custom page helper injection
- Custom Evil Portal HTML upload from SD card with a 32KB limit
- Karma and Karma Portal modes, including active SSID placeholder support for custom portal pages
- WiFi radio controls: join WiFi, set MACs, set channel, shutdown WiFi
- AP cache save/load/clear and SSID pool load/clear
- TCP scanners for SSH, Telnet, and common ports over the joined WiFi network
- Ping Scan and ARP Scan over the joined WiFi network
- Wardrive and Station Wardrive CSV exports to SD card
- MAC Track for monitoring selected devices

### Bluetooth / BLE

- BLE device scanning
- BLE advertising with configurable name saved to SD card
- Full text keyboard support for BLE names
- BLE raw advertisement command support on ESP32
- BLE spam payload groups:
  - Sour Apple
  - Swiftpair
  - Samsung BLE
  - Flipper BLE
  - Spam All
- Basic raw-advertisement analyzer and starter detection screens for AirTag/Flipper-style payloads

### UI And Input

- Full text keyboard for WiFi passwords, Portal SSID, and BLE advertising names
- Keyboard supports lowercase, uppercase, numbers, symbols, spaces, backspace, and up to 63-byte WiFi passwords
- Existing filename keyboard remains for file/path style entry
- File browser is used for SD-card SSID lists, AP caches, IR files, NFC files, and portal HTML files

### Existing Tooling

- NFC read/write/emulate features, including NTAG215 Amiibo support
- NFC utilities: Cyborg Detector, Mifare Fuzzer, Write URL, UID write, T2T wipe
- 125 kHz RFID read/write/emulate and T5577 clone flow
- Sub-GHz RX/TX with configurable radio settings
- Infrared TX/RX, Universal Remote, and Flipper `.ir` file loading
- GPIO controls
- USB CDC + MSC composite mode
- STM32 firmware update from SD card
- ESP32 firmware update from SD card using merged ESP32 binary plus uppercase MD5 sidecar
- Dual-bank firmware switching

## Evil Portal Custom HTML

Custom Evil Portal pages are loaded from SD card through the WiFi portal configuration flow. Use `.html` or `.htm` files and keep each page at or below `32768` bytes. Inline CSS and JavaScript work best because captive portal clients may not load external `http` or `https` assets reliably.

Use `{{SSID}}` anywhere in custom HTML to show the active portal SSID. This also works with Karma Portal, so a page can display the current probe-followed SSID instead of a stale configured name.

For the most reliable credential capture, use a normal POST form:

```html
<form method="POST" action="/login">
  <input name="ssid" placeholder="Network name">
  <input name="password" type="password" placeholder="Password">
  <button type="submit">Continue</button>
</form>
```

The portal accepts common username/network field names: `user`, `username`, `email`, `login`, `identity`, `ssid`, `wifi`, `wifi_name`, `network`, and `network_name`. Password fields can be named `pass`, `password`, or `pwd`.

Custom pages also get an injected capture helper. It mirrors JavaScript form submissions and `fetch()` POST bodies to `/login`, so pages that call `preventDefault()` can still capture as long as the form fields use the supported names. JSON POST bodies are accepted too, including common `/sendJSON` style pages.

## Status

Still in progress:

- Detailed user how-tos and screenshots
- More BLE detection signatures and payload tuning
- More Evil Portal polish and a dedicated portal authoring document
- BadUSB/HID UI and scripting
- GPS and other advanced features

## Hardware

- **MCU:** STM32H573VIT6, ARM Cortex-M33, 250 MHz
- **Wireless coprocessor:** ESP32-C6-MINI-1, WiFi 6 + BLE 5
- **Sub-GHz:** Si446x radio, 300-915 MHz
- **NFC:** ST25R3916, 13.56 MHz
- **LF RFID:** 125 kHz read/write/emulate, EM4100/H10301/T5577
- **IR:** IRMP/IRSND protocol library
- **Display:** 128x64 OLED through u8g2
- **USB:** CDC + MSC composite device
- **Storage:** SD card through FatFS

See `HARDWARE.md` for pin mapping and schematic details.

## Building STM32 Firmware

### Prerequisites

- ARM GCC toolchain, `arm-none-eabi-gcc` 14.2 or newer
- CMake 3.22+
- Ninja
- Python 3

### Linux Command Line

```bash
make
```

Output:

```text
artifacts/M1_SiN360_v0.9.0.7.bin
```

### VS Code

1. Install the prerequisites above.
2. Install the VS Code **CMake Tools** and **Cortex-Debug** extensions.
3. Open the project folder.
4. Select the release CMake preset.
5. Build from the CMake panel or with the configured build task.

Output is normally:

```text
out/build/gcc-14_2_build-release/M1_SiN360_v0.9.0.7_SD.bin
```

## Building ESP32 Firmware

The ESP32-C6 companion firmware is built with ESP-IDF v5.5.

```bash
cd ~/Documents/m1_esp32
. ~/Documents/esp-idf/export.sh
idf.py build
```

For SD-card ESP32 firmware updates, generate a merged image and an uppercase, no-newline MD5 sidecar:

```bash
idf.py merge-bin -o m1_esp32_merged.bin
md5sum build/m1_esp32_merged.bin | awk '{print toupper($1)}' | tr -d '\n' > build/m1_esp32_merged.md5
```

Copy both files to the SD card:

```text
build/m1_esp32_merged.bin
build/m1_esp32_merged.md5
```

The `.md5` file must be exactly 32 uppercase hex characters with no newline.

## Flashing STM32 Firmware

### Via SD Card

1. Copy `M1_SiN360_v0.9.0.7.bin` to the M1 SD card.
2. On the M1, open **Settings > Firmware Update**.
3. Select the firmware file.
4. Wait for the update to complete and reboot.

### Via ST-Link

Connect ST-Link V2 to the M1 GPIO header:

- Pin 10, PA14 -> SWCLK
- Pin 11, PA13 -> SWDIO
- Pin 8 or 18 -> GND

```bash
st-flash write M1_SiN360_v0.9.0.7.bin 0x08000000
```

## Dual Firmware Banks

The M1 has two flash banks. SD-card firmware updates write to the inactive bank, which makes it possible to keep two firmware builds installed and switch between them.

SiN360 supports switching from **Settings > Switch Bank**. A safety fallback is available by holding **BACK + DOWN** during power-up to roll back to the other bank.

## Credits

- **Monstatek** -- Original M1 hardware and base firmware
- **SiN360** -- Custom firmware development, feature completion, build fixes
- **IRMP/IRSND** -- IR protocol library by Frank Meyer
- **Flipper Zero community** -- IR code database references and inspiration

## License

See `LICENSE` and `README_License.md` for details.
