**Sub-GHz: move remaining malloc sites to FreeRTOS heap** — Converted all
  newlib `malloc`/`free` allocations in `m1_sub_ghz.c`, `subghz_keeloq_encoder.c`,
  and `subghz_keeloq_mfkeys.c` to `pvPortMalloc`/`vPortFree` (FreeRTOS heap),
  matching the pattern established in PR #521 for the ring/raw-samples buffers.
  Prevents newlib heap exhaustion on the live-decode replay, Flipper-to-tmp
  converter, raw-save formatter, and KeeLoq encoder/keystore paths.
