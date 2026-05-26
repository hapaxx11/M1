**Sub-GHz: Receiver history Remove Duplicates / Delete Old Signals toggles** —
two new rows in Sub-GHz → Config control how the receiver's signal history
ring behaves.  `Remove Dups` (default ON) keeps the existing Flipper-parity
merge-on-duplicate behaviour; turning it OFF makes every decode produce a
new history row.  `Delete Old` (default ON) keeps the existing
oldest-eviction policy when the 50-entry ring fills up; turning it OFF
preserves the oldest captures and drops new arrivals instead.  Both
settings persist across reboot via `settings.cfg`
(`subghz_remove_duplicates`, `subghz_delete_old_signals`).
