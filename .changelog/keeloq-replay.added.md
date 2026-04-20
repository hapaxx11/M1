**Sub-GHz: KeeLoq counter-mode rolling-code replay** — KeeLoq, Jarolift (Flipper
  format), and Star Line signals can now be replayed from Flipper `.sub` files when
  the manufacturer master key is present in `0:/SUBGHZ/keeloq_mfcodes` on the SD
  card.  The M1 derives the per-device key via Normal or Simple Learning, increments
  the 16-bit counter in the encrypted hop word using the full 528-round KeeLoq NLFSR,
  re-encrypts, and retransmits.  Export the keystore from a Flipper Zero using
  RocketGod's SubGHz Toolkit app.  The `Manufacture:` field in `.sub` files is now
  parsed so the correct master key is selected automatically.
