**Sub-GHz: ProtoPirate TX encoders — PSA, Kia V6, VAG** — captured keyfobs of
  the three fully-decrypted ProtoPirate protocols can now be transmitted, not
  just decoded: PSA frames re-encrypt via the recovered TEA key material
  (Emulate on the Brute Force result screen, counter auto-advances); Kia V6
  (AES-128, keystore-gated) and VAG (AUT64/TEA, all four frame types, counter
  auto-increment) replay through the standard KEY pipeline with Btn/Fx and
  Key2/Serial/Cnt/Type persisted on capture save.  Includes the AUT64 encrypt
  port alongside the existing decrypt.  Ported from RocketGod-git/ProtoPirate.
