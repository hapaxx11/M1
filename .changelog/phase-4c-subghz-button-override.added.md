**Sub-GHz: Transmitter button cycling actually mutates the transmitted
key (Phase 4c)** — pressing LEFT/RIGHT on the Transmitter scene's
READY screen for a rolling-code remote now re-encodes the OOK PWM
waveform with the selected button index, instead of only updating
the on-screen "Btn X/Y" indicator.  Supported on KeeLoq, Jarolift,
and Star Line (the protocols whose button is a plain-bit field that
the M1 encoder path honours).  For other protocols listed in the
button-cycling capability table (Nice FloR-S, CAME Atomo/TWEE,
Alutech AT-4N, KingGates Stylo4k, DITEC GOL4, Scher-Khan, Toyota)
the cycling UI is suppressed at scene entry until per-protocol
re-encoders land in a follow-up phase — preventing a misleading
indicator that has no effect on the transmitted signal.  New
pure-logic `subghz_button_override` module + 16-test host suite
under ASan+UBSan; wiring is gated by a per-prepare override slot
that the Flipper-format converter latches and clears for each
TX_START.
