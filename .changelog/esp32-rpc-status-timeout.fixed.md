**ESP32: RPC status poll now honours its timeout argument** —
  m1_esp32_rpc_rpc_status_timeout() ignored its timeout_sec parameter and always
  used a hardcoded 1 s budget.
