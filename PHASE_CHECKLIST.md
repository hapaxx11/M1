# Phase Checklist — Sub-GHz Momentum Parity

## PR Metadata
- **PR Title**: Sub-GHz: Momentum parity — phased migration (Phase 1 — TxRx state machine)
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

    - **Phase 3b-2b — Transmitter scene + caller migration.**  🔲 Not
      started.  Adds `SubGhzSceneTransmitter` consuming the 3b-1 controller
      via the 3b-2a API; migrates Saved/Playlist/Remote/BindWizard off the
      blocking replay wrappers.
- **Status**: 🔄 In progress (3b-1 ✅; 3b-2a ✅; 3b-2b pending)
- **Commit**: `Phase 3b-2a: wire scene-manager polish primitives into SubGhz scene API`

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
