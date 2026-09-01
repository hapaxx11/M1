**Dashboard: RPC diagnostic wall-clock suffix no longer runs off-screen** — the
  System Dashboard's ESP32 RPC diagnostics page (5/5) drew the last feature
  call's trailing " tNs" wall-clock suffix (issue #719 Phase 6) in-line with
  the rest of the diagnostic line, which overflowed the 128px display and
  made it unreadable. The suffix is now split onto its own line.
