**USB/RPC: remaining C3.154 wedge fixes** — `vUsb2SerTask`'s three OUT-endpoint
  rearm sites now use a new `CDC_RearmRx()` that forces the CDC instance on the
  composite device before re-arming, clearing the paused flag only on
  `USBD_OK` (a stale MSC `classId` left over from a prior MSC transfer could
  otherwise arm the wrong endpoint and leave the serial port deaf); RPC USB
  transmit now also skips outright when the IN endpoint is still busy
  (`CDC_Transmit_Busy()`), not just when CDC isn't ready; a host-side USB bus
  reset/disconnect (qMonstatek reconnect without an MCU reboot) now bumps an
  RPC session epoch from the PCD ISR (`m1_rpc_usb_session_reset_from_isr()`) —
  the receive parser and `rpc_task` drop stale old-session parser state, a
  stale deferred command (NACKed so the host fails fast instead of hanging),
  and any open partial file write, while never dropping work that legitimately
  arrives in the new session; `HAL_PCD_ResetCallback` also restores
  `m1_USB_CDC_ready`/`m1_USB_MSC_ready` from `-1` so a reset without an
  intervening RESUME doesn't permanently wedge RPC TX.
