**Sub-GHz KeeLoq: ensure build-time embedded manufacturer keys are visible
  to the new scene architecture** — the Set Manufacturer Key scene
  (Add Manually → KeeLoq family) and the Signal Settings counter-edit path
  now lazy-load the manufacturer-key store on first access, mirroring the
  guard in the legacy replay path. Without this, a cold-boot user opening
  these scenes before any saved KeeLoq file was loaded would see an empty
  manufacturer-key picker / "device key not resolvable" even when the
  firmware was built with a `KEELOQ_KEY_VAULT` (build-time embedded keys
  via `scripts/gen_keeloq_mfkeys_builtin.py`).
