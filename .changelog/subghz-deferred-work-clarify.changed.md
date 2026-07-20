**Docs: clarify the Sub-GHz Momentum-parity "deferred work" section** — the
  `subghz-protocols` skill section (renamed "Remaining Sub-GHz Momentum-Parity
  Work") now lists only the two items still genuinely open (Phoenix V2 counter
  editing and the per-protocol Info-screen renderers) and drops the historical
  "promoted to SUPPORTED" / excluded-widget-candidate prose that had become
  confusing.

**Docs: correct outdated ESP32 AT-firmware capability list** — the
  `esp32-coprocessor` skill no longer claims deauth and evil portal are
  unsupported "AT implementations pending"; the neddy299 / dag AT forks provide
  deauth (and dag adds beacon spam, karma, evil portal, probe flood, and
  PMKID/handshake capture), all already wired via M1's AT dispatch.
