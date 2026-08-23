**ESP32: last feature RPC call diagnostics (issue #719 Phase 2)** — A field
read-back confirmed the brain-CD3 M1_RPC probe fix (Phase 1): the dashboard now
reports a real firmware name and a non-zero, WIFI_SCAN-capable capability
bitmap. WiFi Scan still fails, but the probe's tiny single-frame PING/
GET_STATUS exchange never exercised the separate bulk-list M1 Link FRAG
reassembly path that WIFI_SCAN (and Station Scan / BLE Scan) actually use.
`m1_esp32_rpc_call()` — the single client every ESP32 feature dispatches
through — now records a snapshot of its last invocation (opcode, whether the
transport returned a matching frame, raw frame byte count, final status, and
decoded payload byte count), readable via `m1_esp32_rpc_get_call_diag()` /
`m1_esp32_rpc_call_diag_format()` and shown on a new Settings > Dashboard page
5/5 (e.g. `"op0103 no-reply st253 r0 p0"`), so the next WiFi Scan failure
report can name the exact failure mode instead of "still fails".
