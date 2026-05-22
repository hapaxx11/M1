**Sub-GHz: Princeton decoder now reports the actual detected TE** — previously
`ndecodeddelay` was hardcoded to 0, so captured Princeton signals displayed
`TE: 0 us`, omitted the `TE:` field when saved to `.sub`, and (most importantly)
replayed using the protocol-table default `te_short` instead of the actual
captured timing. Remotes whose TE differs from the registry default
(typical: 250–500 µs depending on the PT2262/SC5262 Rt resistor) now replay
with the correct timing, fixing receivers that ignored the replayed signal.

**Sub-GHz: TE display/save fallback for all other decoders** — `subghz_decenc_read()`
now populates `te` from the protocol-registry `te_short` when a decoder leaves
it at 0 (true for most non-Princeton OOK PWM decoders). This produces a
meaningful, non-zero `TE:` field in saved `.sub` files and on the Read/Info
screens, matching Flipper/Momentum display parity. TX timing is unchanged —
the replay path already used the same registry value as a fallback.
