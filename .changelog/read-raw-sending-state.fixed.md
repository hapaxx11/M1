**Sub-GHz Read Raw: add missing Sending state** — the Read Raw scene was missing the
  TX/Sending state present in Momentum firmware. When the user pressed Send (OK in Idle),
  the blocking replay call ran while the screen still showed the Idle button bar
  (Erase/Send/Save). The screen now transitions to a dedicated Sending state before the
  blocking call, showing "TX..." in the waveform area and no action buttons — matching
  Momentum's TX state concept adapted for the M1's blocking-TX architecture.
