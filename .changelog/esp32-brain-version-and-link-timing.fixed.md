- **ESP32: report a parseable brain-CD3 firmware version to qMonstatek** — the
  native brain CD3 (`m1-esp32-brain`) advertises a bare `fw_name` (`"m1-native"`)
  in its GET_STATUS response that carries no dotted version, so qMonstatek's
  `parseVerNums()` found no `X.Y.Z` in the device-info `esp32_version` and
  declared the ESP "incompatible firmware" even though the M1 Link transport was
  fully working.  The CD3 detection probe now also issues
  `M1_RPC SYS_GET_FW_VERSION` (0x0003) and folds the returned semver into the
  cached firmware-name string (`"m1-native X.Y.Z[ <hash>]"`) via a new pure,
  host-tested helper (`m1_esp32_rpc_format_fw_version()`), mirroring how the C3
  reference firmware reports `"m1_link X.Y.Z <hash>"`.

- **ESP32: fix M1 Link poll budget and pacing for slow brain operations** — the
  full-duplex M1 Link transport gave up after a fixed 8 follow-up polls with no
  inter-poll pacing, and its per-transaction HANDSHAKE wait busy-spun without
  yielding.  Bulk-list operations (WiFi AP scan, station scan, BLE scan) keep the
  brain busy for ~1 s or more before it queues its pipelined RESP, so every such
  feature reliably timed out ("AP scan failed, please try again") even after
  detection succeeded.  The on-target transport now scales its poll budget from
  the caller's timeout, paces each poll on the slave's HANDSHAKE with a scheduler
  yield (avoiding an idle-task/IWDG starvation), and runs `HAL_SPI_Abort`
  self-heal after a failed transaction so a single desynced frame no longer
  wedges every later exchange — mirroring the proven C3 `m1_link` master.
