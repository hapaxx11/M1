---
name: ui-scene-architecture
description: M1 UI and scene-manager standards: the font inventory and maintenance rules, user-configurable menu font size, saved-item action pattern, post-connection navigation pattern, Momentum-style button bars/headers, and scene-based application architecture. Load for any scene, menu, button bar, font, or saved-file UI work.
---

# UI & Scene Architecture

> Extracted from CLAUDE.md. Load for any UI, scene, menu, button-bar, or font work.

### Font Inventory

The M1 firmware compiles ~1987 u8g2 fonts in `Drivers/u8g2_csrc/u8g2_fonts.c`.
The table below inventories fonts referenced by application code via
`m1_csrc/m1_display.h` (display-role macros),
`m1_csrc/m1_app_api.c` (BadUSB scripting API table), and direct
`u8g2_SetFont()` call sites.  It may also include a separate
"defined but unused" entry, which is not necessarily linked into the final
binary when section garbage collection is enabled.

**u8g2 font suffix meanings:**

| Suffix | Charset | Notes |
|--------|---------|-------|
| `_tr` | Printable ASCII 32–127 (transparent mode) | Has lowercase, most common |
| `_tf` | Printable ASCII 32–127 (filled mode) | Has lowercase, filled background |
| `_tu` | Uppercase only 32–90 (transparent) | **No lowercase** |
| `_tn` | Numbers + punctuation 32–57 (transparent) | Digits only |
| `_mr` / `_mf` | Monospaced, reduced / full | Fixed-width |
| `_8r` / `_8f` | Full 256-char set | Extended Latin |
| `_t_all` | Full Unicode set | Large flash footprint |

**Active font table — updated 2026-04-11:**

| Font | Macro / API | Role | Ascent | Lowercase |
|------|-------------|------|--------|-----------|
| `u8g2_font_resoledmedium_tr` | `M1_DISP_MAIN_MENU_FONT_N` + API | Main menu normal text | 7 | Yes |
| `u8g2_font_helvB08_tf` | `M1_DISP_MAIN_MENU_FONT_B` + API | Main menu bold text | 8 | Yes |
| `u8g2_font_helvB08_tr` | `M1_POWERUP_LOGO_FONT` + API | Splash screen "M1 Hapax" | 8 | Yes |
| `u8g2_font_NokiaSmallPlain_tf` | `M1_DISP_SUB_MENU_FONT_N` + API | Sub-menu normal text | 7 | Yes |
| `u8g2_font_squeezed_b7_tr` | `M1_DISP_SUB_MENU_FONT_B` + API | Sub-menu bold text | 7 | Yes |
| `u8g2_font_profont12_mr` | `M1_DISP_FUNC_MENU_FONT_N` + API | Function menu monospace (Medium) | 8 | Yes |
| `u8g2_font_spleen5x8_mf` | API only | Legacy medium font (kept for script compat) | 6 | Yes |
| `u8g2_font_nine_by_five_nbp_tf` | `M1_DISP_FUNC_MENU_FONT_N2` + API | Function menu larger | 9 | Yes |
| `u8g2_font_courB08_tf` | `M1_DISP_RUN_MENU_FONT_B` + API | Running menu bold | 6 | Yes |
| `u8g2_font_Terminal_tr` | `M1_DISP_RUN_ERROR_FONT_1B` + API | Error messages | 9 | Yes |
| `u8g2_font_lubB08_tf` | `M1_DISP_RUN_ERROR_FONT_2B` + API | Error messages bold | 9 | Yes |
| `u8g2_font_victoriabold8_8r` | `M1_DISP_RUN_WARNING_FONT_1B` | Power warnings (wide bold) | 8 | Yes |
| `u8g2_font_pcsenior_8f` | `M1_DISP_RUN_WARNING_FONT_2B` + API | Warning messages | 7 | Yes |
| `u8g2_font_samim_10_t_all` | `M1_DISP_RUN_WARNING_FONT_3` | **Defined but unused** | — | Yes |
| `u8g2_font_profont17_tr` | `M1_DISP_LARGE_FONT_1B` + API | Large readable text | 11 | Yes |
| `u8g2_font_VCR_OSD_tu` | `M1_DISP_LARGE_FONT_2B` + API | Large uppercase display | 12 | **No** |
| `u8g2_font_spleen8x16_mf` | `M1_DISP_LARGE_FONT_3B` + API | Large monospace | 11 | Yes |
| `u8g2_font_Pixellari_tu` | `M1_MAIN_LOGO_FONT_1B` + API | Main logo / heading | 10 | **No** |
| `u8g2_font_4x6_tr` | API only | BadUSB tiny text | 5 | Yes |
| `u8g2_font_5x8_tr` | API only | BadUSB small text | 6 | Yes |
| `u8g2_font_6x10_tr` | API only | BadUSB small readable | 7 | Yes |
| `u8g2_font_10x20_mr` | API only | BadUSB large monospace | 14 | Yes |
| `u8g2_font_finderskeepers_tf` | API only | BadUSB game-style | 7 | Yes |
| `u8g2_font_u8glib_4_tf` | game_2048.c only | Tiny 4px font for 4+ digit tile values in 2048 | 4 | Yes |

**Where the font is bound determines which code files must be updated when
changing it:**

- **`m1_display.h` macro** → changing the macro automatically updates all callers.
- **`m1_app_api.c` table** → the BadUSB scripting engine looks up fonts by string
  name at runtime.  If a font is removed from the table, existing `.m1app` scripts
  referencing it by name will fail.

### Font Maintenance Rules

1. **When adding a new font**: add it to the table above, add it to the
   `m1_app_api.c` font table if BadUSB scripts should be able to use it, and
   verify it is declared in `u8g2_fonts.c` (all 1987 fonts are already compiled;
   you only need to add a new entry if you bring in a font from an external
   source).
2. **When changing `M1_POWERUP_LOGO_FONT`**: the replacement font's `1` glyph
   must be visually distinct from its `I` glyph — otherwise "M1" reads as "MI"
   on the splash screen.  Prefer fonts whose `1` has a top hook or base serif.
3. **When removing a font**: remove it from this table, from the `m1_app_api.c`
   table, and from the `m1_display.h` macro (if applicable).  Note that removing
   a font from `m1_app_api.c` breaks any `.m1app` scripts that reference it.
4. **Keep this table current**: any agent session that modifies `m1_display.h`
   font macros, `m1_app_api.c` font table entries, or introduces a new
   `u8g2_SetFont()` call with a font not already listed here **MUST** update
   this table before the session ends.
5. **Suffix awareness**: when choosing a font for a context that displays
   lowercase text (menus, filenames, version strings), **never** use a `_tu` or
   `_tn` suffix font — those lack lowercase glyphs.  Use `_tr`, `_tf`, `_mr`,
   or `_mf` instead.

### User-Configurable Font Size — `m1_menu_style`

The user can change the menu text size in **Settings → LCD & Notifications → Text Size**.
The setting is stored as `m1_menu_style` (declared in `m1_system.h`, defined in
`m1_system.c`, persisted via `menu_style=N` in `settings.cfg`).

| `m1_menu_style` | Label  | Row Height | Visible Items | Font |
|-----------------|--------|------------|---------------|------|
| 0 (default)     | Small  | 8 px       | 6             | `M1_DISP_SUB_MENU_FONT_N` (`NokiaSmallPlain`) |
| 1               | Medium | 10 px      | 5             | `M1_DISP_FUNC_MENU_FONT_N` (`profont12`) |
| 2               | Large  | 13 px      | 4             | `M1_DISP_FUNC_MENU_FONT_N2` (`nine_by_five_nbp`) |

**Helpers** (declared in `m1_scene.h`, defined in `m1_scene.c`):

| Helper | Returns |
|--------|---------|
| `m1_menu_item_h()` | Row height in pixels (8, 10, or 13) |
| `m1_menu_max_visible()` | Max items that fit in the 52px menu area (6, 5, or 4) |
| `m1_menu_font()` | Pointer to the u8g2 font for the current style |
| `M1_MENU_VIS(count)` | `min(count, m1_menu_max_visible())` — convenience macro |

**Layout constants** (in `m1_scene.h`):

| Constant | Value | Purpose |
|----------|-------|---------|
| `M1_MENU_AREA_TOP` | 12 | Y coordinate below title + separator |
| `M1_MENU_AREA_H` | 52 | Available vertical space (64 − 12) |
| `M1_MENU_TEXT_W` | 124 | Highlight/text area width (leaves 4px for scrollbar) |
| `M1_MENU_SCROLLBAR_X` | 125 | Scrollbar left edge |
| `M1_MENU_SCROLLBAR_W` | 3 | Scrollbar track width |

#### Rules for all scrollable lists

1. **NEVER hardcode visible item counts or row heights.**  Use `m1_menu_max_visible()`
   and `m1_menu_item_h()` instead of `#define LIST_VISIBLE_ITEMS 4` or
   `#define LIST_ITEM_HEIGHT 9`.
2. **ALWAYS use `m1_menu_font()`** for list item text.  Do not hardcode
   `M1_DISP_FUNC_MENU_FONT_N` or `M1_DISP_SUB_MENU_FONT_N` — the correct font
   depends on the user's text size setting.
3. **Use `M1_MENU_VIS(count)`** when computing the visible slice of a list.
4. **The text baseline offset is `m1_menu_item_h() − 1`** — this places text
   1px above the bottom of its row.
5. **Highlight box width must be `M1_MENU_TEXT_W` (124px)**, not 128px, to leave
   room for the scrollbar.
6. **Include `m1_scene.h`** in any `.c` file that draws a scrollable list.
7. **When importing features from other repositories** (Flipper, C3, SiN360, or any
   fork), the imported code will almost certainly hardcode fonts and row heights.
   These **must** be converted to the Hapax font-aware helpers before merging.
   Neither Flipper nor upstream Monstatek have a user-configurable text size
   feature — every imported scrollable list needs adaptation.  This is a
   **blocking merge requirement**, not a follow-up task.

#### Exceptions (justified hardcoded values)

Some specialized displays intentionally use fixed row heights because they are
**not** standard selectable menus — they are compact data displays with
constrained vertical space:

| File | Define | Justification |
|------|--------|---------------|
| `m1_subghz_scene_read.c` | `HISTORY_ROW_H 8`, `HISTORY_VISIBLE 3` | Compact RX history in split-screen (RSSI bar + history + bottom bar) |
| `m1_subghz_scene_saved.c` | `DECODE_ROW_H 8`, `DECODE_VISIBLE 3` | Offline decode results in dual-view layout |
| `m1_sub_ghz.c` | `FREQ_SCANNER_VISIBLE_ROWS 5` | Frequency scanner data display, not a selectable menu |
| `m1_sub_ghz.c` | `SUBGHZ_HISTORY_ROW_HEIGHT 6`, `SUBGHZ_HISTORY_VISIBLE_ITEMS 5` | Legacy history (tiny rows for maximum density) |
## Saved Item Actions Pattern

> **This is the canonical UX standard for all M1 modules with saved files.**
> It takes precedence over any previously defined UX preferences when they conflict.
> The pattern aligns with Flipper Zero's `*_scene_saved_menu.c` architecture —
> specifically, the action-menu verb set and navigation flow are modelled on Flipper
> and Momentum firmware rather than Monstatek stock.  Monstatek UX conventions
> (display rendering, keypad mapping, UIView framework) still apply when not
> superseded by this pattern.

Every module that loads files from SD card **MUST** provide a standard set of
saved-item actions.  This ensures consistency across the device and makes Flipper
port alignment straightforward.

### Core Verbs (required for ALL modules with saved files)

| Verb | Description | Implementation |
|------|-------------|----------------|
| **Emulate / Send** | Transmit or replay the saved item | Module-specific TX function |
| **Info** | Display file metadata (protocol, frequency, key/data) | Read-only info screen; BACK dismisses |
| **Rename** | Rename file on SD via virtual keyboard | `m1_vkb_get_filename()` → `f_rename()` |
| **Delete** | Delete file with confirmation dialog | `m1_message_box_choice()` → `m1_fb_delete_file()` or `f_unlink()` |

### Optional Verbs (module-specific)

| Verb | Modules | Description |
|------|---------|-------------|
| **Save** | Read scenes only (live decode, not loaded files) | Save captured signal to SD |
| **Write** | NFC, RFID | Write data to a physical tag/card |
| **Edit** | NFC (Edit UID), RFID (Edit data) | Modify item fields in-place |
| **Unlock** | NFC only | MF Classic key recovery / unlock |
| **Send All** | IR only | Transmit all commands in an .ir file sequentially |
| **Card/Tag Actions** | NFC, RFID | Context-specific tool submenu for loaded item |

### Implementation Patterns

**Sub-GHz** (`m1_subghz_scene_saved.c`):
- Inline action menu with `in_action_menu` / `in_info_screen` state flags
- Actions: Emulate, Info, Rename, Delete
- Info loads `.sub` metadata via `flipper_subghz_load()`

**Infrared** (`m1_ir_universal.c`):
- `ir_file_action_menu()` called from `show_commands()` via LEFT button
- Actions: Send All, Info, Rename, Delete
- Returns `false` if file was renamed/deleted (caller exits to browse)

**NFC** (`m1_nfc.c`):
- `m1_nfc_more_options_file[]` array with view mode dispatch
- Actions: Emulate UID, Unlock, Edit UID, Card Actions, Info, Rename, Delete

**RFID** (`m1_rfid.c`):
- `m1_rfid_save_mode_options[]` array with view mode dispatch
- Actions: Emulate, Write, Edit, Rename, Delete, Info

### Rules for New Modules

1. **Always include all four core verbs** (Emulate/Send, Info, Rename, Delete)
2. **Never add "Back" as a menu item** — the hardware BACK button handles navigation
3. **Rename must preserve the file extension** — strip ext before VKB, re-append after
4. **Delete must confirm** — use `m1_message_box_choice()` with "OK / Cancel"
5. **Info screen is read-only** — dismiss on any button press (typically BACK)
6. **After Rename or Delete, return to the file browser** — the old path is invalid

---

## Post-Connection Navigation Pattern

> **This is the default UX pattern for any action that establishes a connection to
> a device, network, or peripheral.**  It applies to all WiFi and Bluetooth
> connection flows, and serves as the fallback pattern for other protocols (NFC,
> RFID, GPIO, etc.).  Individual protocols MAY override this pattern when a
> more sensible flow exists for their specific hardware — for example, NFC tag
> detection may jump directly to a tag-info screen instead of a generic
> "connected" menu.

### The pattern

When a user action successfully establishes a new connection (WiFi join,
Bluetooth pair, device handshake, etc.), the delegate scene **MUST** navigate
to a **Connected Menu** scene instead of showing a dismissible message and
returning to the previous screen.

The Connected Menu scene:
- **Title**: displays the connected device/network identifier (e.g. SSID,
  device name, tag UID) — not a generic "Connected" label.
- **Items**: lists only the actions available while connected (e.g. Status,
  Net Scan, Disconnect for WiFi; Info, Services, Disconnect for Bluetooth).
- **Guard**: checks that the connection is still active on entry
  (`on_enter`).  If the connection has dropped, pops back immediately.
- **BACK**: returns to the parent menu (the menu that led to the connection
  action), NOT to the connection delegate itself.

### Implementation — blocking delegate pattern

For blocking delegates that call a connection function, use the
`was_connected` / `m1_scene_replace` pattern:

```c
static void my_connect_on_enter(M1SceneApp *app)
{
    (void)app;
    bool was_connected = my_protocol_is_connected();

    my_protocol_connect_delegate();   /* blocking — shows UI internally */

    m1_esp32_deinit();   /* or protocol-specific teardown */
    app->running = true;

    if (!was_connected && my_protocol_is_connected()) {
        m1_scene_replace(app, MyProtocolSceneConnectedMenu);
        return;
    }
    m1_scene_pop(app);
}
```

**Key details:**
- Save `was_connected` **before** calling the delegate.
- Check the connection state **after** the delegate returns.
- Only navigate to the Connected Menu on a **false → true** transition.
  If the user was already connected (and did not disconnect/reconnect),
  pop normally — they were browsing, not connecting.
- Use `m1_scene_replace()`, not `m1_scene_push()`, so BACK from the
  Connected Menu returns to the parent menu rather than the (now stale)
  delegate.

### Current implementations

| Module | Connect scenes | Connected Menu scene | Guard function |
|--------|---------------|---------------------|----------------|
| **WiFi** | `WifiSceneScanConnect`, `WifiSceneSaved`, `WifiSceneGeneralJoin` | `WifiSceneConnectedMenu` (Status, Net Scan, Disconnect) | `wifi_is_connected()` / `wifi_require_connected()` |
| **Bluetooth** | _(not yet implemented)_ | _(planned)_ | _(planned)_ |

### Rules for new connection flows

1. **Every connection action must navigate to a Connected Menu on success.**
   Do not show a "Connected!" message box and return to the previous screen.
   The user expects to interact with the connected device immediately.

2. **The Connected Menu must guard against stale connections.**  If the
   connection drops between the delegate returning and the menu rendering
   (race, timeout, user-initiated disconnect from the menu itself), the
   menu's `on_enter` must detect this and pop back.

3. **The Connected Menu title must identify the target.**  Use the SSID,
   device name, MAC address, or other human-readable identifier — not a
   generic "Connected" string.  Fall back to "Connected" only when no
   identifier is available.

4. **Disconnect must be a menu item in the Connected Menu**, not buried in
   a sub-menu.  After disconnecting, the scene should pop back to the
   parent menu.

5. **Protocol-specific overrides are permitted.**  If a protocol's
   connection leads to a single obvious action (e.g. NFC tag read
   immediately shows tag info), the Connected Menu pattern may be skipped
   in favour of navigating directly to that action's scene.  Document the
   override reason in the scene file header.

6. **Use `m1_scene_replace()` for the transition**, not `m1_scene_push()`.
   The connection delegate is a transient action, not a persistent screen
   — it should not remain on the scene stack.

---

## UI / Button Bar Rules

> **Note:** The [Saved Item Actions Pattern](#saved-item-actions-pattern) above is the
> highest-priority UX standard.  The rules below still apply but are subordinate —
> if a saved-item action menu needs a specific layout, the Saved Item Actions pattern
> wins over generic button bar guidelines.

### Visual Standard: Momentum-style button bars and headers

All M1 scene UI must follow the **Momentum / Flipper-inspired visual standard**:

#### Bottom action bars (button bars)

- **Individual rounded-corner buttons** — each action slot is a separate filled
  rounded box (`u8g2_DrawRBox` with r=2), **not** a single solid bar spanning the
  full display width.
- **1 px gap between buttons** (background color shows through) so they appear
  visually distinct.
- **Inverted colours within each button** — filled dark box, white (background-colour)
  text and icons.
- **Slim font inside buttons** — use `M1_DISP_SUB_MENU_FONT_N`
  (`u8g2_font_NokiaSmallPlain_tf`) for button labels, never the heavy bold fonts.
- **Icon/text content is centered within each populated button**:
  - **LEFT slot**: center the combined icon+text block; icon first, text to its right.
  - **CENTER slot (OK)**: center the combined icon+text block; icon first, text to its right.
  - **RIGHT slot**: center the combined text+icon block; text first, icon to its right.
- **3-column bar** (`subghz_button_bar_draw`): each button is 42 px wide.
  Layout: `[42 px][1 px gap][42 px][1 px gap][42 px]` = 128 px.
- **2-column bar** (`m1_draw_bottom_bar`): each button is 63 px wide.
  Layout: `[63 px][2 px gap][63 px]` = 128 px.
  **`m1_draw_bottom_bar` has no OK/center column.** Its left slot maps to the
  LEFT button; its right slot maps to the RIGHT button.  If the primary action
  on a screen is triggered by pressing OK (e.g. "Select", "Set", "Confirm",
  "Connect"), do **NOT** use `m1_draw_bottom_bar()` — use `subghz_button_bar_draw()`
  with the center slot and the `ok_circle_8x8` icon instead:
  ```c
  /* Correct — OK action goes in the center slot */
  subghz_button_bar_draw(NULL, NULL, ok_circle_8x8, "Select", NULL, NULL);

  /* Wrong — left slot is the LEFT button, not OK */
  m1_draw_bottom_bar(&m1_u8g2, NULL, "Select", NULL, NULL);
  ```
- Draw a button only for **populated slots** — if both icon and text are NULL for a
  slot, leave that column empty (no empty box).

#### Dialog boxes (`m1_message_box_choice`)

- **Always use `u8g2_DrawRBox` (rounded corners, r=2)** for dialog button
  highlights.  Never use `u8g2_DrawBox` — square-corner buttons violate the
  Hapax UI standard.
- **Button label text must fit the allocated button width.**  Measure with
  `u8g2_GetStrWidth` and truncate or abbreviate if necessary.  Common
  abbreviations: "Disconnect" → "Discon.", "Configure" → "Config".
- **Title lines must not exceed 21 characters** at `u8g2_font_6x10_tr`
  (128px ÷ 6px/char ≈ 21).  Truncate or reword titles that would overflow
  the display width.
- **Each button label must be NUL-terminated individually** when measuring
  width.  The `\n`-delimited `buttons` string must be split before calling
  `u8g2_GetStrWidth` — passing the raw substring (which continues past `\n`
  to the next label) measures the wrong width.

#### Scene header / status bars

- **Black text on white background** — scene title headers and status bars (including
  the Sub-GHz frequency/modulation/state bar) use normal draw colour (black) on the
  default white background, **never** an inverted dark-filled background bar.
- A **1 px separator line** at the bottom of every header/status bar visually
  separates it from the scene body.
- **Scene menu titles** (`m1_scene_draw_menu`, `m1_subghz_scene_menu.c`, etc.) already
  follow this standard — they draw text in black and a `DrawHLine` separator at y=10.
- **Sub-GHz status bar** (`subghz_status_bar_draw`): draws black text (frequency,
  modulation, sample count) directly on white background with a `DrawHLine` at y=10.
  No inverted `DrawBox` is drawn.
- **Any scene that previously used an inverted title bar** (dark filled box + white
  text) must be updated to the non-inverted pattern before merging.

### Button-to-Column Mapping (3-Column Bar)

The `subghz_button_bar_draw()` API provides three columns: LEFT, CENTER,
RIGHT.  **Each column MUST correspond to its physical button:**

| Column | Physical button | Typical labels |
|--------|----------------|----------------|
| LEFT | LEFT button | Config, Erase, Stop |
| CENTER | OK button | REC, Stop, Send |
| RIGHT | RIGHT button | Save |

**NEVER** put a DOWN action label in the CENTER column or an OK action in
the RIGHT column.  This creates a confusing mismatch between what the user
sees on screen and what happens when they press a button.

If an action is triggered by UP or DOWN (which have no dedicated column),
either omit it from the button bar or use the LEFT column with a ↓ icon as
a visual hint.

#### Known violations (migration backlog)

All known button-to-column mapping violations have been fixed.

### General Button Bar Rules

- **NEVER add "Back" as a menu item or button bar label.** The back button is self-explanatory
  — users do not need a screen element telling them that pressing back goes back. This applies
  to `subghz_button_bar_draw()`, `m1_draw_bottom_bar()`, hand-drawn bottom bars, and any
  selection list / action menu array.
- **NEVER add "Back" as a selectable item in action menus.** If a menu has items like
  "Emulate / Rename / Delete", do NOT append "Back" — the hardware back button already
  handles navigation. Adding it wastes a menu slot and screen space.
- **Button bars should only show actionable hints** — labels for directional buttons (↓ Config,
  OK:Listen, LR:Change, etc.) that tell the user what *non-obvious* actions are available.
  Pass `NULL, NULL` for the left slot instead of showing a back arrow + label.
- If a bottom bar would be entirely empty after removing "Back", that is acceptable — an
  empty bar is better than a redundant one.
- **Selection lists MUST NOT have a button bar with "OK".** When a scene presents a list
  of selectable items (main menus, action menus, option lists), pressing OK to select is
  self-evident.  Do NOT draw `subghz_button_bar_draw()` or `m1_draw_bottom_bar()` with an
  "OK" label at the bottom — it wastes 12px of vertical space and looks wrong.  Instead,
  use that space for additional list items and draw a **scrollbar** on the right edge as a
  position indicator.  Reference implementation: `m1_subghz_scene_menu.c`.
- **Scrollbar pattern for selection lists**: Draw a 3px-wide track frame at x=125 spanning
  the menu area, with a filled handle whose Y position is proportional to the selected
  item index.  Limit the highlight-box width to 124px so it does not overlap the scrollbar.
- **Button bars are appropriate ONLY when they convey non-obvious functionality** — e.g.
  "OK:Browse" (tells user OK opens a file browser), "↓ Config" (tells user down opens
  config), "Send" / "Save" (action confirmation in radio scenes).
- **When adding or removing menu items, redistribute spacing.** If the item count changes,
  use `m1_menu_item_h()` and `m1_menu_max_visible()` — do NOT manually recompute row
  heights.  See [User-Configurable Font Size](#user-configurable-font-size--m1_menu_style).

---

## Scene-Based Application Architecture

**The scene manager is the target architecture for ALL M1 application modules.**
Every module that has more than one screen or any non-trivial navigation must use
scene-based architecture.  The `SubGhzSceneHandlers` pattern
(`on_enter` / `on_event` / `on_exit` / `draw` callbacks with stack-based push/pop
navigation) keeps state management clean, testable, and consistent across the device.

### Migration status

Scene managers are present in **all** application modules with submenus.  However
"scene-wrapped" (blocking delegate) and "scene-native/async" are meaningfully
different quality levels.  Sub-GHz is *scene-native/async*; the remaining modules
are *scene-wrapped* — their scene shells call legacy blocking functions that may
freeze the UI briefly while waiting on hardware.

All scene-wrapped modules now use the **Phase E submenu model** pattern
(`subghz_submenu_model_t` + `m1_submenu_draw()` + `m1_submenu_event()`).
The legacy `m1_scene_menu_event()` helper has zero callers and is retained only
for backward compatibility — **never use it in new code**.

| Module | Scene quality | Notes |
|--------|--------------|-------|
| **Sub-GHz** | ✅ Scene-native/async | 30 per-screen files; pure-logic units in `Sub_Ghz/`; async TX/RX flows; extensive host tests |
| **125KHz RFID** | ✅ Phase E submenu model | Good `lfrfid/` pure-logic separation + tests; 7 blocking `osDelay`/`vTaskDelay` calls |
| **NFC** | ✅ Phase E submenu model | 8 scene IDs in 1 file; 25 `vTaskDelay`, 40 `while` loops; pure-logic units in `NFC/` and `m1_csrc/nfc_*.c/h` |
| **Infrared** | ✅ Phase E submenu model | 6 scene IDs in 1 file; 12 `vTaskDelay`, 54 `while`; pure-logic units in `ir_signal_record.c/h`, `ir_button_map.c/h`, `flipper_ir.c/h` |
| **GPIO** | ✅ Phase E submenu model | 6–9 scene IDs; main + CAN Bus sub-menu; minimal blocking |
| **WiFi** | ✅ Phase E submenu model | 51 scene IDs split across 7 files; `HAL_Delay` eliminated; `CMD_WIFI_JOIN` async conversion N/A (SiN360 ESP32 has no async push) |
| **Bluetooth** | ✅ Phase E submenu model | 32 scene IDs split across 4 files; no hardware blocking |
| **BadUSB/BadBT** | — Single function | Intentionally no submenus; `osDelay` calls are legitimate script-pacing delays |
| **Games** | ✅ Phase E submenu model | 8 game delegates from scene-based menu |
| **ESL Tags** | ✅ Phase E submenu model | 2-item menu; IR-based Pricer ESL tag interaction |
| **Settings** | 🔶 Scene-wrapped | 21 scene IDs split across 5 files |

### Agent instructions for scene migration

Scene migration is complete for all modules.  There are no more legacy-menu
modules to migrate.  When working on **any** module, all new screens and features
**MUST** be implemented as scenes, not as standalone event-loop functions.

#### Phase E submenu model pattern (MANDATORY for all menus)

Every scene that presents a scrollable selection list **MUST** use the Phase E
pattern.  The legacy `m1_scene_menu_event()` / `m1_scene_draw_menu()` direct-call
pattern is deprecated — no new code may use it.

**Required pattern** (reference: `m1_subghz_scene_menu.c`, `m1_nfc_scene.c`):

1. `#include "m1_submenu.h"` (pulls in `subghz_submenu_model.h` + `m1_scene.h`)
2. Declare a file-static `subghz_submenu_model_t` model variable.
3. In `on_enter`: call `subghz_submenu_model_init()` with `M1_MENU_VIS(count)`.
4. In `on_event`: call `m1_submenu_event(app, event, &model, targets)`.
5. In `draw`: call `m1_submenu_draw(&model, "Title", labels)`.

Both `m1_submenu_event()` and `m1_submenu_draw()` auto-sync `visible_count` via
`M1_MENU_VIS()`, so the menu automatically adapts when the user changes the
text-size preference — no manual `subghz_submenu_model_set_visible_count()` call
is needed in the scene itself.

#### Mandatory import conversion plan (legacy menu code from other repos)

When importing UI code (Monstatek, Flipper, Momentum, SiN360, C3, or any fork),
do **not** preserve legacy submenu loops.  Convert imported menu flows to scene
style before merging:

1. Create/update the module `m1_<module>_scene.c` entry and scene ID enum.
2. Split each imported screen into `on_enter` / `on_event` / `on_exit` / `draw`
   handlers.
3. Keep hardware-heavy legacy functions only as blocking delegates called from
   scene wrappers until a full rewrite is justified.
4. Replace hardcoded list UI values with Hapax helpers (`m1_menu_font()`,
   `m1_menu_item_h()`, `m1_menu_max_visible()`, `M1_MENU_VIS()`).
5. Add new scene files to `cmake/m1_01/CMakeLists.txt` and host tests for any
   extracted pure logic touched by the import.
6. Verify no module regresses to legacy submenu definitions in `m1_menu.c`
   (`num_submenu_items` must remain `0` for scene-entry modules).

### Legacy functions still in m1_sub_ghz.c

After the dead-code cleanup, the only legacy functions remaining in `m1_sub_ghz.c`
are **actively called** as blocking delegates from their scene wrappers:

| Function | Called by | Notes |
|----------|-----------|-------|
| `sub_ghz_frequency_reader()` | `m1_subghz_scene_freq_analyzer.c` | Blocking delegate |
| `sub_ghz_spectrum_analyzer()` | `m1_subghz_scene_spectrum_analyzer.c` | Blocking delegate |
| `sub_ghz_rssi_meter()` | `m1_subghz_scene_rssi_meter.c` | Blocking delegate |
| `sub_ghz_freq_scanner()` | `m1_subghz_scene_freq_scanner.c` | Blocking delegate |
| `sub_ghz_weather_station()` | `m1_subghz_scene_weather_station.c` | Blocking delegate |
| `sub_ghz_brute_force()` | `m1_subghz_scene_brute_force.c` | Blocking delegate |
| `sub_ghz_replay_flipper_file()` | `m1_subghz_scene_saved.c` (PACKET/key emulate path) and `m1_subghz_scene_playlist.c` | Direct call for TX; RAW files go through Read Raw scene instead |

These are **not dead code** — they are the implementation behind scene wrappers.
Converting them to scene-native implementations is optional and can be done
incrementally.  Do **not** delete them.

### Rules for all scene menus

- **MUST use Phase E submenu model** — `subghz_submenu_model_t` + `m1_submenu_draw()` +
  `m1_submenu_event()`.  Never use the deprecated `m1_scene_menu_event()` or call
  `m1_scene_draw_menu()` directly from a scene.
- Scene menus use the scrollbar pattern (no "OK" button bar).
- Never add "Back" as a menu item or button bar label.
- When item count changes, verify `MENU_VISIBLE` and scrolling math.
- Every scene file must be added to `cmake/m1_01/CMakeLists.txt`.
- **All scrollable lists MUST use the font-aware helpers** (`m1_menu_item_h()`,
  `m1_menu_max_visible()`, `m1_menu_font()`) — see the
  [User-Configurable Font Size](#user-configurable-font-size--m1_menu_style) section.

---
