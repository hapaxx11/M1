<!-- See COPYING.txt for license details. -->

# M1 Hardware Schematics Reference

Derived from all **7 schematic sheets**: **MP-NFC.pdf** (NFC Board, REV2.7R) and
**MP-MAIN-1.pdf** (Main Board, REV2.8R, sheets 1–5).  Passive component designators (R, C, L)
are summarised by function group in the [Passive Components](#passive-components) section at the
end of this file; individual values are omitted as not actionable.

> **Agent usage note:** Consult this file *secondarily* when assessing whether a hardware
> capability exists on the device — official firmware may not expose everything the silicon
> supports.  Primary truth is always the source code and build configuration.

---

## MCU

- **STM32H573VIT** (Cortex-M33, 250 MHz, 2 MB dual-bank flash, 640 KB RAM, 100-pin LQFP)
  — designated **U1-A** on schematic; power-management portion designated **U1-B**
  (VREF, VDDUSB, VBAT, VDDA, VSS, VDD rails)
- Crystals: **Y1** (main HSE system clock), **Y2** (32.768 kHz LSE for RTC)
- SWD debug header (5-pin): `SWO`, `SWCLK`, `SWDIO`, `/RST`, `GND` — full CoreSight trace
  available
- USB-UART flash header (4-pin): `ESP_BOOT`, `RST`, `RX`, `TX` — used for ESP32 firmware
  flashing via ROM bootloader
- Voltage supervisor / reset IC: **U2** (main board)

---

## WiFi / Bluetooth Module

- **ESP32-based module** (designated **M1** on schematic) connected to MCU via **SPI AT**
  interface
- Runtime signals: `ESP32_TX`, `ESP32_RX`, `ESP32_HANDSHAKE`, `ESP32_DATAREADY`
- Firmware flash path: `ESP32_BOOT` + dedicated USB-UART header (`ESP_BOOT`, `RST`, `RX`, `TX`)
- Espressif stock UART-based AT firmware is **incompatible** — must use SPI-mode AT build

---

## Sub-GHz RF Transceiver

- **Si4463** (inferred from net labels `SPI4463_RFSH1`, `RFSH2`, `GP00`–`GP03`, `BAT_DIG`, `ENA`, `SW1`, `SW2`)
- Dedicated SPI bus to MCU; GPIO lines include two antenna switch controls (`SW1`, `SW2`) and
  two refresh/strobe lines (`RFSH1`, `RFSH2`)
- `BAT_DIG`: battery voltage monitor input to Si4463 (enables transmit power compensation)

---

## 125 kHz LF-RFID Subsystem (NFC Board)

The board is labelled "NFC Board" in the hardware, but its subsystem operates at **125 kHz** (LF-RFID).
The 13.56 MHz HF/NFC transceiver is located on the main board (see [NFC (13.56 MHz) Subsystem](#nfc-1356-mhz-subsystem) below).

**NFC board ICs:**
| Ref | Description |
|-----|-------------|
| U2 (NFC) | RF carrier oscillator / driver IC |
| U3-A, U3-B (NFC) | Dual op-amp / comparator (e.g. LM393) — signal demodulation |
| U4-A, U4-B (NFC) | Quad op-amp (e.g. LM324) — signal conditioning |
| U5 (NFC) | High-side / low-side driver IC |
| U7 (NFC) | Op-amp / buffer |
| Q4 (NFC) | MOSFET/transistor — TAG mode load modulation pull |

**Antenna connections:**
- 125 kHz coil antenna via **PLUG1** (4-pin connector), **CON1** (antenna board connector),
  and terminals **ANT1** / **ANT2**
- Rectifier / signal diodes: VD2, VD3, VD4, VD5 (NFC board)

**Signal chain:**
1. RF carrier oscillator/driver (U2) → coil antenna → field generation
2. Received signal path: antenna → rectifier diodes → comparators (U3-A/B) → quad op-amp
   (U4-A/B) signal conditioning → MCU via `RFID_OUT` / `RFID_RF_IN`
3. `RF_CARRIER` net feeds both the NFC board oscillator and the main board MCU

**Emulator / TAG mode:**
- Q4 (MOSFET, pull circuit) driven by `RFID_PULL` — enables active load modulation
  for card emulation (the device can impersonate a 125 kHz transponder)

**Key interface signals to MCU:**
| Signal | Direction | Purpose |
|--------|-----------|---------|
| `RFID_OUT` | NFC→MCU | Demodulated bit stream from reader field |
| `RFID_RF_IN` | NFC→MCU | Raw RF envelope / carrier detect |
| `RF_CARRIER` | MCU→NFC | Carrier enable/gate |
| `RFID_PULL` | MCU→NFC | Load modulation drive (emulation) |
| `1_65` | — | 1.65 V mid-rail reference for op-amp biasing |

> **Note:** `NFC_CS` and `NFC_IRQ` are **not** part of the 125 kHz subsystem — they connect the
> main board's 13.56 MHz HF/NFC transceiver to the MCU (see [NFC (13.56 MHz) Subsystem](#nfc-1356-mhz-subsystem) below).

---

## NFC (13.56 MHz) Subsystem

A 13.56 MHz HF-RFID / NFC transceiver is present on the main board; its IC identity is not
confirmed from schematic net labels alone.  The `NFC/` source directory and the `NFC_CS` /
`NFC_IRQ` signal pairs confirm the transceiver is SPI-connected and interrupt-driven.

---

## Display

- **LCD1**: 128×64 dot matrix LCD, 1/65 duty, 1/9 bias
- Interface connector: **J2** (LCD interface)
- SPI-connected: `DISPLAY_CS`, `DISPLAY_RST`, `DISPLAY_D1`, `DISPLAY_SCK`
- No touch interface signals visible in schematic sheets

---

## SD Card Storage

- Connector: **J4** (10-pin Micro SD card interface)
- **SDIO 4-bit** interface: `SDIO_CK`, `SDIO_CMD`, `SDIO_D0`–`SDIO_D3`
- Card-detect pin: `SD_DETECT`

---

## User Input

Six navigation buttons connected to MCU GPIOs:

| Ref | Signal |
|-----|--------|
| S1 | `BUTTON_BACK` |
| S2 | `BUTTON_OK` |
| S3 | `BUTTON_RIGHT` |
| S4 | `BUTTON_LEFT` |
| S5 | `BUTTON_UP` |
| S6 | `BUTTON_DOWN` |

`BUTTON_OK` (S2) also serves as `QON` / ship-mode wake input to the power IC (U3).

**S2 (IR):** separate switch on the IR circuit for enable/test (shares S2 designator with a
different schematic sheet).

---

## USB

- **USB-C connector** (**J1**) with dedicated **USB PD controller** (**U5** main)
- `VBUS` present; USB PD negotiation capable (voltage/current profiles beyond 5 V possible)
- MCU USB signals route through `VDDUSB` rail (isolated LDO)
- VBUS diode: **D1** (main board)

---

## IR (Infrared) Subsystem

| Ref | Description |
|-----|-------------|
| U6 | IR receiver IC |
| LD1 | Infrared LED (I_IR = 58 mA) |
| LD2 | Infrared LED (I_IR = 58 mA) |
| Q3 | MOSFET — IR LED driver (58 mA switch) |

Both IR LEDs (LD1 and LD2) are driven by Q3 in parallel or in sequence; peak current 58 mA
per LED.

---

## LED Indicators

| Ref | Description |
|-----|-------------|
| D11 | RGB LED (3-color) |
| IC1 | I2C LED driver IC (OUT0–OUT3, e.g. IS31FL or similar) |

IC1 controls the RGB indicator LED and potentially additional LEDs via I2C.

---

## Power Subsystem

### Battery
- **Li-Ion 3.7 V / 2100 mAh** with protection PCB — 3-pin connector (`BAT+`, `BTEMP`, `BAT-`)
  on **U8 (bat)** footprint
- `BTEMP` (NTC thermistor input) connected to charger IC → hardware over-temperature protection
- Over-temperature flags: `BAT_OTC` (charge), `BAT_DTC` (discharge)

### Battery Charger (U3)
- Pin-compatible with **BQ25895** or equivalent (BTST, PMID, PSEL, STAT, PG, BAT, REGN, OTG, CE)
- **OTG-capable** — device can act as a USB power source (5 V output on VBUS)
- Communicates over I2C (`I2C_SDA`, `I2C_SCL`); interrupt on `I2C_INT`
- `LO_BAT_INT`: low-battery interrupt to MCU
- Inductor: **L4** (power path / charger output)

### Battery Fuel Gauge (U4)
- Pin-compatible with **BQ27xxx** series (GPOUT, BIN, SCL, SDA, SRX, BAT)
- Reports state-of-charge, remaining capacity, temperature, and cycle count over I2C
- `GPOUT`: programmable alert output to MCU

### Voltage Regulators
| Rail | IC | Output | Inductor | Notes |
|------|----|--------|----------|-------|
| +3V3 (MCU core) | LDO **U11** | 3.3 V | — | MCU backup / always-on path |
| EXT3V3 | Buck-boost **U7** + current-limit **U8** | 3.3 V | **L1** | Switchable via `EN_EXT_3V3`; 1.5 A converter, 1 A short-circuit limit |
| 5V (internal) | Boost **U9** | 5 V | **L2** | Internal 5 V rail; 1.5 A converter |
| 5V_EXT | **U10** (current-limit) | 5 V | — | Switchable via `EN_EXT1_5V` / `EN_EXT2_5V`; 1 A short-circuit limit |

The two independent 5 V enable lines (`EN_EXT1_5V`, `EN_EXT2_5V`) suggest two separately
switched 5 V outputs (e.g. for different accessory ports).

### Power Rails Summary
| Rail | Description |
|------|-------------|
| `VBUS` | USB input power |
| `SYS` | System power (charger output) |
| `+3V3` / `3V3` | Main 3.3 V logic |
| `+3V3_RF` | RF-filtered 3.3 V (Sub-GHz transceiver) |
| `EXT3V3` | External 3.3 V (current-limited, switchable) |
| `5V` | Internal 5 V boost |
| `+5_EXT` | External 5 V (current-limited, switchable) |
| `A3_3V` | NFC analog 3.3 V |
| `1_65` | NFC 1.65 V mid-rail reference (op-amp biasing) |
| `AGND` / `DGND` | Analog / digital ground (split planes) |

### I2C Bus
Shared bus hosts at minimum: battery charger (U3), fuel gauge (U4), and LED driver (IC1).
Interrupt-driven via `I2C_INT` to MCU.

---

## External Connectors & Headers

| Ref | Label | Pins | Signals |
|-----|-------|------|---------|
| J5 | X8 MONSTATEK | 8 | SPI4, I2C4, `+5_EXT` |
| J6 | RF Board Interface | 26 | Full RF board signal bus |
| J7 | X10 MONSTATEK | 10 | SWD, UART1, ADC, FDCAN, `EXT3V3` |
| J4 | Micro SD | 10 | SDIO 4-bit + card-detect |
| J2 | LCD Interface | — | Display SPI + control |
| J1 | USB-C | — | USB 2.0 + PD |
| PLUG1 | NFC Antenna | 4 | ANT1, ANT2 + NFC board |
| — | SWD Debug Header | 5 | `SWO`, `SWCLK`, `SWDIO`, `/RST`, `GND` |
| — | USB-UART Header | 4 | `ESP_BOOT`, `RST`, `RX`, `TX` |

**J6 (RF Board, 26-pin):** carries SPI lines to Si4463, antenna switch controls (SW1/SW2),
strobe/refresh lines (RFSH1/RFSH2), GPIO, and power.  ESD protection diode array covers the
connector lines (Q2–Q18 area).

**J7 (X10):** exposes FDCAN bus in addition to the more common SWD/UART/ADC — indicates the
MCU's CAN-FD peripheral (`FDCAN1` or `FDCAN2`) is pinned out for accessories.

---

## Capability Summary for Firmware Support Assessment

| Capability | Hardware Evidence | Official Firmware Support |
|------------|-------------------|--------------------------|
| 125 kHz LF-RFID read | RF carrier oscillator (U2) + comparator chain (U3-A/B, U4-A/B) + `RFID_OUT` | Yes (lfrfid/) |
| 125 kHz LF-RFID emulate | Q4 load-modulation MOSFET + `RFID_PULL` | Yes |
| 13.56 MHz NFC/HF-RFID | NFC_CS / NFC_IRQ signals + NFC/ source dir | Yes (partial) |
| Sub-GHz TX/RX | Si4463 transceiver (J6 RF board) | Yes (Sub_Ghz/) |
| WiFi / BT | ESP32 module (M1) | Yes (Esp_spi_at/) |
| SD card storage | J4 SDIO interface + SD_DETECT | Yes |
| USB CDC/MSC | J1 USB-C + PD controller (U5) | Yes |
| USB OTG (host power) | Charger OTG pin (U3) | Not exposed in stock firmware |
| Battery % / health | BQ27xxx fuel gauge (U4) | Partial (BQ27 not fully exercised) |
| Battery temperature | BTEMP + charger NTC input (U3) | Not exposed to user |
| Switchable 3.3 V output | EXT3V3 rail + U7/U8 + EN_EXT_3V3 | Partial |
| Switchable 5 V output(s) | +5_EXT rail + U9/U10 + EN_EXT1/2_5V | Partial |
| IR transmit | LD1/LD2 + Q3 driver | Yes (IR/) |
| IR receive | U6 IR receiver IC | Yes (IR/) |
| RGB LED | D11 + IC1 I2C LED driver | Yes |
| USB PD negotiation | USB PD controller (U5) | Not exposed in stock firmware |
| External UART accessory | J7 (X10) UART1 header | Not exposed in stock firmware |
| External CAN-FD | J7 (X10) FDCAN header | **Hapax: CAN Commander** (Sniffer, Send) — requires external transceiver |

---

## Passive Components

Approximately **130+ resistors** and **100+ capacitors** across all 7 sheets.  Individual
values are not listed here; the groupings below identify functional purpose.

### Resistors (key groupings)

| Sheet / Area | Refs | Function |
|---|---|---|
| NFC board | R4–R31, R39, R42–R45 | Bias, gain, signal conditioning, pull |
| Main MCU | R1, R3, R7, R8, R23, R62, R63, R69–R71, R88, R92, R94, R98–R103 | Pull-up/down, boot, USB, reset |
| Buttons / GPIO | R11–R22, R24–R33, R43–R48, R50–R57, R104 | Button pull-ups, series, ESD |
| Peripherals | R27–R42, R49, R67–R68, R72–R73, R80, R84, R90–R91, R93, R101 | LCD, SD, IR, RF interface |
| Power | R58–R60, R64–R66, R74–R79, R81–R82, R85, R87, R96, R107 | Charger, converters, fuel gauge |

### Capacitors (key groupings)

| Sheet / Area | Refs | Function |
|---|---|---|
| NFC board | C34, C36–C42, C44–C45, C47–C50, C59, C61, C69, C80–C81, C91–C92 | RF filtering, decoupling |
| Main MCU | C1, C2, C7–C11, C36, C61, C68 | USB, clock, decoupling |
| Buttons / GPIO | C3, C4, C25, C62, C63 | Decoupling, supervisor |
| Peripherals | C12, C13, C26–C29, C37, C39 | LCD, SD, IR, RF decoupling |
| Power | C14–C35, C40–C54, C57, C59–C60 | Bulk, converter, charger filtering |

### Inductors

| Ref | Function |
|-----|----------|
| L1 | Buck-boost inductor (EXT3V3, U7) |
| L2 | Boost inductor (5V internal, U9) |
| L4 | Power path inductor (charger output, U3) |
| L10 (NFC) | Inductor (NFC power filter) |

---

## Test Points

| Sheet | Refs |
|-------|------|
| NFC board | TP3, TP11, TP12, TP13, TP15 |
| Main MCU | TP8 |

---

## Mounting Hardware

- Main board: **H1, H2, H3, H4** (mounting holes)
- Mirrored set: **MH1, MH2, MH3, MH4** (mounting holes)
