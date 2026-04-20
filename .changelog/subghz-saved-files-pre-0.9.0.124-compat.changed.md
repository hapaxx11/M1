**Sub-GHz: ⚠ Migration note — Sub-GHz files saved before v0.9.0.124 must be recaptured** —
  Any `.sub` or `.sgh` signal file that was **saved by the Hapax firmware itself** in a
  build earlier than v0.9.0.124 contains a zeroed key value (`Key: 00 00 00 00 00 00 00 00`)
  and a missing frequency field. These files will load and display without error, but
  **emulation will fail silently** because the key that drives the OOK PWM waveform is zero.

  **Root causes (both fixed in v0.9.0.124):**

  1. The legacy save path (`subghz_save_history_entry`) did not copy the decoded key into
     the signal struct before writing it to disk — the key was always written as 0x0.
  2. The frequency field was empty because `snprintf("%.2f MHz", ...)` is a no-op under
     `--specs=nano.specs` (newlib-nano) without the `-u _printf_float` linker flag.

  **Who is affected:**
  - Files captured and saved using **any Hapax build prior to v0.9.0.124**.

  **Who is NOT affected:**
  - `.sub` / `.sgh` files saved by **C3.12, SiN360, or stock Monstatek v0.8.0.x** firmware —
    those files carry the correct key value and load/emulate correctly on Hapax v0.9.0.124+.
  - Files from the bundled **`subghz_database/`** signal library — these are pre-validated
    Flipper `.sub` format files and are unaffected.

  **Action required:** Delete all `.sub`/`.sgh` files saved by Hapax before v0.9.0.124
  and recapture the signals using v0.9.0.124 or later. The new save path writes the correct
  key, bit count, TE, and frequency every time.
