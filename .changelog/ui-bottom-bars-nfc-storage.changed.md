**UI: Modernize remaining full-width inverted bottom bars in NFC and Storage** —
Replace 15 hand-rolled `u8g2_DrawBox(0,52,128,12)` button bars in `m1_nfc.c`
(NFC read-done, write/wipe confirmations, info screens, fuzzer, unlock with
reader, NDEF view, write URL, cyborg detector) and 3 in `m1_storage.c`
(Mount / Unmount / Format confirmations) with the Momentum-style rounded
button bar via `m1_draw_bottom_bar()`.  Aligns these blocking-delegate
screens with the device-wide UI standard documented in `CLAUDE.md`.
Action labels have been shortened to fit the compact rounded slots (e.g.
`OK=Write` → `Write`, `Bk:Stop` → `Stop`); button mappings and flow are
preserved, and right-arrow actions now also accept the RIGHT button press.
