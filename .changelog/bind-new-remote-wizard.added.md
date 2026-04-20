**Sub-GHz: Bind New Remote wizard** — new guided wizard for creating and binding
  brand-new rolling-code remotes to receivers directly from the M1.  Accessible via
  Sub-GHz → Bind Remote (13th menu item).  Supported protocols (all have
  `SubGhzProtocolFlag_PwmKeyReplay` — bind once, replay forever from Saved):
  CAME Atomo 433, Nice FloR-S 433, Alutech AT-4N 433, DITEC GOL4 433,
  KingGates Stylo4k 433.  The wizard generates an entropy-seeded random
  key, saves the `.sub` file to `0:/SUBGHZ/`, then walks the user
  through the receiver-side binding steps with optional countdown timers and inline
  one-press TX at each transmit step.  The generated file is immediately replayable
  from Sub-GHz → Saved with full counter-increment logic.
