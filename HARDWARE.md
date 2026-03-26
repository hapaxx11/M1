<!-- See COPYING.txt for license details. -->

# M1 Hardware Documentation

For the full schematic-level component and signal reference (ICs, power subsystem, RF
subsystems, capability assessment), see
[`documentation/hardware_schematics.md`](documentation/hardware_schematics.md).

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

## Development Tools

- STM32CubeIDE (recommended)
- J-Link or ST-Link debugger
- ARM GCC toolchain (14.2 or compatible)
