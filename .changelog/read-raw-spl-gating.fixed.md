**Sub-GHz Read Raw: SPL counter no longer ticks up from ambient noise** — the
  on-screen `N spl.` counter now follows the same signal-present gating as
  the waveform cursor, so it only advances while the cursor is scrolling
  forward.  Previously, ambient RF noise that survived the 80 µs ISR pulse
  filter would keep the displayed sample count climbing even when no real
  signal was present, giving the false impression that a capture was
  filling up.  Matches Momentum's behaviour.
