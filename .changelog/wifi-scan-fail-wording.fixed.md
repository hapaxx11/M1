**WiFi: fix broken-English scan-failure screen** — the WiFi access-point scan
  screen shown when no APs are found previously read "Scan AP" / "Failed. Let
  retry!".  It now reads "AP scan failed." / "Please try again.".  The wording
  is exposed as macros (`M1_WIFI_SCAN_FAIL_LINE_*` in `wifi_scan_fail_msg.h`) and
  covered by host-side unit tests.
