**Sub-GHz: KeeLoq family in the Create-from-scratch catalog (Phase 8c-1)** —
  Extended the pure-logic protocol catalog in `Sub_Ghz/subghz_create_proto.c/h`
  from 17 to 20 entries by adding KeeLoq 433, Star Line 433, and Jarolift 433.
  Unlike the existing entries which expose a single opaque hex key, these
  KeeLoq-cipher counter-mode protocols advertise discrete `FIELD_SERIAL`,
  `FIELD_BUTTON`, `FIELD_COUNTER`, and `FIELD_MFKEY` flags — the foundation
  for the per-field editor scenes coming in Phase 8c-2/3.  Adds new
  `serial_bits` / `button_bits` / `counter_bits` metadata so the upcoming
  editor cursors and the encoder assembler can size and mask user-entered
  values correctly.  Host-tested only — no firmware behavioural change yet.
