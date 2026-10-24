**Sub-GHz: ProtoPirate Kia V6 and VAG decoders tolerate the trailing
  inter-packet gap** — the live packetizer appends the closing gap to the
  pulse buffer before decode, which previously invalidated the whole Kia V6
  stream (any unclassifiable post-sync pulse) and clipped VAG's final
  Manchester cell when the frame's last low pulse merged into the gap.
  Both decoders now treat a full window followed by an unclassifiable
  trailing duration as end-of-frame and pad the merged-away trailing
  cell(s), restoring decode of live captures.
