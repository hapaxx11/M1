# Versioning Scheme (full rationale)

> Canonical versioning reference, extracted from CLAUDE.md. The slim CLAUDE.md
> keeps only a quick-reference; this file holds the full scheme.


- **`FW_VERSION_MINOR`** is our fork's generation number (currently `9`, matching SiN360's `0.9.x.x` scheme). This is NOT locked to Monstatek upstream — we own MINOR and RC.
- **`FW_VERSION_RC`** maps 1:1 to `M1_HAPAX_REVISION`. Both are the Hapax release counter. First release is `1`, incrementing with each CI build.
- **`FW_VERSION_MAJOR`** and **`FW_VERSION_BUILD`** remain `0` until Monstatek publishes a breaking change.
- **`Hapax` is the project codename**, NOT a version number.
- **`M1_HAPAX_REVISION`** in `m1_fw_update_bl.h` = the Hapax fork revision. Keep in sync with `FW_VERSION_RC`. Source-file default = `1` (= local build default). **CI auto-increments** by querying the latest published release tag before each build.
- **Display format**: `v{major}.{minor}.{build}.{rc}-Hapax.{hapax_revision}` — e.g. `v0.9.0.1-Hapax.1`, `v0.9.0.2-Hapax.2`, etc.
- **File/tag format**: `M1_Hapax_v{major}.{minor}.{build}.{rc}` — e.g. `M1_Hapax_v{ver}_wCRC.bin`, tag `v{ver}`. No `-Hapax.X` suffix in filenames or release tags.
- **CMake project name** is fully dynamic: `M1_Hapax_v{major}.{minor}.{build}.{rc}` — derived entirely at CMake configure time by reading the four `FW_VERSION_*` macros from `m1_fw_update_bl.h`. `CMakeLists.txt` is **never** patched by CI and never needs manual editing for a version bump. All output filenames (ELF, BIN, HEX, wCRC) derive from this automatically.
- **When bumping Hapax revision manually** (e.g. for a local build): update only `FW_VERSION_RC` and `M1_HAPAX_REVISION` in `m1_fw_update_bl.h` — `CMakeLists.txt` is not touched. The CI does this automatically.
- **RPC protocol**: `hapax_revision` is sent as a separate byte in `S_RPC_DeviceInfo`. qMonstatek conditionally appends the `-Hapax.X` suffix only when `hapax_revision > 0`, so stock Monstatek firmware displays without it.

For Flipper protocol import procedures (Sub-GHz, LF-RFID, NFC, IR), see
[`.github/skills/flipper-import/SKILL.md`](../.github/skills/flipper-import/SKILL.md).

For hardware capability assessment (which silicon is present, which features official firmware
does not expose), see
[`documentation/hardware_schematics.md`](../hardware_schematics.md).
Consult this file *secondarily* — source code and build config are primary truth.

---
