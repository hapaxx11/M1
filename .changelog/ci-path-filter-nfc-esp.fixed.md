**Tests: add missing CI path filters for NFC and Esp_spi_at** — `test_nfc_poller`
  (sources in `NFC/`) and `test_esp_queue` (sources in `Esp_spi_at/`) were not
  triggering on CI when their source directories changed. Added `NFC/**` and
  `Esp_spi_at/**` to the `tests.yml` path filter. Also documented the CI path
  filter maintenance rule in `DEVELOPMENT.md` and `CLAUDE.md` so future agents
  keep the filter in sync when adding new tests.
