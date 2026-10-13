# Phase Checklist — PSA TEA Brute-Force (ProtoPirate port)

## PR Metadata
- **PR Title**: Add PSA two-phase TEA brute-force key recovery (ProtoPirate port)
- **PR Description**: Ports the ProtoPirate PSA brute-force feature: a pure,
  host-tested two-phase TEA key-search core driven by the generic
  `subghz_bruteforce_job` lifecycle, `Key_2` capture parsing/saving, decoder
  acceptance of TEA-mode (type 0x36) PSA frames, recovered-field writeback,
  and an async worker + Proto Pirate menu scene with progress UI.

## Scope notes (evidence-based)
- Upstream RocketGod-git/ProtoPirate has brute-force for **PSA only** (plus a
  Renault V1 Hitag2 IV search, not ported here). Kia V6 AES and VAG AUT64 have
  **no search upstream** — they use provisioned keys, which M1 already matches
  via build-injected keystores. No Kia/VAG search is implemented; docs updated
  to record parity.
- `SubGhzApp` is `static` and embeds 50× `SubGHz_Dec_Info_t`; growing that
  struct is forbidden (RAM at ~144 B free). Key2 for BF-pending frames rides
  in the existing `rolling_code` field (serial==0 marks a BF-pending PSA
  capture).

## Phases

### Phase 1 — PSA TEA brute-force pure-logic core + host tests
- **Description**: `Sub_Ghz/subghz_psa_bruteforce.c/h` — TEA scheduled
  encrypt/decrypt, phase-1 (0x23000000–0x23FFFFFF) and phase-2
  (0xF3000000–0xF3FFFFFF) candidate evaluation, capture byte packing, result
  extraction, ranged run driving `subghz_bruteforce_job_t`. Tests with
  synthetic captures built by forward encryption: phase-1 hit, phase-2 hit,
  exhaustion, cancel, empty range, result field extraction.
- **Status**: ✅ Complete
- **Commit**: `Add PSA two-phase TEA brute-force search core`

### Phase 2 — Capture parsing: `Key_2` in .sub parser/saver + decoder 0x36 acceptance
- **Description**: `flipper_subghz_signal_t` gains `key_2`/`has_key_2`; parser
  reads `Key_2:`; save variant writes it. PSA decoder accepts TEA-mode frames
  (marker nibble valid, direct-XOR path failed) and surfaces Key_2 via
  `rolling_code`; Read-scene save paths write `Key_2` for such captures.
  Host tests for parse/save round-trip and decoder acceptance.
- **Status**: ✅ Complete
- **Commit**: `Parse and persist PSA Key_2 captures for brute-force`

### Phase 3 — Result writeback (recovered-fields file)
- **Description**: Pure formatter for the recovered `.sub` content (Key,
  Key_2, Serial, Btn, Cnt, Type 0x36, Seed) + save helper; host tests.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 4 — Async worker + Proto Pirate Brute Force scene
- **Description**: New `SubGhzSceneProtoPirateBruteForce` scene: file browser
  pick → validate PSA capture with Key_2 → xTaskCreate worker (low priority,
  semaphore completion, cancel flag) → progress bar UI on scene tick →
  result screen → optional save of recovered file. Register scene enum,
  handlers, CMake; add 5th Proto Pirate menu item.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 5 — Firmware build, RAM budget, docs, changelog
- **Description**: Full firmware build + RAM headroom check; changelog
  fragment; update `documentation/subghz_emulation_status.md` and the
  forks-tracker ProtoPirate row; remove this checklist before PR.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
