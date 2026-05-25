**Sub-GHz: Phase 9e-2..5 deferral rationale corrected** — the
  `PHASE_CHECKLIST.md` entries for Nice FloR-S, CAME Atomo, Alutech
  AT-4N, and Phoenix V2 counter editing previously cited a
  "GPL-3.0 licensing implication" against porting Flipper/Unleashed
  reference code into M1.  This was incorrect: the M1 firmware is
  itself distributed under GPL-3.0 (see `LICENSE` / `COPYING.txt`),
  so importing GPL-3.0 reference code from Flipper Zero / Unleashed /
  Momentum is fully licence-compatible.  Each deferred sub-phase has
  been re-cast in terms of its real engineering blocker — missing
  decoder field decomposition, missing cipher implementation,
  key-recovery complexity, or undocumented checksum — with a
  consolidated licensing note documenting the correction.
