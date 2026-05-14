Sub-GHz replay: `sub_ghz_replay_prepare_flipper()` now rejects NULL `sub_path` as invalid arguments instead of passing it to file-open conversion.

Sub-GHz replay: blocking wrapper now flushes `main_q_hdl` before returning surfaced allocation errors (`4/5`), preserving the documented legacy queue-flush contract on all return paths.
