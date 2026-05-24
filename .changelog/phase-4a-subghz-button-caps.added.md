**Sub-GHz: button-cycling capability lookup** — new pure-logic
  `subghz_button_caps` module maps a protocol name to its
  `(supports_cycling, button_count)` tuple. Covers KeeLoq (incl.
  Jarolift and Star Line), Nice FloR-S (16 codes), CAME Atomo/TWEE,
  Alutech AT-4N, KingGates Stylo4k, DITEC_GOL4, Scher-Khan, and Toyota.
  Phase 4a scaffold — wired into the firmware build but not yet
  consumed by the Transmitter scene.
