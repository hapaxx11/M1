**Sub-GHz: PSA + Renault V1 brute-force key recovery (Proto Pirate)** — Proto
  Pirate → Brute Force attacks saved PSA TEA-mode (type 0x36) and Renault V1
  captures carrying `Key_2`: the PSA two-phase TEA search (2 × 2^24 candidates)
  and the Renault V1 Hitag2 IV search (2^18 candidates) run on a background task
  with a live progress bar and cancellation.  M1's decoders now identify
  PSA TEA-mode frames on capture and save both protocols in the
  ProtoPirate-compatible `Key`/`Key_2` layout.  A hit saves a
  `<name>_recovered.sub` patched with the recovered fields (PSA: Serial/Btn/Cnt/
  Type/Seed; Renault V1: Seed + Recovered), and for Renault V1 the recovered
  seed unlocks Emulate: a fresh frame with an incremented counter is
  synthesized and transmitted.  Ported from RocketGod-git/ProtoPirate.
