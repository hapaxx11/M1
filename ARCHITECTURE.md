<!-- See COPYING.txt for license details. -->

# M1 Project Architecture

## Overview

The M1 firmware is organized into the following main components:

| Directory | Description |
|-----------|-------------|
| `Core/` | Application entry, FreeRTOS tasks, HAL configuration |
| `m1_csrc/` | M1 application logic (display, NFC, RFID, IR, Sub-GHz, CAN, firmware update) |
| `Battery/` | Battery monitoring |
| `NFC/` | NFC (13.56 MHz) support |
| `lfrfid/` | LF RFID (125 kHz) support |
| `Sub_Ghz/` | Sub-GHz radio support |
| `Infrared/` | IR transmit/receive |
| `Esp_spi_at/` | ESP32 SPI AT command interface |
| `USB/` | USB CDC and MSC classes |
| `Drivers/` | STM32 HAL, CMSIS, u8g2, and other drivers |
| `FatFs/` | FAT file system for storage |
| `Middlewares/` | FreeRTOS |
| `lib/furi/` | Furi compatibility layer (FuriString, FURI_LOG, furi_assert/check) |

## Build System

- **STM32CubeIDE:** Uses `.project` and `.cproject`
- **VS Code / CMake:** Uses `CMakeLists.txt` and `CMakePresets.json`
