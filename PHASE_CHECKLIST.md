# Phase Checklist — dagnazty v0.2.2–v0.4.0 cherry-pick (Phases 1–3)

## PR Metadata
- **PR Title**: Upstream cherry-pick: SubGHz Once/Repeat, RSSI bar, IR Custom Remotes (dag v0.2.2–v0.4.0)
- **PR Description**: Cherry-picks three features from dagnazty/M1_T-1000 (romulofer contributor, v0.2.2–v0.4.0), adapted to Hapax's scene/async architecture: (1) SubGHz Send Once vs Repeat mode toggle (LEFT/RIGHT during TX), (2) live RSSI dBm bar on the Read Raw record screen, (3) IR Custom Remotes — on-device multi-button remote builder with learn/rename/delete — plus new ir_database categories (Bluray, Monitor).

## Phases

### Phase 1 — SubGHz Send Once/Repeat toggle
- **Description**: Add a toggle mode (`raw_tx_repeat_mode`) to SubGhzApp. LEFT/RIGHT during TX states flip the toggle. The TX-complete handler uses `toggle || ok_held` to decide whether to loop. Button bar during TX shows current mode. Regression test covers the pure dispatch helper.
- **Status**: ✅ Complete
- **Commit**: `Phase 1: SubGHz Send Once/Repeat toggle (dag a1f489c7)`

### Phase 2 — Live RSSI bar on record screen (Tasks 1–2)
- **Description**: Task 1: extract geometry helper `subghz_rssi_fill_w()` into `Sub_Ghz/subghz_rssi_bar.inc` and refactor `subghz_rssi_bar_draw()` to use it; add host test `tests/test_subghz_rssi.c`. Task 2: call `subghz_rssi_bar_draw()` in the RECORDING state of the Read Raw `draw()` function. Tasks 3–4 from dag are already present in Hapax (100 ms rate limiter and passive pre-scan RX).
- **Status**: ✅ Complete
- **Commit**: `Phase 2: Live RSSI bar on Read Raw record screen (dag Tasks 1-2)`

### Phase 3 — IR Custom Remotes + new ir_database files
- **Description**: (a) Extend `flipper_ir.c/h` with `flipper_ir_rewrite()`, `flipper_ir_rename_signal()`, `flipper_ir_delete_signal()`, `flipper_ir_raw_feed()`. (b) Add host tests `tests/test_flipper_ir_custom.c`. (c) Create `m1_csrc/m1_ir_custom.c/h` — multi-button custom remote scene (learn, browse, rename, delete). (d) Wire "Custom Remotes" entry into `m1_infrared_scene.c`. (e) Add new ir_database files: `ir_database/Bluray/Samsung_Bluray.ir`, `ir_database/Bluray/Sony_Bluray.ir`, `ir_database/Monitor/Universal_Monitor.ir`, `ir_database/LED/RGB_24key.ir`.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
