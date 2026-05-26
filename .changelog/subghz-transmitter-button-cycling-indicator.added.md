**Sub-GHz Transmitter scene: button-cycling capability plumbing (Phase 4b)** —
the Transmitter scene now queries `subghz_button_caps_for_protocol()` on entry
using the protocol name supplied by the caller (`SubGhzApp::tx_protocol_name`)
and surfaces an on-screen "Btn X/Y" indicator with LEFT/RIGHT cycle hints for
rolling-code protocols that M1 can re-encode (KeeLoq family, Nice FloR-S, CAME
Atomo/TWEE, Alutech AT-4N, KingGates Stylo4k, DITEC GOL4, Scher-Khan, Toyota).
The Saved scene and Bind Wizard populate the protocol name from the loaded
signal so cycling is enabled there; Playlist and Remote leave it empty since
their UX is automated playback / one-shot fire.  Key re-encoding for the
selected button still lands in Phase 4c — for now LEFT/RIGHT only updates the
visible button index.
