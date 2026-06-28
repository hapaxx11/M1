**Sub-GHz Read Raw: waveform now progresses at Momentum-equivalent rate** — The
  RSSI spectrogram cursor was advancing on every ISR edge event (thousands/sec
  during active recording), filling the 100-slot history buffer in milliseconds
  and making the waveform appear frozen despite samples being captured. The cursor
  now advances at ~100 ms/step matching Momentum's tick rate, so the waveform
  fills over ~10 s exactly as on Flipper Zero.
