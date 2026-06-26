**LF RFID → Read stuck on "Reading" / no detection** — `lfrfid_read_hw_deinit()`
  could touch TIM3/TIM5 registers while their RCC clocks were gated, raising a
  bus fault → HardFault (which the fault handler spins on, freezing the read
  screen). Now enables those timer clocks defensively before access. Fix
  courtesy of **da-pingwing** (github.com/da-pingwing/M1_T-1000_RFID, GPL-3.0).
