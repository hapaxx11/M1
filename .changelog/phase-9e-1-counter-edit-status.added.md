**Sub-GHz Phase 9e-1 — Counter-edit capability probe** — new pure-logic
accessor `subghz_signal_fields_counter_edit_status(protocol, *out_reason)`
classifies a protocol's counter-edit support as SUPPORTED (KeeLoq family),
DEFERRED with a documented blocker (Nice FloR-S, CAME Atomo, Alutech AT-4N,
Phoenix V2), or UNSUPPORTED.  The Saved → Signal Settings entry is now
offered for the four deferred protocols and displays the specific blocker
(e.g. "Nice FloR-S: HCS perm. table req.") so users can distinguish a
roadmap protocol from a wholly unsupported one.  KeeLoq-family editing
(Button / Counter / CounterMode) is unchanged.  7 new host tests; 72 host
tests passing under ASan+UBSan.
