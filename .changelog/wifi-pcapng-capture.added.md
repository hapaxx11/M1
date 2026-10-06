**ESP32: add host-side codec, client driver and PCAPNG encoder for
  MonstaShark-equivalent WiFi capture.** MtkCore's Native M1 SPI v1 transport
  exposes a monitor-mode packet-capture service (`0x0004`) that returns raw
  802.11 frames with per-frame RSSI/channel metadata; our fork previously had no
  transport for it. The new `m1_esp32_capture.h/.c` implements the CAPTURE
  service codec (START/POLL_READ/STOP request builders, the START-token and
  POLL_READ frame-record parsers, and a `GET_CAPABILITIES` capability gate) plus
  a thin client driver layered on `mtk_native_call`, and `wifi_pcapng.h/.c` is a
  pure-logic PCAPNG + radiotap encoder that turns those raw frames into a
  Wireshark-openable capture stream (SHB/IDB/EPB, `LINKTYPE_IEEE802_11_RADIOTAP`).
  Both modules are buffer-only (no new static RAM) and fully host-tested
  (`tests/test_esp32_capture.c`, `tests/test_wifi_pcapng.c`). Consistent with the
  native transport, the physical 1024-byte SPI exchange primitive and live
  session-to-SD capture wiring are deferred to an on-hardware follow-up; the
  codec, driver and encoder are complete and host-verified today.
