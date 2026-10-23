**ESP32: RPC transport hardening** — m1_esp32_init()/m1_esp32_deinit() now take
  the RPC transaction mutex with a bounded wait instead of portMAX_DELAY, so a
  stuck transaction can no longer wedge (de)init forever; the brain-CD3 WiFi scan
  worker stack grew from 512 to 768 words (the old margin sat ~200 B above the
  worst-case call chain), and the AP-scan UI wait is now bounded with a cancel +
  drain path instead of polling the worker forever.
