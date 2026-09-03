**Weather Station: sensors are now actually captured, with a Flipper-style
  sensor list and detail view.** Reception fixes: the scene clears the raw
  record-mode flag on entry (a stale flag left by the Read scene diverted TIM1
  capture edges into the raw ring buffer and starved the decoder); decoding is
  scoped to weather-typed protocols while the scene is active, so a generic
  gate/remote protocol can no longer consume a weather burst first; weather
  packets are segmented with a 5 ms boundary instead of the generic 1.5 ms one,
  since weather PPM protocols encode bits as gaps of up to ~4 ms and every frame
  used to be split into undecodable fragments; a failed decode is retried with
  sliding start offsets, recovering frames that begin mid-buffer after a
  sync/preamble burst; and the per-modulation dwell is now 60 s instead of 4 s,
  because sensors re-transmit only every 30-60 s so the old window almost always
  fell between two bursts. Scanning still alternates AM/OOK and 2FSK
  automatically — FSK weather sensors (Bresser 5-in-1 / 6-in-1 and Fine Offset
  derivatives) are registered protocols — and no modulation selection is
  required from the user. Sensor fields (id, channel, button, battery,
  temperature, humidity) are now extracted with Flipper-compatible bit layouts
  and checksum validation for 19 protocols, instead of only the four that
  previously filled the shared weather struct. Captured sensors are listed one
  row per sensor (protocol, temperature, humidity, age) with UP/DOWN to scroll;
  OK opens a detail view showing protocol and bit count, serial, channel,
  button, battery state, the raw data word and a boxed temperature/humidity/age
  readout; BACK returns to the list.
