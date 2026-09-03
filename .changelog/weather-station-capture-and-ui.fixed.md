Weather Station: fixed reception so sensors are actually captured, and reworked
the scene into a Flipper-style sensor list plus detail view.

Reception fixes:
- The scene now clears the raw record-mode flag on entry; a stale flag left by
  the Read scene diverted TIM1 capture edges into the raw ring buffer and
  starved the decoder.
- Decoding is scoped to weather-typed protocols while the scene is active, so a
  generic gate/remote protocol can no longer consume a weather burst first (the
  scene then discarded it for not being weather-typed).
- Weather packets are segmented with a 5 ms boundary instead of the generic
  1.5 ms one; weather PPM protocols encode bits as gaps of up to ~4 ms, so every
  frame used to be split into undecodable fragments.
- A failed weather decode is retried with sliding start offsets, recovering
  frames that begin mid-buffer after a sync/preamble burst.
- The dwell scan no longer wastes half its time on 2FSK: every supported
  weather protocol is OOK/AM at 433.92 MHz.

New decoding: sensor fields (id, channel, button, battery, temperature,
humidity) are now extracted with Flipper-compatible bit layouts and checksum
validation for 19 protocols, instead of only the four that previously filled
the shared weather struct.

New UI: captured sensors are listed one row per sensor (protocol, temperature,
humidity, age) with UP/DOWN to scroll; OK opens a detail view showing protocol
and bit count, serial, channel, button, battery state, the raw data word and a
boxed temperature/humidity/age readout; BACK returns to the list.
