**Sub-GHz Read: frequency hopper is now region-aware** — The hopper used by the
  Read scene now selects its 6 dwell frequencies based on the user's ISM Region
  setting (Sub-GHz → Config → Region) rather than using a single global list.
  North America hops 315/345/390/433.92/434.42/915 MHz; Europe hops the SRD 433
  cluster and 868 MHz; Asia/APAC adds 330/345 MHz gate-remote frequencies; Region
  Off uses a wide cross-region fallback covering every major global band.
  No settings UI change is needed — the existing Region selector already controls
  the behaviour.
