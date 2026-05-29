**Firmware Update: fix flashing from SD card** — `firmware_update_init()` no
  longer resets `fw_update_status` when a validated image is already loaded,
  so "Firmware update" now proceeds with the selected file instead of silently
  doing nothing. Added confirmation message after successful image validation
  and "No firmware loaded" feedback when starting without a file.
