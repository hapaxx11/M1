**Infrared: optional external IR transmitter (HX-53)** — a new
**Settings → LCD and Notifications → External IR** toggle routes IR transmit to
an external HX-53 module on the expansion header (DAT on PA9 = TIM1_CH2 38 kHz
carrier, VCC on the +5_EXT 5 V rail) instead of the onboard emitter. Receive and
Learn still use the onboard receiver. Off by default. Because PA9/PA10 are the
USART1 debug-console pins, the `M1_EXT_IR_FREE_UART1` build option frees PA9 for
IR (UART debug console disabled; USB-CDC/qMonstatek RPC unaffected). Pressing OK
on the toggle runs a TX self-test (solid carrier + TIM1 register readout).
