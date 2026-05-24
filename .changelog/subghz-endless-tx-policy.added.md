Sub-GHz: `subghz_endless_tx` TX repeat / endless-hold policy state machine —
  models SINGLE-mode N-burst transmissions and ENDLESS-mode hold-OK
  continuous TX with graceful N-repeat finalisation on release.  Pure-logic
  foundation for the upcoming Transmitter scene (Phase 3b).  Phase 3a of the
  Momentum parity migration; 19 host tests under ASan+UBSan covering every
  legal transition, abort paths, debounce edge cases, and realistic
  KeeLoq / remote burst sequences.
