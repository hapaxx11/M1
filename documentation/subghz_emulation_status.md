<!-- See COPYING.txt for license details. -->

# Sub-GHz Protocol Emulation Status

This document tracks every **Dynamic (rolling-code)** protocol implemented in the
M1 firmware, recording its current emulation capability, the technical reason for
any limitation, and where to look for new information that might unlock emulation
in the future.

**Last reviewed:** 2026-04-20

---

## Quick Summary

| Status | Count | Notes |
|--------|-------|-------|
| ✅ **Replayable** — plain OOK PWM, no cipher | 9 | No key material needed; direct OOK PWM re-encode |
| 🔑 **Replayable with manufacturer key** | 3 | Requires `0:/SUBGHZ/keeloq_mfcodes` on SD card |
| 🔒 **Custom path — special encoder** | 1 | Security+ 1.0 ternary brute-force counter mode |
| ❌ **Not emulatable** — cipher or protocol barrier | 8 | See per-protocol notes below |

---

## ✅ Replayable — Plain OOK PWM (no cipher needed)

These protocols use simple counter-increment or fixed rolling-code counters with no
encryption.  The M1 firmware re-encodes them from the stored key value + TE timing
directly via `subghz_key_resolve_timing()` + `subghz_key_encode_pwm()`.
All carry `SubGhzProtocolFlag_PwmKeyReplay` in the registry.

| Protocol | Bits | Freq | Notes |
|----------|------|------|-------|
| **CAME TWEE** | 54 | 433 MHz | Counter in upper byte; no crypto |
| **CAME Atomo** | 62 | 433 MHz | Counter in upper bits; no crypto |
| **Nice FloR-S** | 52 | 433 MHz | Simple pseudo-random sequence; publicly documented |
| **Alutech AT-4N** | 72 | 433 MHz | Counter-based; no crypto |
| **KingGates Stylo4k** | 60 | 433 MHz | Counter-based OOK PWM; no crypto |
| **Scher-Khan Magicar** | 64 | 433 MHz | OOK PWM counter; no crypto |
| **Scher-Khan Logicar** | 64 | 433 MHz | OOK PWM counter; no crypto |
| **Toyota** | 64 | 433 MHz | OOK PWM rolling counter; no crypto |
| **DITEC_GOL4** | 40 | 433 MHz | OOK PWM counter-based; no crypto |

---

## 🔑 Replayable with Manufacturer Key (KeeLoq cipher family)

These three protocols all use the **KeeLoq NLFSR block cipher** for hop-word
encryption.  The device key is derived from the serial number plus the
manufacturer's 64-bit master key.  M1 implements full counter-mode replay:
decrypt the captured hop word → increment the 16-bit rolling counter →
re-encrypt → transmit as OOK PWM.

**Keystore file:** `0:/SUBGHZ/keeloq_mfcodes` on the SD card.

**Supported file formats** (both may coexist in the same file):

```
# Compact format (one entry per line)
AABBCCDDEEFFAABB:2:ManufacturerName    # type 2 = Normal Learning
AABBCCDDEEFFAABB:1:ManufacturerName    # type 1 = Simple Learning

# RocketGod multi-line format (as exported by the SubGHz Toolkit for Flipper Zero)
Manufacturer: ManufacturerName
Key (Hex):    AABBCCDDEEFFAABB
Key (Dec):    <decimal — ignored>
Type:         2
------------------------------------
```

Use `scripts/convert_keeloq_keys.py` to convert RocketGod output to compact format.

| Protocol | Bits | Cipher / Learning | Notes |
|----------|------|-------------------|-------|
| **KeeLoq** | 64 (Flipper) | KeeLoq; Normal or Simple Learning | Most common garage remote standard in EU/NA |
| **Star Line** | 64 (Flipper) | KeeLoq; Normal or Simple Learning | Russian car alarm OEM variant |
| **Jarolift** | 64 (Flipper) | KeeLoq; Normal or Simple Learning | Roller shutter / blind remote |

> **Note on bit widths:** M1 native captures produce 66-bit (KeeLoq) or 72-bit (Jarolift)
> frames.  The counter-mode encoder only handles the 64-bit **Flipper-format** layout.
> Use a Flipper Zero to capture `.sub` files with the `Manufacture:` field populated —
> these import and replay correctly via `sub_ghz_replay_flipper_file()`.

### Where to get manufacturer keys

1. **RocketGod's SubGHz Toolkit** (Flipper Zero app store) — exports a `keeloq_mfcodes`
   file containing all community-sourced manufacturer keys in the format M1 already
   parses.  This is the primary source.
   - App: `Applications > Sub-GHz > SubGHz Toolkit`
   - Output file: `SD:/subghz/assets/keeloq_mfcodes`
   - Copy to M1 SD card at: `0:/SUBGHZ/keeloq_mfcodes`

2. **Flipper Zero Unleashed firmware** keystore:
   - [`flipperdevices/flipperzero-firmware` — `assets/subghz/keeloq_mfcodes`](https://github.com/flipperdevices/flipperzero-firmware)
   - [`DarkFlippers/unleashed-firmware` — `assets/subghz/keeloq_mfcodes`](https://github.com/DarkFlippers/unleashed-firmware)
   - [`Next-Flip/Momentum-Firmware` — `assets/subghz/keeloq_mfcodes`](https://github.com/Next-Flip/Momentum-Firmware)

3. **Community databases** — search GitHub for `keeloq_mfcodes` to find community-curated
   expansions with 100+ manufacturer entries covering BFT, CAME, Nice, Faac (non-SLH),
   Roger, Beninca (non-ARC), Aprimatic, Ditec, Gibidi, Doorhan, AEHS, Allmatic, Erreka,
   Mpower, Marantec (KeeLoq-variant products), etc.

---

## 🔒 Custom Encoder Path

| Protocol | Freq | Approach | Status |
|----------|------|----------|--------|
| **Security+ 1.0** | 315/390 MHz | Ternary encoding (0/1/2 symbols); brute-force counter mode | ✅ Implemented — `Sub_Ghz/subghz_secplus_v1_encoder.c`; the saved counter value is incremented and all three ternary symbols are tried in sequence |

---

## ❌ Not Emulatable (current state, 2026-04-20)

### Security+ 2.0
- **Modulation:** FSK, 303/315/390 MHz
- **Barrier:** Bidirectional challenge-response.  The opener sends a challenge; the remote
  must sign it with its private key.  There is no static "next counter" to predict — the
  response changes every transmission in a way that requires the remote's secret.
- **Reference:** [`argilo/secplus`](https://github.com/argilo/secplus) — full protocol
  documentation and Python implementation; confirms bidirectional requirement.
- **What to watch:** If the challenge-response mechanism is ever bypassed via a known
  plaintext attack or if a static-seed variant is discovered, check argilo/secplus
  issue tracker and `DarkFlippers/unleashed-firmware` protocol commits.

---

### FAAC SLH (SelfLearning Hopping)
- **Modulation:** Manchester-encoded OOK, 433.92/868 MHz, 64 bits
- **Packet layout:** bits 0–31 = encrypted rolling code; bits 32–59 = 28-bit serial;
  bits 60–63 = button code
- **Barrier:** Proprietary stream cipher.  The encryption is applied to the rolling code
  portion using a per-remote seed.  Unlike KeeLoq, there is no documented manufacturer
  master key — each remote derives its own stream cipher state during the learning
  procedure with the operator.  The algorithm has not been publicly documented as of
  this review date.
- **What to watch:**
  - [`DarkFlippers/unleashed-firmware`](https://github.com/DarkFlippers/unleashed-firmware) —
    check `lib/subghz/protocols/faac_slh.c` for any new decryption fields.
  - [`flipperdevices/flipperzero-firmware`](https://github.com/flipperdevices/flipperzero-firmware) —
    upstream protocol commits.
  - Academic conference proceedings (Black Hat EU, USENIX Security) for RF rolling code
    research — FAAC SLH has been studied but the cipher has not been publicly broken.
  - If a `Seed:` or `MasterKey:` field ever appears in Flipper `.sub` files for FAAC SLH,
    that would indicate a replay path has been found.

---

### Somfy Telis (RTS)
- **Modulation:** Manchester-encoded OOK + XOR obfuscation, 433.42 MHz, 56 bits
- **Packet layout:** enc_key (4b) | checksum (4b) | command (8b) | rolling_code (16b) | address (24b)
- **Barrier:** Per-remote rolling code with no manufacturer master key.  The rolling code is
  seeded during pairing and never repeats.  The XOR obfuscation is publicly known (see
  Flipper source) but is not the barrier — the barrier is that the receiver only accepts
  the *next expected* code from a specific paired address.  Re-transmitting a captured
  code will work exactly once; predicting the next code requires knowing the 16-bit counter
  state, which increments unpredictably from the attacker's perspective.
- **What to watch:**
  - [`Nickfost/somfy-rts-protocol`](https://github.com/Nickfost/somfy-rts-protocol) and
    related RTS research — the XOR table is fully documented but no counter-predict attack
    exists.
  - If a static "test mode" code or factory reset sequence is discovered, it could allow
    pairing a cloned remote.  Watch Flipper community forums.

---

### Somfy Keytis (RTS variant)
- **Modulation:** Manchester-encoded OOK, 433.42 MHz, 80 bits
- **Barrier:** Same architecture as Somfy Telis (RTS) — per-remote paired rolling code;
  no manufacturer master key.  Longer frame but identical cryptographic barrier.
- **What to watch:** Same sources as Somfy Telis above.

---

### Revers_RB2
- **Modulation:** Manchester-encoded OOK, 433.92 MHz, 64 bits
- **Barrier:** Rolling code with no publicly documented decryption algorithm.  The protocol
  is used by Revers (Eastern European gate/barrier brand).  Momentum firmware includes
  a decoder but no encoder.
- **What to watch:**
  - [`Next-Flip/Momentum-Firmware`](https://github.com/Next-Flip/Momentum-Firmware) —
    `lib/subghz/protocols/revers_rb2.c` — watch for an encoder being added.
  - GitHub search: `Revers_RB2 encoder` or `Revers gate replay`.

---

### KIA Seed
- **Modulation:** OOK PWM, 433.92 MHz, 61 bits
- **Barrier:** Non-standard encoding — preamble detection requires >15 short pairs followed
  by a long+long start bit.  The rolling code uses a seed-based PRNG that is not
  compatible with the standard OOK PWM key encoder.  The seed derivation algorithm has
  not been publicly documented.
- **What to watch:**
  - Automotive RF security research — KIA/Hyundai remote entry has been studied by
    academic groups.  Search: `KIA remote entry rolling code attack` in IEEE/USENIX
    proceedings.
  - [`DarkFlippers/unleashed-firmware`](https://github.com/DarkFlippers/unleashed-firmware)
    protocol commits for `kia_seed`.

---

### Beninca ARC
- **Modulation:** OOK PWM, 433.92 MHz, 128 bits
- **Barrier:** AES-128 encrypted rolling code.  The 128-bit frame contains the AES
  ciphertext; decryption requires the Beninca manufacturer AES key, which is not
  publicly available.  The decoder extracts the unencrypted serial and counter fields
  from the frame header without performing AES decryption.
- **What to watch:**
  - [`Next-Flip/Momentum-Firmware`](https://github.com/Next-Flip/Momentum-Firmware) —
    `lib/subghz/protocols/beninca_arc.c`.  If Momentum ever adds a `MasterKey:` field
    and encoder, a similar approach could work here.
  - Academic research: `Beninca ARC AES key extraction` — side-channel attacks on the
    remote hardware could expose the key.

---

### Hörmann BiSecur
- **Modulation:** Manchester-encoded OOK (IEEE 802.3 biphase), **868 MHz**, 176 bits
- **Barrier:** AES-128 encrypted, 868 MHz band.  The 22-byte frame is AES-encrypted;
  the manufacturer key has not been publicly documented.  The decoder captures the raw
  frame and extracts the unencrypted message type byte.
- **What to watch:**
  - [`Next-Flip/Momentum-Firmware`](https://github.com/Next-Flip/Momentum-Firmware) —
    `lib/subghz/protocols/hormann_bisecur.c`.
  - Hörmann SDK / installer manuals sometimes leak key material; search:
    `Hormann BiSecur SDK` or `BiSecur AES key`.
  - Note: M1 hardware supports 868 MHz — the radio configuration and decode path are
    already in place.  Only the AES key material is missing.

---

## Research Resources

| Resource | URL | Notes |
|----------|-----|-------|
| argilo/secplus | https://github.com/argilo/secplus | Security+ 1.0/2.0 full documentation |
| Flipper firmware | https://github.com/flipperdevices/flipperzero-firmware | Primary upstream for protocol decoders |
| Unleashed firmware | https://github.com/DarkFlippers/unleashed-firmware | Largest extended protocol set |
| Momentum firmware | https://github.com/Next-Flip/Momentum-Firmware | Another major fork; often first to add new encoders |
| Flipper forum | https://forum.flipper.net | Community discussion on RF protocols |
| Signal ID Wiki | https://www.sigidwiki.com | Signal identification for unknown protocols |
| RTL-SDR Blog | https://www.rtl-sdr.com | RF reverse engineering news |
| RocketGod SubGHz Toolkit | Flipper app store | KeeLoq manufacturer key exporter |
| GitHub topic: keeloq | https://github.com/topics/keeloq | Community KeeLoq tools |
| GitHub topic: rolling-code | https://github.com/topics/rolling-code | Broader rolling code research |

---

## Update Checklist (for each review)

When reviewing this document (suggested: each time a new Sub-GHz protocol is added or
when a new Flipper/Momentum firmware release is published):

1. **Check Momentum `lib/subghz/protocols/`** — look for new `*_encoder.c` files for any
   protocol listed as ❌ above.  If found, assess portability to M1.
2. **Check Unleashed firmware diff** since last review date — same directory.
3. **Update `keeloq_mfcodes`** on the SD card from RocketGod toolkit to pick up new
   manufacturer entries added by the community.
4. **Update the "Last reviewed" date** at the top of this file.
5. **Move any newly-emulatable protocol** from ❌ to the appropriate ✅ or 🔑 section.
