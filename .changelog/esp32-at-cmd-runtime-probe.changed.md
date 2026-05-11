**ESP32 capability detection: AT firmware now self-reports via stock `AT+CMD?`** —
the secondary capability probe in `m1_esp32_caps_init()` has been switched from
the custom `AT+GETSTATUSHEX` extension to the stock ESP-AT `AT+CMD?` command,
which is supported unchanged by every tracked AT firmware variant (bedge117,
dag, neddy299).  A small `(at_cmd_name → M1_ESP32_CAP_*)` mapping table in
`m1_csrc/m1_esp32_caps.c` (`s_at_cmd_cap_map[]`) translates the presence of
specific AT commands into capability bits — adding a new AT-side feature is now
a single-line edit instead of a curated profile macro.  The curated AT fallback
profile macros (`M1_ESP32_CAP_PROFILE_AT_BEDGE_DAG`, `_AT_NEDDY299`,
`_TRACKED_FALLBACK`) and the `m1_esp32_caps_parse_at_hex()` helper have been
removed.  When both probes fail, capability detection now fails closed
(bitmap=0, name `"Unknown (fallback)"`) so that feature gates do not grant
capabilities that cannot be verified.  Added `m1_esp32_caps_parse_at_cmd_list()`
pure-logic helper (header-inline, host-testable) plus
`m1_esp32_caps_at_cmd_response_valid()`, and 9 new unit tests covering
stock/bedge/neddy responses, quoted-substring anchoring, empty/NULL inputs,
unordered responses, and the response-validity detector.
