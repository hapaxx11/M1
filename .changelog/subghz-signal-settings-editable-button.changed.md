**Sub-GHz: editable button code in Signal Settings (KeeLoq family)** —
Pressing OK on a KeeLoq / Star Line / Jarolift `.sub` file in Saved →
Settings now opens the 4-bit Button editor.  On save, the file's 64-bit
Flipper key is reassembled with the new button value and rewritten,
preserving the original `Manufacture:` line so the replay path still
resolves the correct manufacturer master key.  Files without a
`Manufacture:` line round-trip unchanged.
