**STM32 firmware update UX** — added "Erasing flash..." display during the
  multi-second erase phase, success/reboot confirmation dialog before bank swap,
  error dialogs for CRC mismatch / invalid image / file access failures, and
  fixed stale progress bar state that caused a missing progress display on retry
  after a failed update.
