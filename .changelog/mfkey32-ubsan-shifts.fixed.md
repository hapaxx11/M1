NFC: fix undefined signed left shifts in `mfkey32.c` by casting bit values to `uint32_t` before shifting, preventing UBSan runtime errors and the Host-side unit test timeout in CI.
