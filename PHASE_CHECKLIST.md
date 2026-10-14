# Phase Checklist — Renault V1 Hitag2 IV brute-force (ProtoPirate port)

## PR Metadata
- **PR Title**: Add PSA two-phase TEA brute-force key recovery (ProtoPirate port)
  — extended with Renault V1 Hitag2 IV search per review request.

## Phases

### Phase R1 — Hitag2 IV search pure core + host tests
- **Description**: `Sub_Ghz/subghz_renault_v1_bruteforce.c/h` — verbatim port of
  upstream `hitag2_bf_prepare`/`hitag2_bf_rearrange_dest`, byte-oriented Hitag2
  cipher (`hitag2_filter`/`feedback`/`shift_state`/`shift_u32`/`clock_cipher`,
  `hitag2_serial_permute`), 2^18 candidate loop driving
  `subghz_bruteforce_job_t`.  Tests fabricate frames around a chosen candidate
  (closed-form shuffle inverse; both tail bits of frame[9] are provably unused)
  and assert found/exhaustion/cancel plus the recovered-counter-from-seed
  formula.
- **Status**: ✅ Complete
- **Commit**: `Add Renault V1 Hitag2 IV brute-force search core`

### Phase R1b — Renault V1 seed-path encoder (TX after recovery)
- **Description**: `Sub_Ghz/subghz_renault_v1_encoder.c/h` — pure port of
  `hitag2_build_iv` + `hitag2_encrypt_frame` (seed-based re-encryption with a
  fresh counter) and the 1295-entry upload builder (250 preamble pairs, 3 long
  frames, 3 short frames; TE=125 µs Manchester, header 1500/1000 µs).  Host
  tests: encrypt→BF round-trip recovers the same seed, upload shape/timings.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase R2 — Capture path: key_2 widening + Renault V1 decoder frame storage
- **Description**: `flipper_subghz_signal_t.key_2` becomes u64 + `key_2_len`
  (parses up to 8 hex bytes; PSA uses low 2, Renault low 3).  Renault V1
  decoder stores the full frame prefix: `n64_decodedvalue = raw[0..7]` BE and
  raw[8..10] in a new `n32_key2`/`key2` aux word (RAM affordable: ~86% used,
  ~92 KB free; +204 B total).  Save paths write upstream-compatible
  Key/Key_2 for Renault V1.  Update existing tests.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase R3 — Renault recovered write-back + tests
- **Description**: `flipper_subghz_save_renault_v1_recovered()` writes Key,
  Key_2, Serial, Btn, Cnt (full counter recovered from seed), Hop, Seed
  (4 hex bytes), Recovered: 01 — upstream field formats.  Host tests.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase R4 — Scene dispatch (PSA + Renault V1) + worker
- **Description**: The Proto Pirate Brute Force scene accepts both protocols,
  dispatches the matching search core on the worker, and shows protocol-apt
  results (PSA Ser/Btn/Cnt; Renault Seed/Cnt).
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase R5 — Build, RAM, docs, changelog, PR update
- **Description**: Firmware build + RAM check; update
  `documentation/subghz_emulation_status.md`, forks-tracker row, changelog
  fragment; remove checklist.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
