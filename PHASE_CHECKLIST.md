# Phase Checklist — ProtoPirate TX encoders (PSA / Kia V6 / VAG)

## PR Metadata
- **PR Title**: Add ProtoPirate TX encoders: PSA (0x23/0x36), Kia V6, VAG
- **PR Description**: Ports the ProtoPirate encoder/transmit paths for the
  protocols M1 already decodes, completing the brute-force story: recovered
  captures become transmittable with counter advance.

## Remaining-gap context (upstream inventory 2026-10-14)
- Encoders exist upstream for 23/26 protocols; M1 has none (hex-key Emulate
  only).  This branch covers the three M1 already has full crypto for:
  PSA (TEA + direct-XOR), Kia V6 (AES + injected keystore), VAG (AUT64/TEA +
  injected keystore).
- Not in scope (documented follow-ups): real decoders for the 12 name-only
  placeholder protocols, Remote Analyzer tool, saved-capture match/update
  flow, models DB, Renault Hitag2 full-key path (no public key source).

## Phases

### Phase T1 — PSA encoder (0x23 + 0x36) + TX in BF scene
- **Description**: `Sub_Ghz/subghz_psa_encoder.c/h` — pure port of
  `psa_build_buffer_mode23` (inverse XOR network + nibble-search validation),
  `psa_build_buffer_mode36` (TEA encrypt with serial-derived working key) and
  the 325-entry upload builder (80 preamble pairs @250µs, transition
  250/500/250, Manchester MSB-first, 1000µs end).  Host tests: 0x23
  encode→`m1_psa_parse_frame` decode round-trip; 0x36 encode→brute-force
  crack round-trip (search recovers `0x23|serial` and the original fields);
  upload shape.  Scene: Emulate (RIGHT) for PSA found/saved, temp .sgh →
  Read Raw handoff (same pattern as Renault V1).
- **Status**: 🔄 In progress
- **Commit**: _(pending)_

### Phase T2 — Kia V6 encoder + TX
- **Description**: AES-128 payload encrypt (tiny_aes `AES_ECB_encrypt`) +
  144-bit Manchester upload builder (1928 entries, TE 200µs, bit-inverted on
  air), gated on the injected keystore availability flags.  Needs verbatim
  upstream `kia_v6.c` encoder extraction first.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase T3 — VAG encoder + TX
- **Description**: port AUT64 encrypt (upstream aut64.c has encrypt+decrypt;
  M1 has decrypt only), then the 4 frame-type upload builders.  Needs
  verbatim upstream extraction first.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase T4 — Build, RAM, docs, changelog
- **Description**: Firmware build + RAM check; emulation-status doc; forks
  tracker; changelog fragment; remove checklist before PR.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
