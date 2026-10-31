# Meshtastic for M1 Hapax

A Meshtastic client for the M1, adapted from
[donliggett/m1-meshtastic](https://github.com/donliggett/m1-meshtastic) and
ZeroMesh. It connects to a Meshtastic radio over the expansion-header UART.

The app provides live broadcast and direct messages, a node roster, RSSI/SNR and
GPS status, telemetry, debug logs, notification settings, and node configuration.
The normal Hapax firmware build produces `build/apps/meshtastic/m1-meshtastic.m1app`.

## Install

1. Use firmware containing the `m1_uart_app_*` and extended-button app exports.
2. Copy `m1-meshtastic.m1app` to `0:/apps/` on the SD card.
3. Launch **Meshtastic** from the M1 Apps menu.
4. On the Meshtastic node, enable the serial module in **PROTO** mode at **115200 baud**.

Settings are stored in `0:/apps_data/zeromesh/settings.cfg`.

## Wiring

USART1 uses 3.3 V logic. Connect grounds and cross TX/RX:

| Meshtastic node | M1 expansion header |
|---|---|
| TX | Pin 13 — PA10, M1 RX |
| RX | Pin 12 — PA9, M1 TX |
| GND | GND |

This is one pin off from a Flipper Zero UART connection; Flipper UART hats are not
drop-in. While the app runs, it owns USART1 and the USB-to-serial bridge does not
forward header-UART traffic. USB RPC continues to work. USART1 cannot be shared
with the external-IR option that reuses PA9/PA10; disable that option while using
Meshtastic.

## Controls

| Screen | Input | Action |
|---|---|---|
| Any | Left / Right | Change page |
| Messages | OK | Compose broadcast |
| Messages | Long OK | Cycle channel |
| Roster | Up / Down | Select node |
| Roster | OK | Open direct chat |
| Roster | Long OK | Node details |
| Chat | OK | Compose direct message |
| Logs | OK | Pause / resume |
| Settings | OK | Edit / finish editing |
| Settings | Left / Right while editing | Change value |
| Node Config | OK | Edit / apply |
| Node Config | Left / Right while editing | Change value |
| Node Config, Reboot | Long OK | Reboot node |
| Any | Back | Exit app or leave editing |

## Scope and validation

The offline vector map and Bluetooth transport from ZeroMesh are not included.
Setting a fixed position still requires the deferred map UI; clearing a fixed
position is supported. Custom SD ringtones are not included.

The app and its serial framing are build- and host-tested. On-hardware behavior
still requires validation with a real Meshtastic node, including UART timing,
display rendering, keyboard input, and node-configuration writes. Treat remote
configuration and reboot controls accordingly.

## Licensing

GPL-3.0, inherited from ZeroMesh and the Meshtastic protocol assets. Nanopb is
zlib-licensed. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
