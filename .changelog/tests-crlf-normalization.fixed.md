**Testing: source-invariant ESP32 test suites tolerate CRLF checkouts** — the
  source-grep read_file() helpers normalise CRLF to LF, so the suites pass on
  Windows working trees with core.autocrlf=true, matching CI behaviour.
