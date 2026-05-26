**Sub-GHz: polymorphic Info-screen renderer (Phase 11-1 foundation)** —
  added a `SubGhzGetStringFn get_string` function-pointer slot to
  `SubGhzProtocolDef`, allowing each protocol to render its own
  human-readable "Signal Info" text instead of every consumer
  hard-coding a generic "Proto / Key / Bits / TE" layout.  Installed
  the first concrete renderer (`subghz_signal_format_keeloq_info`) on
  the KeeLoq, Star Line, and Jarolift entries — these files now show
  decomposed Serial / Button / EncHop fields on the Saved Info screen.
  All other parsed protocols fall through to the existing generic
  layout unchanged.  No firmware-functional behaviour change for any
  non-KeeLoq-family file.
