Documentation: Added an *Async / Non-Blocking RTOS Best Practices* section to
  `CLAUDE.md` documenting the project preference for event-driven, non-blocking
  patterns in scene handlers and ISRs, the canonical `xQueueSendFromISR` →
  `Q_EVENT_*` → scene-loop pattern, forbidden patterns
  (`vTaskDelay`/`HAL_Delay`/spin-wait inside `on_event`), and the canonical
  follow-up plan for the Sub-GHz Read Raw async TX state machine
  (TX / TXRepeat / LoadKeyTX / LoadKeyTXRepeat — Momentum parity).
