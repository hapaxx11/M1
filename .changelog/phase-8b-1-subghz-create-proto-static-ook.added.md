**Sub-GHz Create-from-scratch — static-OOK catalog (Phase 8b-1, pure logic)** —
extended `Sub_Ghz/subghz_create_proto.c/h` with the 12 static-OOK families
served today by the legacy "Add Manually" delegate: Princeton 433/315, Nice
FLO 12/24-bit, CAME 12/24-bit + CAME 868, Linear 300, GateTX 433, DoorHan
315/433, and Holtek HT12X.  Catalog count grew from 5 to 17 entries.  Each
new entry advertises `SUBGHZ_CREATE_FIELD_KEY` only (matching the Add
Manually UX of "pick a protocol, enter a hex key").  The `proto_name`
field carries the canonical registry name — fixing two latent bugs in the
legacy strchr-based name stripping (`Nice FLO 12b` → "Nice" and
`Gate TX 433` → "Gate") that today rely on the encoder's strstr() fallback.
Phase 8b-2 will scaffold the SetType scene that consumes this catalog;
Phase 8b-4 will retire the blocking `sub_ghz_add_manually()` delegate.

Host-only change — no firmware behavioural change yet.  35 host tests under
ASan+UBSan (up from 18) cover the extended catalog shape, per-protocol
metadata regression for all 12 new entries, key-range truncation for the
narrowest catalog entry (Linear 10-bit), unique-label invariant, and the
"freq sits in a supported ISM band" invariant.  All 69 host tests pass.
