# Phase Checklist — Sub-GHz Read Raw / Decode Momentum Parity

## PR Metadata
- **PR Title**: Sub-GHz: fix decode scene styling and add Send/Save actions for decoded signals
- **PR Description**: Addresses Read Raw decode inconsistencies with Momentum: (1) fix decode scene list and detail views to use the standard menu styling (m1_menu_font, M1_MENU_TEXT_W, scrollbar, highlight geometry), (2) add Send and Save actions to the decoded signal detail view in both the DecodeRaw scene (Read Raw → More → Decode) and the SavedMenu decode screen (Saved → file → Decode), (3) use protocol-specified TE instead of observed average in decode results display.

## Phases

### Phase 1 — Fix DecodeRaw scene styling (Read Raw path)
- **Description**: Updated `m1_subghz_scene_decode_raw.c` list and detail views to follow M1 visual styling guidelines: uses `m1_menu_font()` / `m1_menu_item_h()` / `m1_menu_max_visible()` for list rows, `M1_MENU_TEXT_W` for highlight width, scrollbar, centered title via `m1_draw_text`.
- **Status**: ✅ Complete
- **Commit**: `Fix DecodeRaw scene styling and add Send/Save actions`

### Phase 2 — Fix SavedMenu decode screen styling
- **Description**: Updated `m1_subghz_scene_saved_menu.c` `draw_decode_screen()` to match the same styling: `m1_menu_font()`, `m1_menu_item_h()`, scrollbar, proper highlight geometry.
- **Status**: ✅ Complete
- **Commit**: `Fix DecodeRaw scene styling and add Send/Save actions`

### Phase 3 — Add Send/Save actions to DecodeRaw detail view
- **Description**: OK = Send (writes temp .sub key file → pushes Transmitter scene), DOWN = Save (prompts filename → writes persistent .sub/.sgh key file). Button bar shows Save/Send hints.
- **Status**: ✅ Complete
- **Commit**: `Fix DecodeRaw scene styling and add Send/Save actions`

### Phase 4 — Add Send/Save actions to SavedMenu decode detail view
- **Description**: Same Send/Save actions in the SavedMenu decode path.
- **Status**: ✅ Complete
- **Commit**: `Fix DecodeRaw scene styling and add Send/Save actions`

### Phase 5 — Use protocol-specified TE in decode results display
- **Description**: Both decode scenes now prefer `subghz_protocols_list[protocol].te_short` when the decoded TE is 0, and the Send/Save paths also use the protocol-specified TE.
- **Status**: ✅ Complete
- **Commit**: `Fix DecodeRaw scene styling and add Send/Save actions`

### Phase 6 — Cleanup and remove checklist
- **Description**: Remove PHASE_CHECKLIST.md before final PR.
- **Status**: 🔄 In progress
- **Commit**: _(pending)_
