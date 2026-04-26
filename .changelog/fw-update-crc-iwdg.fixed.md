**Firmware update: fixed IWDG reset during SD-card flash CRC validation** — the
  pre-flash CRC loop in `firmware_update_get_image_file()` read the entire firmware
  image (~1 MB) in chunks without ever kicking the hardware watchdog, causing the
  device to reboot mid-validation on a 4-second IWDG window.  Added a
  `m1_wdt_reset()` call on each chunk iteration; the device now flashes reliably
  without spurious reboots.
