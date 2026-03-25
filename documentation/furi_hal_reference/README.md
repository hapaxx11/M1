<!-- See COPYING.txt for license details. -->

# Flipper One f100 `furi_hal` Reference Sources

This directory contains the complete `furi_hal` hardware abstraction layer implementation
from the **Flipper One MCU firmware**, target `f100` (RP2350 / Cortex-M33 coprocessor).

**Source repository:**  
`https://github.com/flipperdevices/flipperone-mcu-firmware`  
**Commit:** `29ada14951a34902bafaaad2be00e7b05774a414`  
**Path in source:** `targets/f100/furi_hal/`

---

## Purpose

These files are included as a **porting reference** for implementing Furi HAL services on
the M1's STM32H573 MCU.  They are **not compiled as-is** — they target the RP2350 SDK
(`hardware/gpio.h`, `hardware/i2c.h`, etc.) and cannot link against the STM32H5 toolchain
without adaptation.

Use them as the authoritative template for:

- API signatures and enum definitions that are identical across platforms
- Business logic that is platform-independent (e.g. `furi_hal_serial_control.c`,
  `furi_hal_nvm.c`, `furi_hal_power.c` sleep/wake cycle, `furi_hal_usb_cdc.c`)
- Documentation for each HAL module

---

## Files

| File | Description |
|------|-------------|
| `furi_hal.h` / `.c` | Top-level HAL init/deinit sequence |
| `furi_hal_clock.h` / `.c` | Clock tree management (ROSC, XOSC, PLL, SysTick) |
| `furi_hal_debug.h` / `.c` | GDB session detection, low-power debug enable/disable |
| `furi_hal_flash.h` / `.c` | Flash base address / page size abstraction |
| `furi_hal_gpio.h` / `.c` | GPIO init, alt-function, interrupts |
| `furi_hal_interrupt.h` / `.c` | Interrupt registration table (all RP2350 IRQs) |
| `furi_hal_memory.h` / `.c` | Heap region discovery |
| `furi_hal_nvm.h` / `.c` | Key-value NVM storage (wraps `kvs` library) |
| `furi_hal_os.h` / `.c` | FreeRTOS tick, tickless deep-sleep integration |
| `furi_hal_power.h` / `.c` | Insomnia counter, deep sleep entry/exit |
| `furi_hal_pwm.h` / `.c` | PWM output (GPIO-mapped slice/channel) |
| `furi_hal_resources.h` | GPIO pin declarations, InputPin/GpioPinRecord types |
| `furi_hal_resources.c` | Early GPIO reset |
| `furi_hal_resources_pins.c` | **F100 board pin assignments** (GPIO numbers) |
| `furi_hal_serial_types.h` | Serial ID/config enums |
| `furi_hal_serial_types_i.h` | Internal `FuriHalSerialHandle` struct |
| `furi_hal_serial.h` | Serial blocking/async/DMA TX/RX API |
| `furi_hal_serial.c` | UART driver (RP2350 UART0/UART1, DMA) |
| `furi_hal_serial_control.h` / `.c` | Serial arbiter thread (acquire/release/logging) |
| `furi_hal_spi_types.h` | SPI ID/mode enums |
| `furi_hal_spi_types_i.h` | Internal `FuriHalSpiHandle` struct |
| `furi_hal_spi.h` / `.c` | SPI master driver (DMA TX) |
| `furi_hal_i2c_types.h` | I2C bus/handle/event type definitions |
| `furi_hal_i2c.h` / `.c` | I2C master/slave transaction API |
| `furi_hal_i2c_config.h` / `.c` | I2C bus instances (control=I2C0 HW, main=PIO, cpu=I2C1 slave) |
| `furi_hal_usb_cdc.h` / `.c` | USB CDC send/receive (TinyUSB) |
| `furi_hal_version.h` / `.c` | Firmware version, UID, BLE MAC |
| `furi_hal_version_device.c` | HW target validation (`== 100` for F100) |

---

## Porting Notes: STM32H573 vs RP2350

When adapting these files for M1's STM32H573:

### GPIO (`furi_hal_gpio`)

| RP2350 concept | STM32H5 equivalent |
|---|---|
| `gpio_set_function(pin, func)` | `HAL_GPIO_Init()` with `Alternate` |
| `GPIO_FUNC_SIO` (pure GPIO) | `GPIO_MODE_OUTPUT_PP` / `GPIO_MODE_INPUT` |
| `gpio_set_slew_rate()` | `GPIO_SPEED_FREQ_*` in `GPIO_InitTypeDef` |
| `gpio_set_pulls()` | `GPIO_PULLUP` / `GPIO_PULLDOWN` |
| `gpio_set_irq_enabled()` | `HAL_NVIC_EnableIRQ()` + EXTI config |
| `GpioPin.pin` (uint) | Replace with `{GPIO_TypeDef* port; uint16_t pin}` |

The `GpioPin` struct and all `furi_hal_gpio_*` inline functions must be rewritten
to wrap `stm32h5xx_hal_gpio.h`.

### Interrupt (`furi_hal_interrupt`)

The `FuriHalInterruptId` enum maps RP2350 IRQ numbers.  For STM32H573 replace with
`IRQn_Type` values from `stm32h573xx.h`.  The pattern of `irq_set_exclusive_handler()`
+ `irq_set_enabled()` maps to `NVIC_SetVector()` + `HAL_NVIC_EnableIRQ()`.

The `FuriHalInterruptPriority` enum and `FuriHalInterruptPriorityKamiSama` concept map
directly to `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` — which M1 already uses.

### Serial (`furi_hal_serial`)

The RP2350 implementation uses `hardware/uart.h` UART peripherals with DMA.
On STM32H573 this maps to `USART1`/`USART2` with GPDMA.  The `FuriHalSerialControl`
arbiter thread in `furi_hal_serial_control.c` is **platform-independent** and can be
used with minimal changes (replace `hardware/uart.h` calls with HAL_UART equivalents).

### SPI (`furi_hal_spi`)

SPI Mode 1 (CPOL=0, CPHA=1) is what M1's ESP32-C6 bridge requires.  In
`FuriHalSpiTransferMode` that is `FuriHalSpiTransferMode1`.  The DMA TX pattern
mirrors what M1 already does in `m1_esp32_hal.c` — this implementation can serve
as a cleaner long-term replacement.

### Power / Sleep (`furi_hal_power`, `furi_hal_os`)

The `furi_hal_power_deep_sleep()` implementation is RP2350-specific (ROSC, clock
stop/restart).  On STM32H573, replace with `HAL_PWR_EnterSTOPMode()` / wakeup timer.
The `vPortSuppressTicksAndSleep()` hook in `furi_hal_os.c` is the correct FreeRTOS
integration point and the logic is directly portable.

### I2C (`furi_hal_i2c`)

Three buses on F100:
- `furi_hal_i2c_bus_control` → I2C0 hardware (400 kHz master, control plane)
- `furi_hal_i2c_bus_main` → PIO software I2C (main bus, no HW I2C available)
- `furi_hal_i2c_bus_cpu` → I2C1 hardware (slave, inter-CPU communication at address 0x70)

On STM32H573 replace PIO I2C with a second `HAL_I2C_*` master and slave with I3C0.

### NVM (`furi_hal_nvm`)

The `kvs` (key-value store) library is Flipper One–specific.  On M1, map to:
- FatFS file in `/CONFIG/` on the SD card (already used for `m1_fw_config`)
- Or a dedicated flash page using STM32H5 flash erase/write

### USB CDC (`furi_hal_usb_cdc`)

TinyUSB (`tusb.h`) is used on RP2350.  M1 uses STM32 USB middleware (`USB/`).
The `CdcCallbacks` struct and `furi_hal_cdc_send/receive` signatures can stay identical
with the underlying call sites replaced with `CDC_Transmit_FS()` / ring buffer reads.

---

## License

These files are adapted from the Flipper One MCU firmware:  
`https://github.com/flipperdevices/flipperone-mcu-firmware`  
Copyright (C) Flipper Devices Inc. — Licensed under **GPLv3**.

Any file in this directory that is ported into M1 firmware source must carry the
GPLv3 attribution comment and be listed in `README_License.md`.
