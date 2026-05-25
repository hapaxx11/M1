**Sub-GHz: KeeLoq-family button override now produces internally-consistent
packets** — `keeloq_encode_replay()` now synchronises the HOP plaintext button
bits (`[15:12]`) with the FIX-portion button nibble before re-encrypting, so a
key whose FIX button was mutated by `subghz_button_override_apply()` no longer
transmits a mismatched HOP-button. Real KeeLoq receivers reject packets with
inconsistent FIX vs HOP button bits, so before this fix, cycling buttons via
the Transmitter UI could produce a packet the receiver silently ignored. The
update is a no-op for untouched captures because FIX-button == HOP-button by
construction on a real remote.
