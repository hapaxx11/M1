# Brain ESP32 NTP clock sync on WiFi connect

`wifi_sync_rtc()` now handles the brain CD3 (M1_RPC) transport path.
When connected via `ESP32_TRANSPORT_RPC`, it issues a `SYS_SNTP_SYNC` RPC call
to the brain firmware (which queries `pool.ntp.org` internally) and applies
the returned UTC time to the M1 RTC — matching the NTP sync that the AT
firmware path has always performed on connect.
