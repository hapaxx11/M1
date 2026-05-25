**Sub-GHz scene manager:** `subghz_scene_search_and_pop_to()` now copies the scene
stack into a byte buffer before passing it to the pure-logic helper — the previous
`(uint8_t *)app->scene_stack` cast scanned raw enum bytes and made stack searches
beyond the first entry fail, which broke Saved → Delete returning to the file list.
A target that is already on top of the stack is now reported as success instead of
"not found", so callers that fall back to `subghz_scene_pop()` no longer pop the
target itself.

**Sub-GHz scene manager:** `subghz_scene_push()`, `_pop()`, and `_replace()` now
reset the tick cadence BEFORE invoking the new scene's `on_enter()` handler.  The
previous order cleared the tick period immediately after the scene opted in via
`subghz_scene_set_tick_period()`, disabling animation and periodic tick handling
on the Transmitter and other scenes that schedule ticks during entry.

**Sub-GHz Read scene:** When the receiver history ring is full and "Delete Old
Signals" is disabled, dropped decodes no longer re-save an unrelated old entry.
The Read scene now detects whether `subghz_history_add_ex()` actually inserted
or merged the new decode and skips selection, autosave, and feedback otherwise.
