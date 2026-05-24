# Phase Checklist — Sub-GHz Momentum Parity

## PR Metadata
- **PR Title**: Sub-GHz: Momentum parity — Phase 3b-2b-iv (Remote migration to async Transmitter)
- **PR Description**: Begins the multi-phase effort to close the remaining Sub-GHz architecture and feature gaps between M1 and Momentum.  This PR ships the phase plan and lands Phase 1 (TxRx state-machine foundation with host-side tests).  Subsequent phases are tracked here and will land in follow-up commits on the same branch.

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
      - **Phase 3b-2b-v — Bind Wizard migration.**  🔲 Not started.
- **Status**: 🔄 In progress (3b-1 ✅; 3b-2a ✅; 3b-2b-i ✅; 3b-2b-ii ✅; 3b-2b-iii ✅; 3b-2b-iv ✅; 3b-2b-v pending)
- **Commit**: `Phase 3b-2b-iv: migrate Remote scene to async SubGhzSceneTransmitter (tx_autostart)`

### Phase 4 — Custom button cycling for rolling-code TX
- **Description**: UP/DOWN during Transmitter cycles button code 0..3 for
  KeeLoq / FloR-S / CAME Atomo / FAAC rolling-code remotes.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 5 — Split Saved into Saved + SavedMenu + Delete
- **Description**: Extract action menu and delete confirmation from
  `m1_subghz_scene_saved.c` into dedicated scenes; Saved becomes a pure file
  browser.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 6 — MoreRAW + DecodeRAW scenes
- **Description**: Extract Read-Raw "More" submenu and offline decode results
  screen into dedicated scenes.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

### Phase 7 — Reusable `m1_submenu` widget
- **Description**: Generic font-aware scrollable list widget; migrate every
  hand-rendered list scene onto it.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_

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
