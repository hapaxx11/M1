Sub-GHz Read Raw async TX teardown and passive RX restore no longer reset the shared main event queue, preserving queued keypad/scene events after TX completion, abort, and start-failure recovery.

Sub-GHz async replay now enforces temp-file ownership by requiring `out_tmp_path` in `sub_ghz_replay_prepare_flipper()`, and re-surfaces async start allocation failures (codes 4/5) so callers can show a memory error.
