**NFC: Poll profile support** — added `nfc_poll_profile_t` enum with
  `NFC_POLL_PROFILE_NORMAL` (all technologies) and `NFC_POLL_PROFILE_FAST_A`
  (NFC-A only with shorter 220ms discovery window) modes, plus
  `nfc_poller_set_profile()` / `nfc_poller_get_profile()` accessors.
  Ported from dagnazty/M1_T-1000.
