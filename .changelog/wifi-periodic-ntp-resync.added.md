**WiFi: automatic background NTP re-sync every 30 minutes** — while WiFi remains
  connected the RTC is silently re-synchronised once every 30 minutes via a lightweight
  single `AT+CIPSNTPTIME?` query (no UI, no retry loop).  The home-screen clock updates
  on its next 1-minute refresh.  The initial NTP sync at connect time also stamps the
  re-sync timer so the first background sync is deferred a full 30 minutes.
