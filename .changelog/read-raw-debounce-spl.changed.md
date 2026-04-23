**Sub-GHz Read Raw: Momentum-aligned debounce and sample label** — After each signal
  burst in both the passive-listen (Start) and Recording states, a short debounce window
  (~800 ms, 4 × 200 ms ticks) of low-RSSI gap columns is pushed to the waveform so that
  successive signals are visually separated by empty space, matching Momentum Read Raw.
  The status bar shows an incrementing full-integer "N spl." label during recording
  (e.g. "18851 spl."), matching the exact Momentum format including the trailing period.
  A new `ok_circle_8x8` (⊙) icon is shown on the center-column button hint in all three
  states: "⊙ REC" (Start), "⊙ Stop" (Recording), "⊙ Send" (Idle) — matching Momentum's
  `⊙ Stop` button bar.  The state machine already implements Stop → Idle correctly;
  pressing OK during Recording stops the capture and transitions to Idle where Erase,
  Send, and Save actions become available.
