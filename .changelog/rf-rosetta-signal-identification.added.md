**Sub-GHz: passive signal identification (RF Rosetta)** — A new sensor-agnostic
  identification core fingerprints a captured signal by its physical
  characteristics (frequency band, modulation family, timing element,
  repetition count) and scores it against a protocol-signature database with
  security metadata (fixed / rolling / encrypted + plain-English notes). When a
  Read Raw capture matches no known protocol, the Decode Results screen now
  shows a best-guess category, confidence, and security posture instead of a
  bare "No protocols decoded", falling back to the modulation hint when no
  identity is confident. The database also covers the 2.4 GHz domains
  (BLE / WiFi / 802.15.4) for future cross-sensor use. Inspired by RF Rosetta.
