**ESP32 runtime capability layer** (`m1_esp32_caps.h/c`) — replaces compile-time
  `#ifdef` gating with a transport-agnostic `M1_ESP32_CAP_*` bitmap probed at
  first use.  Two probes are tried in order: binary `CMD_GET_STATUS` (opcode
  `0x02`, answered by SiN360 and future binary-SPI firmware) then stock
  `AT+CMD?` (answered by all tracked AT firmware variants — bedge117, dag,
  neddy299 — without any custom extension).  The resulting `uint64_t` bitmap
  uses one bit per named capability regardless of transport; 17 bits are
  assigned (WiFi scan / station scan / BLE scan & advertise / deauth / beacon /
  probe flood / Karma / packet monitor / portal / WiFi join / MAC spoof /
  channel override / network scanners / BLE HID / BT manage / 802.15.4), 47
  reserved.  If both probes fail the bitmap stays at zero (fail-closed).  The
  cache persists for the lifetime of the STM32 firmware — capabilities are a
  property of the ESP32 firmware variant and cannot change across a routine
  deinit/init cycle.  Call sites use `m1_esp32_require_cap()` / `m1_esp32_has_cap()`;
  WiFi and Bluetooth scenes no longer carry `#ifdef M1_APP_BADBT_ENABLE` /
  `#ifdef M1_APP_BT_MANAGE_ENABLE` guards.  The `esp32_version` RPC field is
  populated from the runtime firmware name reported by the probe.
