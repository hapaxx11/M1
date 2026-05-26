**Sub-GHz: SetKey hex-entry scene (Phase 8b-3)** — adds the
`SubGhzSceneSetKey` hex-digit editor scene pushed by the SetType
picker after the user picks a protocol. Renders a hex-digit editor
sized to the picked protocol's bit width, lets the user build a
64-bit key with UP/DOWN cycling a single digit and LEFT/RIGHT moving
the cursor, then writes a temp `.sub` and pushes the Transmitter
scene to fire the signal. Hex-digit editing logic extracted into a
new host-tested pure-logic module `Sub_Ghz/subghz_hex_editor.c/h`
(17 host tests under ASan+UBSan) so the same backing model can drive
the upcoming KeeLoq SetSerial / SetButton / SetCounter editor scenes.
