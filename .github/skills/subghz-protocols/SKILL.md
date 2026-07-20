---
name: subghz-protocols
description: Sub-GHz protocol reference: the four file-format types and two replay paths (.sub/.sgh taxonomy), rolling-code replay philosophy and feasibility registry, the Bind New Remote wizard rules, the 15-item Sub-GHz menu structure, and deferred Momentum-parity work. Load for any Sub-GHz protocol, replay, emulation, freq-preset, or bind-wizard work.
---

# Sub-GHz Protocols

> Extracted from CLAUDE.md. Load for any Sub-GHz protocol, replay, or bind work.
> See also documentation/subghz_emulation_status.md and documentation/flipper_import_agent.md.

### Sub-GHz File Format Taxonomy and Replay Paths

> **Every agent session that adds, modifies, or reviews Sub-GHz emulation or
> replay code MUST understand the four file types and the two replay functions.
> Conflating them is the single most common source of "Memory error" / "Emulate
> failed" bugs.**

#### File types

| File type | Extension | Filetype header | Version | Subtype field | Description |
|-----------|-----------|-----------------|---------|---------------|-------------|
| **M1 native NOISE** | `.sgh` | `M1 SubGHz NOISE` | `0.8` or `0.9` | — | Raw pulse recording by M1, C3.12, or SiN360 — **unsigned** durations on `Data:` lines |
| **M1 native PACKET** | `.sgh` | `M1 SubGHz PACKET` | `0.8` or `0.9` | — | Decoded/static key recorded by M1, C3.12, or SiN360 — uses `Payload:`, `Bits:`, `BT:`, `Modulation:` |
| **Flipper RAW** | `.sub` | `Flipper SubGhz RAW File` | `1` or `2` | `Protocol: RAW` | Raw pulse recording from Flipper — **signed** mark/space on `RAW_Data:` lines |
| **Flipper Key** | `.sub` | `Flipper SubGhz Key File` | `1` or `2` | any protocol | Decoded/static key from Flipper — uses `Key:`, `Bit:`, `TE:`, `Preset:` |

`flipper_subghz_load()` and `flipper_subghz_probe()` in `flipper_subghz.c`
detect the format automatically.  After loading, `flipper_subghz_signal_t`
carries two discriminator fields:

- **`type`** — `FLIPPER_SUBGHZ_TYPE_RAW` (NOISE / Flipper RAW) or `FLIPPER_SUBGHZ_TYPE_PARSED` (PACKET / Flipper Key)
- **`is_m1_native`** — `true` for `.sgh` files (M1 NOISE and PACKET), `false` for Flipper `.sub` files

#### The replay API — blocking wrappers and async primitives

**Blocking wrappers** (legacy one-shot replay callers):

| Function | Used for | What it does |
|----------|----------|--------------|
| `sub_ghz_replay_datafile(path, freq, mod)` | M1 native NOISE `.sgh` only | Sets up radio, points the streaming engine (`sub_ghz_raw_samples_init`) directly at the original `.sgh` file — **no temp file, no conversion**. Caller provides freq+mod from the already-loaded header. |
| `sub_ghz_replay_flipper_file(path)` | Flipper RAW `.sub`, Flipper Key `.sub`, M1 native PACKET `.sgh` | Reads the source file, **writes a temp `.sgh`** at `/SUBGHZ/_flipper_tmp.sgh`, then streams it. For RAW/NOISE types it strip-converts signed Flipper samples to unsigned M1 format. For Key/PACKET types it invokes the OOK PWM key encoder to synthesize a waveform. |

**Async prepare functions** (Read Raw scene):

| Function | Used for | What it does |
|----------|----------|--------------|
| `sub_ghz_replay_prepare_datafile(path, freq, mod)` | M1 native NOISE `.sgh` only | Populates streaming globals for the native file — same as the blocking wrapper but returns immediately without arming TX. |
| `sub_ghz_replay_prepare_flipper(sub_path, &out_tmp_path)` | Flipper RAW `.sub`, Flipper Key `.sub`, M1 native PACKET `.sgh` | Converts source to temp `.sgh`; returns the temp path via `out_tmp_path` for scene-local unlink. |
| `sub_ghz_replay_start_async()` | After either prepare call | Arms TIM1+DMA and returns immediately; completion events arrive as `Q_EVENT_SUBGHZ_TX`. |
| `sub_ghz_replay_continue_async(repeat)` | Read Raw scene `SubGhzEventTxComplete` handler | Advances streaming or tears down; returns `RUNNING`, `DONE`, or `ERROR`. |
| `sub_ghz_replay_abort()` | BACK / scene exit | Synchronous teardown; safe to call even after natural completion. |

The dispatch decision is encoded as a pure function so it is testable:

```c
/* flipper_subghz.h */
flipper_subghz_emulate_path_t
flipper_subghz_emulate_path(bool is_raw, bool is_m1_native);
// Returns DIRECT  when (is_raw && is_m1_native)  → use sub_ghz_replay_datafile()
// Returns CONVERT otherwise                       → use sub_ghz_replay_flipper_file()
```

#### Why M1 native NOISE should prefer the DIRECT path

In the current implementation, `sub_ghz_replay_flipper_file()` handles both
Flipper-style `RAW_Data:` and M1-style `Data:` lines, and sets `has_data`
accordingly.  If replay data or frequency is missing, it returns error code 2
("no data or missing frequency") rather than falling through to a memory-error
path.

M1 native NOISE should still prefer the DIRECT path via
`sub_ghz_replay_datafile()`, but for a different reason: it avoids an
unnecessary Flipper-format temp-file conversion step for data that is already
in the firmware's native representation.  That keeps the replay path simpler
and avoids edge cases around very long `Data:` payloads, including possible
truncation or line-continuation issues during conversion.  DIRECT is the
correct native fast path; CONVERT is retained for compatibility with non-native
inputs.

#### Emulate dispatch in the Saved scene (`m1_subghz_scene_saved.c`)

The Saved scene's `handle_action(SAVED_ACTION_EMULATE)` has two branches:

1. **RAW files** (`is_raw_file == true`, i.e. `type == RAW && raw_count > 0`):
   Pushes into the **Read Raw scene** in `SubGhzReadRawStateLoaded` state,
   passing replay metadata via `app->raw_load_path`, `app->raw_load_is_native`,
   `app->raw_load_freq_hz`, `app->raw_load_mod`.  When the user presses Send,
   the Read Raw scene calls `sub_ghz_replay_prepare_datafile()` (native `.sgh`)
   or `sub_ghz_replay_prepare_flipper()` (Flipper `.sub` RAW), then arms TX via
   `sub_ghz_replay_start_async()` — fully async with sine-wave animation and
   hold-to-repeat (LoadKeyTX / LoadKeyTXRepeat states).  This gives a waveform
   viewer with Send / New buttons rather than a blind one-shot inline replay.

2. **PACKET/Key files** (`is_raw_file == false`): Performs an inline blocking
   replay directly in the Saved scene by calling `sub_ghz_replay_flipper_file()`.
   This handles M1 native PACKET files, Flipper Key files, and Flipper RAW files
   that parsed as PACKET (e.g. because they decoded to a known protocol).
   After the call, `menu_sub_ghz_init()` restores radio state.

#### Emulate dispatch in the Playlist scene (`m1_subghz_scene_playlist.c`)

`playlist_transmit_next()` calls `flipper_subghz_probe()` (header-only, no
sample loading) then `flipper_subghz_emulate_path(probe.is_noise, probe.is_m1_native)`
to choose the replay function.  After every file, `menu_sub_ghz_init()` is called
to restore radio state before the next transmission.

#### Rules for new emulation code

1. **Never call `sub_ghz_replay_flipper_file()` on an M1 native NOISE `.sgh` file.**
   For blocking callers (Saved PACKET path, Playlist), check `is_m1_native && type == RAW`
   and use `sub_ghz_replay_datafile()` instead.  For the Read Raw async path, use
   `sub_ghz_replay_prepare_datafile()` + `sub_ghz_replay_start_async()`.
2. **Never call `sub_ghz_replay_datafile()` on a PACKET or Flipper `.sub` file.**
   For blocking callers, use `sub_ghz_replay_flipper_file()`.  For the Read Raw async
   path, use `sub_ghz_replay_prepare_flipper()` + `sub_ghz_replay_start_async()`.
3. **Always use `flipper_subghz_emulate_path()`** to encode the dispatch decision.
   Do not inline `if (is_m1_native && type == RAW)` in new scenes — call the shared
   function so tests catch any change to the dispatch rule.
4. **After any blocking call to `sub_ghz_replay_datafile()` or `sub_ghz_replay_flipper_file()`**,
   call `menu_sub_ghz_init()` before the radio is used again (both functions call
   `menu_sub_ghz_exit()` internally which powers off the SI4463).  The Read Raw scene
   uses the async path and restores radio state via `start_passive_rx()` — it does **not**
   call the blocking wrappers directly for its own TX.
5. **When loading metadata** for dispatch, prefer `flipper_subghz_probe()` (header only,
   low overhead) over the full `flipper_subghz_load()` when sample data is not needed
   (playlist, one-shot batch TX).  Use `flipper_subghz_load()` when the sample data
   or key fields are also needed (info screen, offline decode, saved scene).
## Rolling Code Protocol Replay Philosophy

> **The standing preference for this project is: if a rolling code protocol can be
> decrypted or replayed using publicly documented algorithms or known parameters,
> it MUST be implemented.**  Every agent session that adds, modifies, or reviews
> a rolling code protocol MUST research its replay feasibility before concluding
> that replay is impossible.  "Search high and low for sources."

### Replay Feasibility Registry (researched 2026-04-17)

The `SubGhzProtocolFlag_PwmKeyReplay` bit in `subghz_protocol_registry.h` is the
authoritative, flag-based gate for which Dynamic (rolling-code) protocols the standard
OOK PWM key encoder can replay.  **Never gate on protocol name strings — use the flag.**

| Protocol | Replay? | Reason | Flag set? |
|----------|---------|--------|-----------|
| **CAME Atomo** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **CAME TWEE** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Nice FloR-S** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Alutech AT-4N** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **KingGates Stylo4k** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Scher-Khan Magicar** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Scher-Khan Logicar** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Toyota** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **DITEC_GOL4** | ✅ Yes | OOK PWM, no crypto | ✅ |
| **Security+ 1.0** | ✅ Special | Ternary; brute-force counter mode in `m1_sub_ghz.c` | ❌ (custom path) |
| **KeeLoq** | ✅ Yes | OOK PWM; counter-mode with Normal/Simple Learning; requires manufacturer key (built-in or SD fallback) | ✅ (counter-mode encoder) |
| **Jarolift** | ✅ Yes | KeeLoq-based OOK PWM; requires manufacturer key (built-in or SD fallback); Flipper Bit:64 format fully supported | ✅ (counter-mode encoder) |
| **Star Line** | ✅ Yes | KeeLoq-based OOK PWM; requires manufacturer key (built-in or SD fallback); Flipper Bit:64 format fully supported | ✅ (counter-mode encoder) |
| **FAAC SLH** | ❌ No | Manchester-encoded; algorithm not fully public (2026-04-17) | ❌ |
| **Somfy Telis** | ❌ No | Manchester + proprietary XOR; no feasible OTA replay method | ❌ |
| **Somfy Keytis** | ❌ No | Same RTS protocol as Telis | ❌ |
| **Revers_RB2** | ❌ No | Manchester-encoded; no public decryption method | ❌ |
| **KIA Seed** | ❌ No | Special preamble/start-bit encoding; not standard OOK PWM | ❌ |
| **Security+ 2.0** | ❌ No | FSK + bidirectional challenge-response; replay not feasible | ❌ |
| **Hormann BiSecur** | ❌ No | Manchester + AES-128; no known feasible attack | ❌ |
| **Beninca ARC** | ❌ No | AES-128 rolling code; no known feasible attack | ❌ |

> **¹ KeeLoq note:** The KeeLoq cipher is fully implemented in `Sub_Ghz/subghz_keeloq.c`
> (528-round NLFSR, Normal Learning, Simple Learning, counter-mode increment).
> Manufacturer keys are embedded directly into the firmware at build time via
> `scripts/gen_keeloq_mfkeys_builtin.py` and the `KEELOQ_KEY_VAULT` GitHub Actions secret
> (Flipper-compatible approach — keys never appear on the SD card).
> When no built-in keys are present (public/CI builds without the vault), the firmware
> falls back to `0:/SUBGHZ/keeloq_mfcodes.enc` (AES-256-CBC encrypted) or the legacy
> plaintext `0:/SUBGHZ/keeloq_mfcodes` (auto-migrated to encrypted on first load).
> Once keys are available, `.sub` files with the `Manufacture:` field are replayed
> automatically via counter-mode: decrypt → increment → re-encrypt → transmit.

### Implementation Rules

1. **When adding a new Dynamic protocol**, research its replay feasibility immediately.
   Set `SubGhzProtocolFlag_PwmKeyReplay` only if the protocol uses plain OOK PWM with
   no cipher (just a counter increment).  If it uses a known reversible cipher with a
   public or partially-published key, add it to the ⚠️ Possible list and open a task.

2. **KeeLoq counter mode is implemented** (`Sub_Ghz/subghz_keeloq*.c/h`).  Manufacturer
   keys are embedded at build time via `scripts/gen_keeloq_mfkeys_builtin.py` and the
   `KEELOQ_KEY_VAULT` CI secret.  When no built-in keys are present, the firmware falls
   back to the SD card: `0:/SUBGHZ/keeloq_mfcodes.enc` (AES-256-CBC encrypted) or the
   legacy plaintext `0:/SUBGHZ/keeloq_mfcodes` (auto-migrated on first load).
   The counter-mode encoder:
   - Derives the device key via Normal or Simple Learning
   - Decrypts the captured hop word using the full KeeLoq cipher
   - Increments the 16-bit rolling counter
   - Re-encrypts and transmits as OOK PWM

3. **Security+ 1.0 brute-force counter mode** is already implemented in `m1_sub_ghz.c`.
   The dedicated ternary encoder lives in `Sub_Ghz/subghz_secplus_v1_encoder.c`.

4. **Never gate replay on name strings** — always use `SubGhzProtocolFlag_PwmKeyReplay`
   or a dedicated custom encoder path.  The fragile `strcasecmp(proto->name, ...)` guard
   has been replaced by the flag; do not reintroduce it.

5. **Manchester-encoded protocols** (FAAC SLH, Somfy Telis/Keytis, Revers_RB2) must
   NOT have `SubGhzProtocolFlag_PwmKeyReplay`.  A Manchester encoder would be needed
   before replay could be attempted, and the receiver algorithm must also be known.

6. **Research sources to check** when evaluating a new rolling code protocol:
   - **DarkFlippers/Unleashed SubGHzRemoteProg.md** — authoritative step-by-step binding
     procedures for all major rolling-code systems.  **Always consult this document before
     modifying or extending the Bind New Remote wizard:**
     `https://github.com/DarkFlippers/unleashed-firmware/raw/refs/heads/dev/documentation/SubGHzRemoteProg.md`
   - Flipper Zero firmware and Unleashed/DarkFlippers forks (Sub-GHz protocol handlers)
   - argilo/secplus (Security+ 1.0/2.0)
   - Academic papers: IEEE, USENIX, Black Hat EU proceedings on RFID/RF security
   - Hackaday, RTL-SDR Blog, forum.flipper.net
   - GitHub topics: `keeloq`, `rolling-code`, `subghz`, `rfhacking`
   - Signal identification wiki: sigidwiki.com
   - OpenSesame, RFCrack, GNU Radio out-of-tree modules

### New Protocol Checklist — Mandatory Before Merging Any Protocol Port

> **This checklist exists because of a real bug**: Magellan/GE/Interlogix North American
> security sensors operate at 319.5 MHz.  The M1 preset list skipped from 318.00 to
> 320.00, so the frequency could never be tuned to.  The Magellan protocol was also
> incorrectly flagged as 433-only despite having an NA variant at 319.5 MHz (315-band).
> The Flipper emulated .sub files correctly but M1 could never receive them — a bug that
> looks like a timing or decoder problem but is actually a missing frequency preset.
>
> **Run `ctest --test-dir tests/build-tests -R subghz_freq_presets` after every protocol
> addition.  The structural invariants will catch missing presets, sort errors, and
> count mismatches before the firmware is even built.**

When porting any Sub-GHz protocol from Flipper/Momentum/Unleashed:

1. **Frequency preset audit** — for every frequency at which the protocol operates
   (not just the most common one), check that the exact Hz value is present in
   `m1_csrc/subghz_freq_presets.c`.  Open the file and search for the value.
   - North American 315-band devices often use non-round frequencies: 302.757 MHz,
     303.875 MHz, 313.85 MHz, **319.5 MHz** (Magellan NA), 345 MHz, etc.
   - EU 433-band devices can use 433.075, 433.42, 433.92, 434.07, 434.42 MHz, etc.
   - If a required frequency is absent, insert it in sorted order, increment
     `SUBGHZ_FREQ_PRESET_COUNT`, update `SUBGHZ_FREQ_PRESET_CUSTOM` to match,
     and recheck `SUBGHZ_FREQ_DEFAULT_IDX` (must still point to 433.92 MHz).
   - Add the new protocol↔frequency pair to the `known_proto_freqs[]` table in
     `tests/test_subghz_registry.c` so it is checked in CI forever.

2. **Band flag audit** — set `SubGhzProtocolFlag_315`, `_433`, `_868`, and/or `_300`
   to cover every band where the protocol is *actually used in the real world*,
   not just the most common one.  A sensor that operates at 319.5 MHz belongs in
   the 315-band and must have `SubGhzProtocolFlag_315`.  A missing band flag means
   "Add Manually" will not show the protocol in that band's picker.
   - **Rule**: if the protocol has any variant below 400 MHz → needs `_315` (or `_300`).
   - **Rule**: if the protocol has any variant in 430–470 MHz → needs `_433`.
   - **Rule**: if the protocol has any variant in 860–870 MHz → needs `_868`.

3. **Flipper vs M1 clock speed** — Flipper's CC1101 and M1's SI4463 both synthesize
   RF frequency from a crystal oscillator, but they do so differently.  A .sub file
   records the frequency Flipper used.  If M1's preset list does not contain that
   exact frequency, M1 cannot retune to it, even if the decoder would otherwise work.
   **Clock speed does not affect decoding; frequency availability does.**  Always
   check the .sub file's `Frequency:` field against the preset table.

4. **Run the tests** after adding the frequency and registering the protocol:
   ```bash
   cmake -B tests/build-tests -S tests && cmake --build tests/build-tests
   ctest --test-dir tests/build-tests --output-on-failure -R "subghz_freq_presets|subghz_registry"
   ```
   The `test_subghz_freq_presets` suite enforces: count consistency, sort order,
   no duplicates, range validity, and specific frequency regression guards.
   The `test_subghz_registry` suite enforces: band flags, known protocol↔freq coverage.

---

## Bind New Remote Wizard — Agent Rules

The Bind New Remote wizard (`SubGhzSceneBindWizard`, `m1_subghz_scene_bind_wizard.c`) is a
guided step-by-step tool for creating and binding **brand-new** rolling-code remotes to
receivers.  It is distinct from the Add Manually scene (which handles static OOK protocols
with manually-entered hex keys) and from the Saved → Emulate path (which replays a
previously captured key).

### Primary reference — DarkFlippers SubGHzRemoteProg.md

> **MANDATORY: Every agent session that modifies, extends, or reviews the Bind New Remote
> wizard MUST consult this document before making any changes:**
>
> `https://github.com/DarkFlippers/unleashed-firmware/raw/refs/heads/dev/documentation/SubGHzRemoteProg.md`
>
> This document is the authoritative source for:
> - The exact receiver programming button sequences for each protocol
> - The timing windows and step ordering required for successful binding
> - Which protocols support new-remote creation vs. clone-only
> - Receiver board variant differences (especially Doorhan)
>
> Fetch the document at the start of any Bind New Remote work session.  Do **not** rely
> solely on the step text already embedded in the wizard source — the DarkFlippers document
> may have been updated with corrected sequences or new protocol variants since the last
> wizard edit.

### Architecture

| File | Role |
|------|------|
| `m1_csrc/m1_subghz_scene_bind_wizard.c` | Wizard scene — protocol picker, step engine, countdown bars, inline TX |
| `Sub_Ghz/subghz_new_remote_gen.c/h` | Pure key generator — splitmix64 PRNG seeded from HAL_GetTick + STM32H5 UID registers; hardware-independent and host-testable |
| `tests/test_subghz_new_remote_gen.c` | Unit tests for the key generator |

### Supported protocols (current implementation)

All five are OOK PWM with `SubGhzProtocolFlag_PwmKeyReplay` — the M1 can bind them
**and** replay them indefinitely from Saved without any cipher keys.

| Protocol | Bits | File prefix | Steps |
|----------|------|-------------|-------|
| CAME Atomo 433 | 62 | `CameAtomo` | 4 |
| Nice FloR-S 433 | 52 | `NiceFlors` | 5 |
| Alutech AT-4N 433 | 64 | `Alutech` | 6 |
| DITEC GOL4 433 | 54 | `DitecGol4` | 4 |
| KingGates Stylo4k 433 | 60 | `KingGates` | 4 |

Generated files are saved to `0:/SUBGHZ/NewRemote_<prefix>_<12-hex>.sub`.

### Rules for adding new protocols to the wizard

1. **Fetch SubGHzRemoteProg.md first** — transcribe step text directly from that document
   into the `BindStep` array.  Do not invent steps from memory.
2. **Verify PwmKeyReplay** — check `subghz_protocol_registry.c` for
   `SubGhzProtocolFlag_PwmKeyReplay`.  Protocols without it cannot be replayed from Saved
   after binding and must display a note on the done screen (see KeeLoq note pattern).
3. **Add to `subghz_new_remote_gen.h/c`** — add a new `BW_PROTO_*` enum value and fill in
   the `ProtoSpec` table entry (label, proto_name, freq_hz, bit_count, te, file_prefix).
4. **Add to `bind_protos[]`** in `m1_subghz_scene_bind_wizard.c` — reference the new step
   array and step count.
5. **Add unit tests** in `tests/test_subghz_new_remote_gen.c` — verify proto_name,
   freq_hz, bit_count, te, key masking, and file_base format.
6. **Do not add Somfy Telis/Keytis** — these have no feasible OTA replay method; the M1
   cannot use the remote after binding.  Document any request to add them as out-of-scope.

---

## Sub-GHz Menu Structure

The Sub-GHz scene menu (`m1_subghz_scene_menu.c`) must contain **exactly 15 items** in this
order.  Do not remove items, reorder, or add "Back" entries.

| # | Label | Scene ID | Implementation |
|---|-------|----------|----------------|
| 1 | Read | SubGhzSceneRead | Scene-native (protocol decode) |
| 2 | Read Raw | SubGhzSceneReadRaw | Scene-native (raw capture) |
| 3 | Saved | SubGhzSceneSaved | Scene-native (file browser) |
| 4 | Playlist | SubGhzScenePlaylist | Blocking delegate |
| 5 | Frequency Analyzer | SubGhzSceneFreqAnalyzer | Blocking delegate → `sub_ghz_frequency_reader()` |
| 6 | Spectrum Analyzer | SubGhzSceneSpectrumAnalyzer | Blocking delegate → `sub_ghz_spectrum_analyzer()` |
| 7 | RSSI Meter | SubGhzSceneRssiMeter | Blocking delegate → `sub_ghz_rssi_meter()` |
| 8 | Freq Scanner | SubGhzSceneFreqScanner | Blocking delegate → `sub_ghz_freq_scanner()` |
| 9 | Weather Station | SubGhzSceneWeatherStation | Blocking delegate → `sub_ghz_weather_station()` |
| 10 | Brute Force | SubGhzSceneBruteForce | Blocking delegate → `sub_ghz_brute_force()` |
| 11 | Add Manually | SubGhzSceneSetType | Scene-native (protocol picker → SetKey hex editor → Transmitter) |
| 12 | Remote | SubGhzSceneRemote | Scene-native (multi-button .rem remote control) |
| 13 | Bind Remote | SubGhzSceneBindWizard | Scene-native (guided rolling-code binding wizard) |
| 14 | Proto Pirate | SubGhzSceneProtoPirateMenu | Scene-native (rolling-code analysis toolkit — PR #579) |
| 15 | Signal ID | SubGhzSceneSignalIdentifier | Blocking delegate → `sub_ghz_signal_identifier()` (RF Rosetta sweep + fingerprint) |

**"Blocking delegate"** scenes call a legacy function that runs its own event loop and
drawing.  The thin scene wrapper (`m1_subghz_scene_<name>.c`) calls the function in
`on_enter`, then pops itself when the function returns.  This integrates legacy code with
the scene manager without rewriting each tool.

---

## Deferred Sub-GHz Momentum-Parity Work

> The Momentum-parity phased programme (Phases 1–12) is otherwise complete.  The items
> below were intentionally deferred and **MUST NOT** be re-opened without first
> re-reading the phase rationale in this section.  When working on a related area,
> check whether any blocker has been lifted and update this table accordingly.

### Non-scene-native list candidates (excluded from widget migration)

The generic `subghz_submenu_model` + `m1_submenu_draw` widget migration is functionally
complete.  The three remaining "list-shaped" UIs are intentionally **not** migrated
because they do not fit the simple-label-list shape the widget targets:

| Site | Reason it is not a widget candidate |
|------|-------------------------------------|
| `SubGhzSceneConfig` | LEFT/RIGHT value columns with `<` `>` arrows — not a label list |
| Saved file browser | Delegates to the generic `storage_browse()` helper — not a scene-native list |
| Add Manually picker | Was the legacy blocking delegate; already retired (now `SubGhzSceneSetType`, which already uses the widget) |

These entries are **not** open work — do not reopen them.

### Counter editing for non-KeeLoq rolling-code protocols

The capability probe (`subghz_signal_fields_counter_edit_status`) classifies
protocols as `SUPPORTED`, `DEFERRED`, or `UNSUPPORTED`.

**Promoted to SUPPORTED (no longer deferred):**
- **Nice FloR-S** — cipher implemented in `subghz_nice_flor_s.c/h` with a 32-byte rainbow table (build-time secret, mirrors KeeLoq key-vault pattern).
- **CAME Atomo** — LFSR stream cipher self-contained in `subghz_came_atomo.c/h`; no external key material needed.
- **Alutech AT-4N** — TEA-variant cipher in `subghz_alutech_at_4n.c/h` with a 32-byte build-time rainbow table.

**Still DEFERRED:**

| Protocol | Blocker |
|----------|---------|
| **Phoenix V2** | Discriminant/checksum trailing the counter must be recomputed after editing; algorithm not yet implemented. |

### Additional Info-screen `get_string()` renderers

The polymorphic `SubGhzGetStringFn` vtable on `SubGhzProtocolDef` is live and the
KeeLoq-family renderer ships in `Sub_Ghz/subghz_signal_format.c`.

Nice FloR-S, CAME Atomo, and Alutech AT-4N now have decoder field decomposition
(their cipher modules are complete) and can receive their own
`subghz_signal_format_<proto>_info()` renderers.  For each:
1. Add `subghz_signal_format_<proto>_info()` in `Sub_Ghz/subghz_signal_format.c`.
2. Wire it onto the matching registry entries.
3. Cover with a `test_subghz_signal_format` sub-suite (output shape, prefix-terminated
   match, NULL safety, truncation safety) — mirror the KeeLoq test pattern.

Phoenix V2 renderer remains blocked until counter editing is implemented.

---
