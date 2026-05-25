# Phase Checklist — Sub-GHz Momentum Parity

## PR Metadata
- **PR Title**: Sub-GHz: Momentum parity — Phase 7c-1 (migrate Menu scene to m1_submenu widget)
- **PR Description**: Continues the multi-phase Sub-GHz Momentum-parity refactor. Phase 7c-1 is the first scene migration onto the Phase 7a/7b reusable `subghz_submenu_model` + `m1_submenu_draw` widget. The Sub-GHz top-level Menu scene now delegates all scroll/selection math to the pure-logic model and all rendering to the shim, deleting ~85 lines of custom pack/unpack/up-down/wrap math and custom u8g2 draw. The persisted state slot is simplified from a 16-bit (sel | scroll<<8) pack to a single byte (sel only) since the model rederives scroll_offset from selected + visible_count on entry. No visual change — `m1_submenu_draw` calls the same `m1_scene_draw_menu()` renderer the scene previously inlined. All 68 host tests pass.

## Phases

### Phase 1 — `subghz_txrx` state machine (foundation)
- **Description**: Introduce a pure-logic state machine module that models radio
  lifecycle states (Idle / Rx / Tx-blocking / Tx-async) and the legal transitions
  between them.  Host-tested.  This is the foundation for centralising radio
  lifecycle in subsequent phases — firmware scenes will be migrated incrementally.
- **Status**: ✅ Complete (state machine + tests landed; scene migration in later phases)
- **Commit**: `Phase 1: add subghz_txrx_state pure-logic state machine + 24 host tests`

### Phase 2 — Per-scene state persistence
- **Description**: Add `subghz_scene_set_state(app, sceneId, value)` and getter so
  scenes can persist UI state (selected submenu index, sub-mode) across pushes/pops
  without static file-scope globals.
- **Status**: ✅ Complete (pure module + 12 host tests; menu scene migrated as first user)
- **Commit**: `Phase 2: add per-scene 32-bit state slots + migrate menu scene`

### Phase 3a — `subghz_endless_tx` repeat / endless-hold policy (pure logic)
- **Description**: Pure-logic state machine modelling SINGLE-mode N-burst TX
  and ENDLESS-mode hold-OK continuous TX with graceful N-repeat finalisation
  on release.  Foundation for the Transmitter scene (Phase 3b).
- **Status**: ✅ Complete (pure module + 19 host tests under ASan+UBSan)
- **Commit**: `Phase 3a: add subghz_endless_tx repeat/hold policy state machine`

### Phase 10 — Scene manager polish (pure logic)
- **Description**: Pure-logic primitives `subghz_scene_stack_find` /
  `subghz_scene_stack_pop_to_depth` (back `search_and_pop_to`),
  `subghz_scene_tick_due` (wrap-safe periodic-tick scheduler), and the
  `subghz_scene_custom_payload_t` typedef for custom events with a 32-bit
  payload.  Pulled forward from its original position because the
  Transmitter scene (Phase 3b) needs ticks (burst-counter animation) and
  custom-event payloads (routing `Q_EVENT_SUBGHZ_TX` → endless-TX engine).
- **Status**: ✅ Complete (pure module + 19 host tests under ASan+UBSan;
  scene-manager wire-up to follow in Phase 3b)
- **Commit**: `Phase 10: add subghz_scene_polish primitives (stack search + tick scheduler)`

### Phase 3b — Transmitter scene + migrate blocking-wrapper callers
- **Description**: New `SubGhzSceneTransmitter` consumes `subghz_endless_tx`
  and the existing async TX primitives (`sub_ghz_replay_prepare_*` /
  `sub_ghz_replay_start_async`).  Migrates Saved/Playlist/Remote/BindWizard
  off the blocking wrappers.  Highest UX win — hold-OK continuous TX with
  live sine-wave + burst counter on display.

  Split into sub-phases:

  - **Phase 3b-1 — Transmitter controller (pure logic).**  ✅ Complete.
    `Sub_Ghz/subghz_transmitter_ctl.c/h` sits one layer above the
    `subghz_endless_tx` engine and one layer below the future scene.
    Owns the READY / TX / EXITING phase, translates scene events
    (OK_PRESS / OK_RELEASE / TX_BURST_COMPLETE / BACK / TEARDOWN_DONE /
    LEFT / RIGHT) into scene actions (TX_START / TX_NEXT_BURST /
    TX_TEARDOWN / EXIT_SCENE / CYCLE_BUTTON_*).  Hardware-independent;
    21 host tests under ASan+UBSan cover init, all phase transitions,
    SINGLE/ENDLESS bursts, abort, second-run reuse, button cycling
    (Phase 4 hook), and the two state-invariants.

  - **Phase 3b-2 — Transmitter scene + scene-manager API wire-up +
    caller migration.**  🔄 In progress.  Split into 3b-2a (API wire-up,
    ✅) and 3b-2b (scene + caller migration, 🔲).

    - **Phase 3b-2a — Scene-manager API wire-up.**  ✅ Complete.
      Exposes the Phase 10 polish primitives to scene code by wiring
      `subghz_scene_search_and_pop_to(app, target)` (backed by
      `subghz_scene_stack_pop_to_depth`), `subghz_scene_send_custom_event` /
      `subghz_scene_custom_payload` (backed by `subghz_scene_custom_payload_t`),
      and `subghz_scene_set_tick_period` (backed by `subghz_scene_tick_due`)
      into `m1_subghz_scene.h/c`.  Adds two new event ids — `SubGhzEventTick`
      and `SubGhzEventCustom` — and three new `SubGhzApp` fields
      (`tick_period_ms`, `last_tick_ms`, `custom_event_payload`).  The main
      loop polls at `min(rx_active?200ms : ∞, tick_period_ms)` and dispatches
      `SubGhzEventTick` on cadence using the wrap-safe pure-logic helper.
      All transitions (push / pop / replace / search_and_pop_to) reset
      `tick_period_ms = 0` so a child cannot inherit a parent's cadence.

    - **Phase 3b-2b — Transmitter scene + caller migration.**  🔄 In
      progress.  Split into 3b-2b-i (scaffold) and 3b-2b-ii…v (per-caller
      migration).

      - **Phase 3b-2b-i — Transmitter scene scaffold.**  ✅ Complete.
        Adds `SubGhzSceneTransmitter` with full async-TX integration:
        controller-driven READY/TX/EXITING state machine, prepare_flipper +
        start_async on OK_PRESS, continue_async on TX completion, abort +
        temp-file unlink on TX_TEARDOWN and scene_on_exit, animated dots
        + burst counter while TX is in flight, error display + recovery
        for prepare/start failures.  Inputs: `app->tx_path`,
        `tx_repeat_count`, `tx_mode`.  Registered in the scene registry
        and wired into the firmware CMake build.  No callers migrated
        yet — that lands in 3b-2b-ii…v.

      - **Phase 3b-2b-ii — Saved (PACKET path) migration.**  ✅ Complete.
        Replaces the inline `sub_ghz_replay_flipper_file()` blocking call
        in `m1_subghz_scene_saved.c::handle_action(SAVED_ACTION_EMULATE)`
        with `subghz_scene_push(app, SubGhzSceneTransmitter)`.  The
        Saved scene now hands off TX entirely to the async-driven
        Transmitter scene; on completion or BACK, it returns to the
        file browser.  No new tests required — the Transmitter scene's
        21-test controller suite covers the state machine, and the
        flipper-load path remains identical.
      - **Phase 3b-2b-iii — Playlist migration.**  ✅ Complete.
        Converts `m1_subghz_scene_playlist.c` from a synchronous
        `while`-loop driver (`playlist_run_pass` + `playlist_transmit_next`
        calling `sub_ghz_replay_flipper_file` / `sub_ghz_replay_datafile`
        in a tight loop) to a fully async scene-push state machine.
        New `playlist_push_transmitter()` sets `tx_path`/`tx_repeat_count`/
        `tx_mode` and pushes `SubGhzSceneTransmitter` for the current
        file.  `scene_on_enter` checks `app->resume_from_child` to detect
        the Transmitter pop-back; on natural completion advances
        `playlist_current` (or starts a new pass and increments
        `playlist_repeat_done`), on user-initiated abort stops playback.
        Abort detection uses a new `SubGhzApp::tx_completed_naturally`
        flag set to `true` by the Transmitter scene's `scene_on_enter`
        and overridden to `false` by its BACK / error-dismiss handlers.
        The Playlist no longer calls `vTaskDelay` (which violated the
        async/non-blocking RTOS rule) or the blocking replay wrappers;
        inter-signal delays from the .txt playlist are still parsed
        but not yet applied — to be re-added via `set_tick_period`
        in a follow-up.
      - **Phase 3b-2b-iv — Remote migration.**  ✅ Complete.
        Replaces the `sub_ghz_replay_flipper_file()` + `menu_sub_ghz_init()`
        blocking call in `m1_subghz_scene_remote.c::remote_fire()` with
        a `subghz_scene_push(app, SubGhzSceneTransmitter)` handoff.  To
        preserve the legacy "one press = fire" UX (the user must not
        see a READY/Press-OK confirmation between pressing a remote
        button and the signal going out), a new `SubGhzApp::tx_autostart`
        boolean is consumed by the Transmitter scene's `scene_on_enter`,
        which synthesizes an `OK_PRESS` controller event to start TX
        immediately and then clears the flag.  Remote sets it to `true`
        on every `remote_fire()`; defensively clears it in its own
        `scene_on_enter` to keep stale values from leaking across
        sibling Transmitter pushes.  The Remote scene's own "Sent!"
        flash overlay and the corresponding `rem_tx_flash` /
        `rem_tx_last` static state were dropped — the Transmitter
        scene's TX overlay supersedes them.  No new tests required —
        the Transmitter controller's 21-test suite already covers
        the OK_PRESS path, and the new field is exercised by the
        existing READY→TX transition coverage.
      - **Phase 3b-2b-v — Bind Wizard migration.**  ✅ Complete.
        Replaced the wizard's blocking `sub_ghz_replay_flipper_file()`
        call in `bw_transmit()` with `bw_push_tx()`, which sets
        `tx_path` / `tx_repeat_count=1` / `tx_mode=SINGLE` /
        `tx_autostart=true` / `resume_from_child=true` and pushes
        `SubGhzSceneTransmitter`.  Step advancement now happens in
        `scene_on_enter`'s resume-from-child branch: on
        `tx_completed_naturally==true` the wizard advances to the next
        step (or to DONE if last step); on a user BACK abort, the
        wizard stays on the current TX step so the user can retry.
        Radio lifecycle is now fully owned by the Transmitter scene —
        the wizard no longer needs to call `menu_sub_ghz_init/exit`.
        All 65 host tests pass.
- **Status**: ✅ Complete (3b-1 ✅; 3b-2a ✅; 3b-2b-i..v ✅)
- **Commit**: `Phase 3b-2b-v: migrate Bind Wizard to async SubGhzSceneTransmitter`

### Phase 4 — Custom button cycling for rolling-code TX
- **Description**: UP/DOWN during Transmitter cycles button code 0..3 for
  KeeLoq / FloR-S / CAME Atomo / FAAC rolling-code remotes.
- **Status**: ✅ Complete (4a ✅; 4b ✅; 4c ✅)
- **Commit**: `Phase 4c: button-override key mutation (KeeLoq family) + AND-gate cycling`

  - **Phase 4a** ✅ — pure-logic `subghz_button_caps` module
    (`Sub_Ghz/subghz_button_caps.{c,h}`) maps a protocol name to
    `(supports_cycling, button_count)`.  Covers KeeLoq family, Nice
    FloR-S (16 codes), CAME Atomo/TWEE, Alutech AT-4N, KingGates
    Stylo4k, DITEC_GOL4, Scher-Khan, Toyota.  16 host tests
    (`tests/test_subghz_button_caps.c`) pin protocol coverage,
    case-insensitive matching, whitespace trimming, and the
    "supports_cycling ⇒ button_count ≥ 2" invariant.  All 66 host
    tests pass.
  - **Phase 4b** ✅ — `tx_protocol_name[32]` field added to
    `SubGhzApp`; Transmitter `scene_on_enter` queries
    `subghz_button_caps_for_protocol()` and passes
    `(supports_cycling, button_count)` into
    `subghz_transmitter_ctl_init()`.  The READY screen renders a
    "Btn X/Y" indicator and LEFT/RIGHT arrow hints when cycling is
    enabled; TX screen appends "Btn X/Y" to the burst counter.  The
    `SUBGHZ_TXCTL_ACT_CYCLE_BUTTON_*` branches in `perform_action()`
    now redraw on each LEFT/RIGHT press.  Callers updated:
    Saved populates from `saved_signal.protocol`, Bind Wizard from
    `bw_params.proto_name`; Playlist and Remote clear the field
    (cycling not part of their UX).  No key re-encoding yet — that
    lands in 4c.  All 66 host tests still pass; no new tests needed
    (controller cycling is already covered by the Phase 3b-1 21-test
    suite, and protocol→caps lookup is covered by the 16 Phase 4a tests).
  - **Phase 4c** ✅ — pure-logic `subghz_button_override` module
    (`Sub_Ghz/subghz_button_override.{c,h}`) mutates the protocol's
    button-field bits inside a 64-bit Flipper SubGhz Key value.
    Supports KeeLoq family (KeeLoq, Jarolift — button at [63:60];
    Star Line — button at [3:0]) where the button is a plain-bit
    field that downstream encoders honour.  Protocols whose button
    is encoded via opaque manufacturer lookup tables (Nice FloR-S)
    or inside an encrypted rolling payload (CAME Atomo, Alutech
    AT-4N, KingGates Stylo4k, DITEC GOL4, Toyota, Scher-Khan)
    return `false` from `subghz_button_override_apply()` —
    documented for future per-protocol encoders.  Wiring:
    `sub_ghz_replay_set_button_override(button)` populates a
    per-prepare slot; the Flipper-format converter
    (`subghz_replay_flipper_to_tmp`) latches and clears the slot
    at entry and applies it to the parsed `key_value` before the
    OOK PWM / KeeLoq counter-mode encoder runs.  Transmitter scene
    AND-gates cycling enable against
    `subghz_button_override_supports()` so the UI only offers
    cycling when LEFT/RIGHT will actually mutate the transmitted
    key, and calls `sub_ghz_replay_set_button_override()` before
    each TX_START.  16-test host suite
    (`tests/test_subghz_button_override.c`) covers all 16 button
    indices, serial+hop preservation, round-trip restoration,
    pairwise-distinct keys for 4-button cycles, unsupported
    protocols, NULL safety, case/whitespace tolerance, and
    `_supports()`/`_apply()` agreement.  All 67 host tests pass.

### Phase 5 — Split Saved into Saved + SavedMenu + Delete
- **Description**: Extract action menu and delete confirmation from
  `m1_subghz_scene_saved.c` into dedicated scenes; Saved becomes a pure file
  browser.  Adds `SubGhzSceneSavedMenu` (action menu + info + decode screens)
  and `SubGhzSceneDelete` (LEFT/RIGHT confirmation dialog, Cancel-default).
  File-selection state moves from file-scope statics to shared `SubGhzApp`
  fields (`saved_filepath`, `saved_filename`).  Delete confirm pops back
  through SavedMenu to Saved via `subghz_scene_search_and_pop_to`.
  All 67 host tests pass.
- **Status**: ✅ Complete
- **Commit**: `Phase 5: split Saved into Saved + SavedMenu + Delete scenes`

### Phase 6 — MoreRAW + DecodeRAW scenes
- **Description**: Extract Read-Raw "More" submenu and offline decode results
  screen into dedicated scenes.  Adds `SubGhzApp::raw_filepath` shared field,
  `SubGhzSceneMoreRaw` (Decode/Rename/Delete), and `SubGhzSceneDecodeRaw`
  (list + detail view).  Read Raw resume-from-child detects child-side delete
  (raw_filepath cleared) and resets to Start state.  Removes ~340 lines of
  overlay state from `m1_subghz_scene_read_raw.c`.
- **Status**: ✅ Complete (host tests: 67/67 passing)
- **Commit**: `Phase 6: extract Read-Raw MoreRAW + DecodeRAW into dedicated scenes`

### Phase 7 — Reusable `m1_submenu` widget
- **Description**: Generic font-aware scrollable list widget; migrate every
  hand-rendered list scene onto it.

  Split into sub-phases:

  - **Phase 7a — Pure-logic `subghz_submenu_model` (scroll/selection math).**
    ✅ Complete.  `Sub_Ghz/subghz_submenu_model.c/h` provides a 4-byte
    model `{item_count, selected, scroll_offset, visible_count}` with
    `init`, `set_item_count`, `set_visible_count`, `set_selected`, `up`,
    `down`, `is_visible`, and `visible_row`.  Wrap-around (top↔bottom),
    scroll-window anchoring (snap to selection in both directions),
    empty-list and zero-visible safety are all enforced.  Hardware-
    independent — no u8g2 / RTOS / SubGhzApp / HAL coupling.  25 host
    tests under ASan+UBSan pin every invariant including a random-walk
    fuzz pass.  Registered in `cmake/m1_01/CMakeLists.txt` so the
    firmware links it in for Phase 7b.  No scenes migrated yet — that
    starts in Phase 7c.
  - **Phase 7b — Firmware rendering shim.**  ✅ Complete.  Adds
    `m1_csrc/m1_submenu.{h,c}` providing
    `m1_submenu_draw(model, title, labels)`, a thin null-guarded
    adapter that translates the Phase 7a model's
    `item_count`/`selected`/`scroll_offset`/`visible_count` fields into
    the existing `m1_scene_draw_menu()` renderer.  All u8g2 drawing,
    geometry constants, and font-aware row sizing already live in
    `m1_scene.c`, so the shim is intentionally minimal — callers manage
    the model in `on_enter`/`on_event` and emit one
    `m1_submenu_draw()` call from `draw`.  Registered in
    `cmake/m1_01/CMakeLists.txt`.  No host tests required (rendering
    is hardware-coupled per the CLAUDE.md "What NOT to unit test"
    list); the model itself is covered by the Phase 7a 25-test suite.
    All 68 host tests pass.
  - **Phase 7c+ — Migrate scenes.**  🔄 In progress.

    - **Phase 7c-1 — Sub-GHz Menu scene.**  ✅ Complete.
      Replaces the scene's hand-rolled scroll/selection math and custom
      u8g2 draw with `subghz_submenu_model_*` + `m1_submenu_draw()`.
      Persisted state slot shrinks from a 16-bit (sel | scroll<<8) pack
      to a single byte (selection only) — the model rederives
      `scroll_offset` from `selected + visible_count` on every
      `scene_on_enter`, and `set_visible_count` is called on every
      `scene_on_event` / `draw` so a user text-size change picked up
      while a child scene was on top resyncs correctly.  No visual
      change — `m1_submenu_draw` calls the same `m1_scene_draw_menu()`
      renderer the scene previously inlined.  All 68 host tests pass.

    Candidate scenes still to migrate: SavedMenu, MoreRAW, Config
    (Sub-GHz config), Saved file browser, Add Manually picker, Bind
    Wizard protocol picker.
- **Status**: 🔄 In progress (7a ✅; 7b ✅; 7c-1 ✅; 7c-2+ pending)
- **Commit**: `Phase 7c-1: migrate Sub-GHz Menu scene to m1_submenu widget`

### Phase 8 — SetType / SetKey / SetSerial / SetButton / SetCounter
- **Description**: Create-from-scratch flow gated by
  `SubGhzProtocolFlag_PwmKeyReplay`.  Covers ~25 protocols (the ones M1 can
  actually replay).
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 9 — SignalSettings scene
- **Description**: Per-file CounterMode and counter/button byte editing on
  KeeLoq, Nice FloR-S, CAME Atomo, Alutech AT-4N, Phoenix V2.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 10 — Scene manager polish
- **Description**: `search_and_pop_to`, periodic tick events, custom events with
  16-bit payload.
- **Status**: ✅ Complete (see entry above — promoted to land before Phase 3b)
- **Commit**: `Phase 10: add subghz_scene_polish primitives (stack search + tick scheduler)`

### Phase 11 — Polymorphic decoder `get_string()` vtable
- **Description**: Add `get_string(decoder, FILE*)` to the protocol registry
  entry struct.  Removes the last `strcasecmp(proto->name, …)` branches in scene
  code.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 12 — Receiver history quality-of-life
- **Description**: `delete_old_signals` and `remove_duplicates` toggles, exposed
  via `SubGhzSceneConfig`.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
