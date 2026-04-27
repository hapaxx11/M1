<!-- See COPYING.txt for license details. -->

# M1 Hapax — Workflow Guides

Practical step-by-step guides for common tasks on the M1.  Each guide explains
what the relevant tools actually do under the hood so you know *why* the steps
work, not just what buttons to press.

---

## Sub-GHz: Finding and Capturing an Unknown Signal

**Situation:** You know there is RF traffic in the area (a gate, a sensor, a
remote) but you don't know the exact frequency or protocol.  You want to
identify it and eventually capture or replay it.

---

### Understanding the three scanning tools

All three tools share the same four band presets and the same SI4463 radio
hardware.  They answer different questions.

#### Spectrum Analyzer — *"Where is RF energy right now?"*

Sweeps 128 evenly-spaced frequencies across the selected band (1 ms settle per
point) and draws a live bar graph.  Refreshes continuously.

- The bar height = received signal strength (RSSI) at that frequency slot.
- A small triangle marker tracks the peak bar.  The top line shows its exact
  frequency and RSSI in dBm.
- **Resolution depends on the band span.**  The 345–395 MHz preset spans
  50 MHz ÷ 128 bars ≈ 390 kHz per bar — don't expect to read channel-exact
  frequencies from it.  The 430–440 MHz preset spans 10 MHz ÷ 128 ≈ 78 kHz
  per bar and is much finer.

**Controls:**
| Button | Action |
|--------|--------|
| L / R | Cycle through the four band presets |
| ↑ / ↓ | Zoom span in / out around current center |
| OK | Re-center on the current peak |
| BACK | Exit |

**Use it to:** Visually confirm which band has activity.  A bar that spikes
repeatedly tells you something is transmitting there.

---

#### Freq Scanner — *"Which specific frequencies have ongoing traffic?"*

Sweeps the selected band at ~50 kHz steps and **accumulates a persistent hit
list** of every frequency that exceeds the –80 dBm threshold.  Hits within
50 kHz of each other are merged.  The counter (`x2`, `x12`, etc.) shows how
many sweeps have seen each frequency above threshold.

- **Higher count = more persistent activity.**  A garage door opener will show
  a low count (it only fires when pressed).  A weather sensor that transmits
  every 30 s will accumulate a high count if you leave the scanner running.
- The list is sticky — it keeps growing across sweeps until you clear it.
- Common North American ISM channels you may see: 303.875, 315.000, 345.000,
  390.000, 433.920 MHz.  These are all legitimate frequencies used by different
  device types, not noise.

**Controls:**
| Button | Action |
|--------|--------|
| L / R | Cycle through the four band presets (clears hits) |
| ↑ / ↓ | Scroll the hit list |
| OK | Clear all hits and start fresh |
| BACK | Exit |

**Use it to:** Find the *exact frequency* with the most persistent traffic.
Leave it running for 3–5 minutes, then look at which frequency has the highest
repeat count and strongest RSSI.  That is your candidate for Read.

---

#### Frequency Analyzer — *"What is the single strongest frequency at this moment?"*

Two-stage scan: a **coarse** pass checks every preset frequency + a 2 MHz grid
across the band; then a **fine** pass sweeps ±300 kHz around the coarse peak
at 20 kHz steps.  The result is displayed as a large MHz reading, accurate to
±10 kHz.  Refreshes every ~50 ms.

The RSSI bar at the bottom shows signal strength.  The vertical line through it
is the adjustable threshold — signals above that line trigger a buzzer beep and
a "Signal!" indicator.

**Controls:**
| Button | Action |
|--------|--------|
| L / R | Cycle through the four band presets |
| ↑ / ↓ | Raise / lower the detection threshold |
| OK | **Freeze / unfreeze display** (HOLD mode) |
| BACK | Exit |

> **Tip:** Press OK when you see a reading you want to record — HOLD freezes
> the display so you can read the exact frequency without it updating away.

**Use it to:** Confirm a specific frequency once Freq Scanner has identified a
candidate.  It's reactive (strongest signal right now), not historical.

---

### Recommended workflow: finding a signal you can capture

**Step 1 — Identify the active band with Spectrum Analyzer**

Open Spectrum Analyzer and use L/R to cycle through the four band presets.
Watch for bars that spike repeatedly or stay elevated.  Note which band it is
(e.g., the 345–395 MHz range shows sustained activity).

**Step 2 — Pin down the exact frequency with Freq Scanner**

Switch to Freq Scanner, select the same band with L/R, and let it run for
**at least 3 minutes** (longer is better for infrequent devices).  When you
come back, look at the hit list:
- Highest repeat count (`xN`) = most consistently active frequency
- Strongest RSSI (least negative dBm) = strongest signal at that frequency

The top entry by both metrics is your Read target.

**Step 3 — Confirm with Frequency Analyzer (optional but useful)**

Switch to Frequency Analyzer, select the same band, and trigger the device
(press the remote, open/close the gate, etc.).  The large frequency display
should jump to the same frequency Freq Scanner identified.  Press OK to hold
it and read the exact value to 3 decimal places (e.g., `433.920 MHz`).

**Step 4 — Capture with Read or Read Raw**

Go to **Sub-GHz → Read**.  Use the Config option (↓ button in the Read scene)
to select the frequency that Freq Scanner found.  If the exact frequency
appears in the preset list, select it directly.  If it doesn't, select the
closest preset and note that the decoder may still lock on (±20 kHz is usually
fine for static protocols).

- **Read** — actively decodes known protocols.  When a matching signal arrives
  you'll see the protocol name, key, and other fields.  Press OK to save.
- **Read Raw** — records the raw OOK waveform without decoding.  Useful when
  Read shows no result (unknown protocol, non-standard timing, etc.).  Press
  OK to start recording, OK again to stop, then OK to save the `.raw` file.

---

### Long-term / overnight capture

None of the three scanning tools is designed to run unattended — they sweep
continuously but don't save anything.  **Read Raw** is the right tool for
overnight capture:

1. Use the workflow above to identify the target frequency.
2. Open **Read Raw**, configure the frequency.
3. Press OK to start recording.
4. Leave it running.  Every burst that arrives is written into the ring buffer.
5. When you return, press OK to stop, then OK to save the `.raw` file to SD.

**Important limitations to know:**
- Read Raw saves a single continuous recording per session.
- The ring buffer is finite — extremely long sessions may wrap and overwrite old
  data.
- The file is only saved to SD when you press OK → Save; if the device
  powers off before that, the capture is lost.
- If you need to capture a specific protocol (not just the raw waveform), use
  **Read** instead and leave it on the target frequency; it will decode and
  display any matching frame as it arrives.

---

### Quick reference: which tool for which question

| Question | Tool |
|----------|------|
| Is anything active in this band? | Spectrum Analyzer |
| Which exact frequency has ongoing traffic? | Freq Scanner (let it run 3–5 min) |
| What is the strongest signal right this second? | Frequency Analyzer |
| Decode a known protocol | Read |
| Record raw waveform for analysis | Read Raw |
| Replay a previously captured signal | Saved → Emulate |

---

### Band preset reference

All three scanning tools share these four presets.

| Preset label | Center | Span | Step (Spectrum) | Notes |
|---|---|---|---|---|
| 300–315 MHz | 307 MHz | 15 MHz | ~117 kHz | NA security sensors, Magellan (319.5 MHz is in Freq Analyzer's 315-band, not this preset) |
| 345–395 MHz | 370 MHz | 50 MHz | ~390 kHz | NA car keyfobs, garage doors, 315/345/390 MHz devices |
| 430–440 MHz | 435 MHz | 10 MHz | ~78 kHz | EU/worldwide 433.920 MHz ISM — most common |
| 910–920 MHz | 915 MHz | 10 MHz | ~78 kHz | EU 868 MHz and NA 915 MHz IoT / Z-Wave |

> **Why doesn't 315 MHz appear in the Spectrum Analyzer 300–315 preset?**
> The preset spans 300–315 MHz (center 307, span 15 MHz).  315 MHz is at the
> upper edge of that range.  Most 315-band devices in North America actually
> transmit at 303.875 MHz, 310.000 MHz, or 315.000 MHz — all within the
> 300–315 preset.  Devices that use 345/390 MHz fall in the next preset.

---

*Last updated: 2026-04-27*
