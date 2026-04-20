**Games: World Clock — local timezone setting** — the world clock now uses the
  user's configured UTC offset (Settings → LCD & Notifications → Local TZ) to
  correctly derive world-zone times from local RTC time.  The Local page now
  displays the configured timezone label (e.g. "Local UTC+5").  The offset is
  persisted in `settings.cfg` as `clock_tz_offset` and survives reboots.
