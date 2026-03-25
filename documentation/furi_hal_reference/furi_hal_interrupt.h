#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Timer ISR */
typedef void (*FuriHalInterruptISR)(void* context);

typedef enum {
    // Timer IRQs
    FuriHalInterruptIdTimer0Irq0,
    FuriHalInterruptIdTimer0Irq1,
    FuriHalInterruptIdTimer0Irq2,
    FuriHalInterruptIdTimer0Irq3,
    FuriHalInterruptIdTimer1Irq0,
    FuriHalInterruptIdTimer1Irq1,
    FuriHalInterruptIdTimer1Irq2,
    FuriHalInterruptIdTimer1Irq3,
    // PWM IRQs
    FuriHalInterruptIdPwmWrap0,
    FuriHalInterruptIdPwmWrap1,
    // DMA IRQs
    FuriHalInterruptIdDmaChannel0,
    FuriHalInterruptIdDmaChannel1,
    FuriHalInterruptIdDmaChannel2,
    FuriHalInterruptIdDmaChannel3,
    // USBCTRL
    FuriHalInterruptIdUsbCtrl,
    // PIO0
    FuriHalInterruptIdPio0Irq0,
    FuriHalInterruptIdPio0Irq1,
    // PIO1
    FuriHalInterruptIdPio1Irq0,
    FuriHalInterruptIdPio1Irq1,
    // PIO2
    FuriHalInterruptIdPio2Irq0,
    FuriHalInterruptIdPio2Irq1,
    // IO_BANK0
    FuriHalInterruptIdIoBank0,
    FuriHalInterruptIdIoBank0Ns,
    // IO_QSPI
    FuriHalInterruptIdIoQspi,
    FuriHalInterruptIdIoQspiNs,
    // SIO
    FuriHalInterruptIdSioFifo,
    FuriHalInterruptIdSioBell,
    FuriHalInterruptIdSioFifoNs,
    FuriHalInterruptIdSioBellNs,
    FuriHalInterruptIdSioMtimecmp,
    // CLOCKS
    FuriHalInterruptIdClocks,
    // SPI
    FuriHalInterruptIdSpi0,
    FuriHalInterruptIdSpi1,
    // UART
    FuriHalInterruptIdUart0,
    FuriHalInterruptIdUart1,
    // ADC
    FuriHalInterruptIdAdcFifo,
    // I2C
    FuriHalInterruptIdI2c0,
    FuriHalInterruptIdI2c1,
    // OTP
    FuriHalInterruptIdOtp,
    // TRNG
    FuriHalInterruptIdTrng,
    // PROC CTI
    FuriHalInterruptIdProc0Cti,
    FuriHalInterruptIdProc1Cti,
    // PLL
    FuriHalInterruptIdPllSys,
    FuriHalInterruptIdPllUsb,
    // POWMAN
    FuriHalInterruptIdPowmanPow,
    FuriHalInterruptIdPowmanTimer,
    // SPARE IRQs
    FuriHalInterruptIdSpareIrq0,
    FuriHalInterruptIdSpareIrq1,
    FuriHalInterruptIdSpareIrq2,
    FuriHalInterruptIdSpareIrq3,
    FuriHalInterruptIdSpareIrq4,
    FuriHalInterruptIdSpareIrq5,

    // Service value
    FuriHalInterruptIdMax,
} FuriHalInterruptId;

typedef enum {
    FuriHalInterruptPriorityLowest = -3,
    FuriHalInterruptPriorityLower = -2,
    FuriHalInterruptPriorityLow = -1,
    FuriHalInterruptPriorityNormal = 0,
    FuriHalInterruptPriorityHigh = 1,
    FuriHalInterruptPriorityHigher = 2,
    FuriHalInterruptPriorityHighest = 3,
    FuriHalInterruptPriorityKamiSama = 6,
} FuriHalInterruptPriority;

void furi_hal_interrupt_init(void);
void furi_hal_interrupt_set_isr(FuriHalInterruptId index, FuriHalInterruptISR isr, void* context);
void furi_hal_interrupt_set_isr_ex(FuriHalInterruptId index, FuriHalInterruptPriority priority, FuriHalInterruptISR isr, void* context);
const char* furi_hal_interrupt_get_name(uint8_t exception_number);
uint32_t furi_hal_interrupt_get_time_in_isr_total(void);

#ifdef __cplusplus
}
#endif
