**Sub-GHz: editable rolling counter for KeeLoq-family `.sub` files** —
  the per-file Signal Settings scene now sports a UP/DOWN selection
  cursor; OK on the Counter row opens a 16-bit hex editor seeded with the
  decoded counter and, on save, re-encrypts the HOP word with the
  resolved manufacturer key (preserving the lower 16 plaintext bits) and
  writes the file back with its `Manufacture:` line intact.  The Counter
  row is only reachable when the manufacturer key is present in the
  keystore and the learning mode is Normal or Simple; otherwise the
  existing `key?` placeholder is shown and the cursor cannot move to it.
