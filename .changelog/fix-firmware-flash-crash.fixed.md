**Firmware Update: fix crash when flashing STM32 firmware from SD card** —
  Scene lifecycle freed the file path buffer before use, causing a HardFault
  and watchdog reboot ~16 seconds after selecting the .bin file.
