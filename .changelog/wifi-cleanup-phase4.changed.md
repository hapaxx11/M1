# WiFi cleanup — Phase 4: attack consolidation & multi-target flow

- Collapsed Karma/Karma+Portal into a single **Karma** menu entry with an
  inline "Serve portal? [Yes] [No]" prompt (`wifi_attack_karma_with_portal()`).
  Attacks menu now has 8 items (was 9).
- Added pure-logic `wifi_multi_target_build()` helper to build a flat AP target
  list from the selected entries of any `wifi_ap_t[]` array (no HAL/RTOS deps;
  seeds from both live scan results and loaded saved lists).
- Added post-deauth chaining prompt to single-target deauth: after the attack
  completes the user is offered `[Handshake] [Evil Portal] [Done]` to chain
  immediately into EAPOL capture or Evil Portal (plan §3.8).
- Tests: `test_wifi_multi_target.c` (11 pure-logic cases) and
  `test_wifi_ux_restructure.c` extended to 31 source assertions.
