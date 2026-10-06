**WiFi sniffers now save MonstaShark-equivalent PCAPNG captures to SD.** When the
  ESP32 is on the RPC "M1 Link" transport, the packet sniffers (All / Beacon /
  Probe / Deauth / SAE / Pwnagotchi, via `wifi_sniffer_run`) open a capture
  session and write every raw 802.11 frame — with per-frame radiotap
  RSSI/channel metadata — to `capture/sniffNNN.pcapng` (auto-indexed 0..999,
  Wireshark-openable, `LINKTYPE_IEEE802_11_RADIOTAP`). The new
  `m1_pcap_capture.h/.c` session wraps the existing host-tested `wifi_pcapng`
  encoder over FatFS; frames are captured up to a 512-byte snaplen while the EPB
  `original_len` preserves the true frame length. The SD-write glue is fully
  host-tested through the stdio-backed FatFS stub (`tests/test_pcap_capture.c`),
  and the session is heap/buffer-only (no new static RAM; link RAM unchanged).
  The decoded-record sniffers (EAPOL and the binary-SPI `CMD_PKTMON_NEXT` path)
  do not deliver raw frames and are left unchanged.
