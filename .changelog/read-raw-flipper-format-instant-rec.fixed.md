**Sub-GHz Read Raw: instant recording start and Flipper-compatible output** — Radio now
  starts in passive listen mode when entering the Read Raw scene, so pressing REC gives
  immediate response with no ~100ms delay (Flipper/Momentum pattern). Recorded files are
  saved as Flipper-compatible `.sub` files (`RAW_Data:` keyword with signed timing values,
  `Preset: FuriHalSubGhzPresetOok650Async` header) and can be opened directly in Magellan
  and other Flipper-ecosystem tools. The animated sine wave in Start state now runs
  continuously at ~5 fps to clearly signal the scene is ready to record.
