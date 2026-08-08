**RPC: fixed corrupted CRC-16 table in the qMonstatek USB/WiFi protocol** — the
  M1 RPC frame CRC-16 table (`m1_csrc/m1_rpc.c`) had 46 wrong entries versus
  the standard CRC-16/CCITT-FALSE table. This caused qMonstatek to detect the
  device as "Legacy FW — Compatibility Mode" and made larger multi-byte RPC
  payloads (WiFi scan/AP records, connect parameters) fail CRC validation far
  more often than short commands, explaining persistent WiFi feature failures.
