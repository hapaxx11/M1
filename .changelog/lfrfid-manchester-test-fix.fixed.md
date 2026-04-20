**Tests: fix lfrfid_manchester test infrastructure** — correct `lfrfid_evt_t` stub
  to use `uint16_t edge` matching the production struct; add `lfrfid/**` to the
  `tests.yml` CI path filter so changes to LFRFID source files automatically
  trigger the unit test suite.
