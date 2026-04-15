<!-- See COPYING.txt for license details. -->

# M1 Hardware Documentation

For the full schematic-level component and signal reference (ICs, power subsystem, RF
subsystems, capability assessment), see
[`documentation/hardware_schematics.md`](documentation/hardware_schematics.md).

## Enclosure 3D Models

STEP files for the M1 enclosure are in [`cad/step/`](cad/step/).
See that directory's README for the file inventory and revision date.

## MCU

- **Part:** STM32H573VIT6
- **Type:** 32-bit ARM Cortex-M33
- **Flash:** 2 MB dual-bank
- **RAM:** 640 KB
- **Speed:** 250 MHz
- **Package:** 100-pin LQFP

## Hardware Revision

- **Supported:** 2.x (Main Board REV2.8R, NFC Board REV2.7R)

## Key Peripherals

| Peripheral | Interface | Notes |
|------------|-----------|-------|
| ESP32 WiFi/BT module | SPI AT | Runtime only; UART used for flashing |
| Si4463 Sub-GHz | SPI | Antenna switch GPIO lines |
| 125 kHz LF-RFID subsystem | GPIO/SPI | Separate NFC board; read + emulate |
| 13.56 MHz NFC/HF-RFID | SPI | IC identity unconfirmed from available sheets |
| Display | SPI | 4-wire |
| SD card | SDIO 4-bit | Card-detect pin |
| Battery charger | I2C | BQ25895-compatible; OTG-capable |
| Battery fuel gauge | I2C | BQ27xxx-compatible |
| USB | USB-C | PD controller present |
| CAN-FD (FDCAN1) | J7 (X10) PD0/PD1 | Requires external CAN transceiver |

## External CAN Transceiver (Required for CAN Bus)

The M1's STM32H573 MCU has a built-in FDCAN peripheral, but the board does **not**
include an on-board CAN transceiver.  An external transceiver module must be connected
to the **J7 (X10)** header to use CAN bus features.

### Recommended Module

**[Waveshare SN65HVD230 CAN Board](https://www.waveshare.com/sn65hvd230-can-board.htm)**

| Property | Value |
|----------|-------|
| Transceiver IC | Texas Instruments SN65HVD230 |
| Logic voltage | **3.3 V** (direct match — no level shifter needed) |
| Bus voltage | 5 V differential (ISO 11898) |
| Max bitrate | 1 Mbps |
| ESD protection | Yes (built-in) |
| Termination | 120 Ω jumper on-board |
| Connector | CAN_H / CAN_L screw terminal |

### Wiring (J7 → Waveshare board)

| J7 (X10) Pin | Signal | Waveshare Pin |
|---------------|--------|---------------|
| PD1 | FDCAN1_TX | TX (CTX) |
| PD0 | FDCAN1_RX | RX (CRX) |
| EXT3V3 | 3.3 V power | 3V3 |
| GND | Ground | GND |

### Alternative Modules

Any 3.3 V CAN transceiver board will work.  Modules based on these ICs are confirmed
compatible:

- **SN65HVD230** (TI) — 3.3 V native, recommended
- **TJA1051T/3** (NXP) — 3.3 V I/O, CAN-FD capable
- **MCP2562** (Microchip) — 3.3 V logic, 5 V bus-side

> **⚠️ TJA1050-based boards (5 V only)** require a 5 V supply and may need a level
> shifter for safe operation with the M1's 3.3 V GPIOs.

## Development Tools

- ARM GCC toolchain (14.2+, recommended: 14.3 bundled in STM32CubeIDE 2.1.0)
- CMake + Ninja (primary build system)
- STM32CubeIDE 1.17+ (alternative)
- ST-Link or J-Link debugger
- Python 3 (for post-build CRC injection)
