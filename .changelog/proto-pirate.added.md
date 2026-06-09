Proto Pirate rolling-code analysis toolkit integrated into Sub-GHz.
Adds "Proto Pirate" to the Sub-GHz main menu with three tools:
- **Receiver** — automotive-protocol live capture (reuses existing async Read scene)
- **Sub Decode** — offline `.sub` file analysis (reuses Saved → Decode Raw flow)
- **Timing Tuner** — async timing comparator: captures raw OOK pulses from the radio,
  classifies them into short/long classes, and compares avg/min/max against a built-in
  table of 25 automotive/garage protocol timing definitions (KeeLoq, Star Line,
  Princeton, CAME, Nice FLO, Chrysler, Subaru, Honda, Kia, PSA, VAG, and more).
  All updates are driven by 200 ms tick events — no blocking loops.
  Pure-logic timing analysis extracted to `subghz_proto_pirate_timing.c/h`; 23 host tests.
