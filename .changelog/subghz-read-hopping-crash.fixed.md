**Sub-GHz Read: fix hard crash (HardFault) when frequency hopping is enabled** — `resume_rx()` called
  `subghz_retune_freq_hz_ext()` before `sub_ghz_rx_init_ext()`, causing TIM1 register accesses while
  the TIM1 RCC clock was disabled (left off by the preceding `stop_rx()` → `sub_ghz_rx_deinit()`).
  Guard `sub_ghz_rx_pause()` and `sub_ghz_rx_start()` to check the HAL channel state before
  accessing any TIM1 registers.
