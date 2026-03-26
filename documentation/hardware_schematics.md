<!-- See COPYING.txt for license details. -->

# M1 Hardware Schematics Reference

Derived from **MP-NFC.pdf** (NFC Board, REV2.7R) and **MP-MAIN-1.pdf** (Main Board, REV2.8R).
Passive component designators (R, C, L) and test points are excluded as not actionable.

> **Agent usage note:** Consult this file *secondarily* when assessing whether a hardware
> capability exists on the device — official firmware may not expose everything the silicon
> supports.  Primary truth is always the source code and build configuration.

---

## MCU

- **STM32H573VIT** (Cortex-M33, 250 MHz, 2 MB dual-bank flash, 640 KB RAM, 100-pin LQFP)
- Crystals: main HSE + 32.768 kHz LSE (RTC)
- Debug/programming: SWD header (`SWCLK`, `SWDIO`, `SWO`, `/RST`) — full CoreSight trace available

---

## WiFi / Bluetooth Module

- **ESP32-based module** (designated M1 on schematic) connected to MCU via **SPI AT** interface
- Runtime signals: `ESP32_TX`, `ESP32_RX`, `ESP32_HANDSHAKE`, `ESP32_DATAREADY`
- Firmware flash path: `ESP32_BOOT` + separate USB-UART header (`ESP_BOOT`, `RST`, `RX`, `TX`)
- Espressif stock UART-based AT firmware is **incompatible** — must use SPI-mode AT build

---

## Sub-GHz RF Transceiver

- **Si4463** (inferred from net labels `SPI4463_RFSH1`, `RFSH2`, `GP00`–`GP03`, `BAT_DIG`, `ENA`, `SW1`, `SW2`)
- Dedicated SPI bus to MCU; GPIO lines include two antenna switch controls (`SW1`, `SW2`) and
  two refresh/strobe lines (`RFSH1`, `RFSH2`)
- `BAT_DIG`: battery voltage monitor input to Si4463 (enables transmit power compensation)

---

## 125 kHz LF-RFID Subsystem (NFC Board)

Despite the board name "NFC Board", this subsystem operates at **125 kHz only** (LF-RFID).
No 13.56 MHz HF/NFC transceiver IC is present on this board.

**Signal chain:**
1. RF carrier oscillator/driver IC → coil antenna (`PLUG1` / `ANT1`/`ANT2`) → field generation
2. Received signal path: antenna → rectifier diodes → comparators (dual op-amp) → quad op-amp
   signal conditioning → MCU via `RFID_OUT` / `RFID_RF_IN`
3. `RF_CARRIER` net feeds both the NFC board oscillator and the main board MCU

**Emulator / TAG mode:**
- Transistor Q4 (MOSFET, pull circuit) driven by `RFID_PULL` — enables active load modulation
  for card emulation (the device can impersonate a 125 kHz transponder)

**Key interface signals to MCU:**
| Signal | Direction | Purpose |
|--------|-----------|---------|
| `RFID_OUT` | NFC→MCU | Demodulated bit stream from reader field |
| `RFID_RF_IN` | NFC→MCU | Raw RF envelope / carrier detect |
| `RF_CARRIER` | MCU→NFC | Carrier enable/gate |
| `RFID_PULL` | MCU→NFC | Load modulation drive (emulation) |
| `NFC_IRQ` | NFC→MCU | Interrupt from NFC board |
| `NFC_CS` | MCU→NFC | Chip-select (SPI or GPIO) |
| `1_65` | — | 1.65 V mid-rail reference for op-amp biasing |

---

## NFC (13.56 MHz) Subsystem

Sheets 2–3 of MP-MAIN-1 were not captured in the schematics extract.  The `NFC/` source
directory and `NFC_CS`/`NFC_IRQ` signals confirm a 13.56 MHz transceiver is present on the
main board, but its IC identity is not confirmed from available schematic pages.

---

## Display

- SPI-connected display: `DISPLAY_CS`, `DISPLAY_RST`, `DISPLAY_D1`, `DISPLAY_SCK`
- No touch interface signals visible in captured sheets

---

## SD Card Storage

- **SDIO 4-bit** interface: `SDIO_CK`, `SDIO_CMD`, `SDIO_D1`, `SDIO_D2`
- Card-detect pin: `SD_DETECT`
- Note: only D1/D2 visible in captured sheet — standard SDIO uses D0–D3; D0/D3 likely on
  uncaptured sheet 2 or 3

---

## User Input

Six navigation buttons connected to MCU GPIOs:
`BUTTON_OK`, `BUTTON_UP`, `BUTTON_DOWN`, `BUTTON_LEFT`, `BUTTON_RIGHT`, `BUTTON_BACK`

`BUTTON_OK` also serves as `QON` / ship-mode wake input to the power IC (U3).

---

## USB

- **USB-C connector** (J1) with dedicated **USB PD controller** (U5)
- `VBUS` present; USB PD negotiation capable (voltage/current profiles beyond 5 V possible)
- MCU USB signals route through `VDDUSB` rail (isolated LDO)

---

## Power Subsystem

### Battery
- **Li-Ion 3.7 V / 2100 mAh**, 3-pin connector (`BAT+`, `BTEMP`, `BAT-`)
- `BTEMP` (NTC thermistor input) connected to charger IC → hardware over-temperature protection
- Over-temperature flags: `BAT_OTC` (charge), `BAT_DTC` (discharge)

### Battery Charger (U3)
- Pin-compatible with **BQ25895** or equivalent (BTST, PMID, PSEL, STAT, PG, BAT, REGN, OTG, CE)
- **OTG-capable** — device can act as a USB power source (5 V output on VBUS)
- Communicates over I2C (`I2C_SDA`, `I2C_SCL`); interrupt on `I2C_INT`
- `LO_BAT_INT`: low-battery interrupt to MCU

### Battery Fuel Gauge (U4)
- Pin-compatible with **BQ27xxx** series (GPOUT, BIN, SCL, SDA, SRX, BAT)
- Reports state-of-charge, remaining capacity, temperature, and cycle count over I2C
- `GPOUT`: programmable alert output to MCU

### Voltage Regulators
| Rail | IC | Output | Notes |
|------|----|--------|-------|
| +3V3 (MCU core) | LDO (U11) | 3.3 V | MCU backup / always-on path |
| EXT3V3 | Buck-boost (U7) + current-limit (U8) | 3.3 V | Switchable via `EN_EXT_3V3`; short-circuit protected |
| 5V_EXT | Boost (U9) + current-limit (U10) | 5 V | Switchable via `EN_EXT1_5V` / `EN_EXT2_5V`; short-circuit protected |

The two independent 5 V enable lines (`EN_EXT1_5V`, `EN_EXT2_5V`) suggest two separately
switched 5 V outputs (e.g. for different accessory ports).

### I2C Bus
Shared bus hosts at minimum: battery charger (U3) + fuel gauge (U4).
Interrupt-driven via `I2C_INT` to MCU.

---

## External UART Header

`UART_1_RX` / `UART_1_TX` — accessible header for external serial accessories.

---

## Capability Summary for Firmware Support Assessment

| Capability | Hardware Evidence | Official Firmware Support |
|------------|-------------------|--------------------------|
| 125 kHz LF-RFID read | RF carrier oscillator + comparator chain + `RFID_OUT` | Yes (lfrfid/) |
| 125 kHz LF-RFID emulate | Q4 load-modulation transistor + `RFID_PULL` | Yes |
| 13.56 MHz NFC/HF-RFID | NFC_CS / NFC_IRQ signals + NFC/ source dir | Yes (partial) |
| Sub-GHz TX/RX | Si4463 transceiver | Yes (Sub_Ghz/) |
| WiFi / BT | ESP32 module | Yes (Esp_spi_at/) |
| SD card storage | SDIO interface + SD_DETECT | Yes |
| USB CDC/MSC | USB-C + PD controller | Yes |
| USB OTG (host power) | Charger OTG pin | Not exposed in stock firmware |
| Battery % / health | BQ27xxx fuel gauge | Partial (BQ27 not fully exercised) |
| Battery temperature | BTEMP + charger NTC input | Not exposed to user |
| Switchable 3.3 V output | EXT3V3 rail + EN_EXT_3V3 | Partial |
| Switchable 5 V output(s) | 5V_EXT rail + EN_EXT1/2_5V | Partial |
| External UART accessory | UART_1 header | Not exposed in stock firmware |
| USB PD negotiation | USB PD controller (U5) | Not exposed in stock firmware |
