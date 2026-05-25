**Sub-GHz `subghz_keeloq_create_key()`: honour `*key_out = 0` contract on
`BAD_ARG`** — the header documents "on failure `*key_out` is set to 0", but the
NULL-`params` path was returning `KEELOQ_CREATE_BAD_ARG` without zeroing the
caller's output. The zero-out now happens immediately after the `key_out`
NULL-check, so every failure path that has a usable out pointer leaves it at
`0ULL` as documented.
