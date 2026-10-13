**Sub-GHz: PSA TEA brute-force key recovery (Proto Pirate)** — TEA-mode (type 0x36)
  PSA keyfob captures that cannot be decoded by the cipher-free Direct-XOR path are
  now identified on capture, saved with their `Key_2` field, and can be attacked
  from Proto Pirate → Brute Force: a two-phase TEA key search (2 × 2^24 candidates)
  runs on a background task with a live progress bar and cancellation, and a
  successful hit saves a `<name>_recovered.sub` patched with the recovered
  Serial/Btn/Cnt/Type/Seed fields (ported from RocketGod-git/ProtoPirate).
