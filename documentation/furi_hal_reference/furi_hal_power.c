#include <furi.h>
#include <furi_hal.h>
#include <hardware/watchdog.h>
#include <hardware/clocks.h>
#include <hardware/pll.h>
#include <pico/platform/cpu_regs.h>
#include <furi_hal_clock.h>

#define TAG "FuriHalPower"

typedef struct {
    volatile uint8_t insomnia;
} FuriHalPower;

static volatile FuriHalPower furi_hal_power = {
    .insomnia = 0,
};

static volatile int wakeup_alarm_irq_num;

void furi_hal_power_reset(void) {
    watchdog_reboot(0, 0, 10);
}

uint16_t furi_hal_power_insomnia_level(void) {
    return furi_hal_power.insomnia;
}

void furi_hal_power_insomnia_enter(void) {
    FURI_CRITICAL_ENTER();
    furi_check(furi_hal_power.insomnia < UINT8_MAX);
    furi_hal_power.insomnia++;
    FURI_CRITICAL_EXIT();
}

void furi_hal_power_insomnia_exit(void) {
    FURI_CRITICAL_ENTER();
    furi_check(furi_hal_power.insomnia > 0);
    furi_hal_power.insomnia--;
    FURI_CRITICAL_EXIT();
}

bool furi_hal_power_sleep_available(void) {
    return furi_hal_power.insomnia == 0;
}

static void furi_hal_power_alarm_sleep_callback(uint alarm_id) {
    hardware_alarm_set_callback(alarm_id, NULL);
    hardware_alarm_unclaim(alarm_id);
}

static void furi_hal_power_sleep_run_from_dormant_source(void) {
    uint src_hz = XOSC_HZ;
    uint clk_ref_src = CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC;

    clock_configure(clk_ref, clk_ref_src, 0, src_hz, src_hz);
    clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF, 0, src_hz, src_hz);

    clock_stop(clk_adc);
    clock_stop(clk_usb);
#if PICO_RP2350
    clock_stop(clk_hstx);
#else
#error Unknown processor
#endif
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, src_hz, src_hz);

    pll_deinit(pll_sys);
    pll_deinit(pll_usb);
    furi_hal_clock_rosc_disable();
}

static void furi_hal_power_processor_deep_sleep(void) {
#ifdef __riscv
    uint32_t bits = RVCSR_MSLEEP_POWERDOWN_BITS;
    if(!get_core_num()) {
        bits |= RVCSR_MSLEEP_DEEPSLEEP_BITS;
    }
    riscv_set_csr(RVCSR_MSLEEP_OFFSET, bits);
#else
    scb_hw->scr |= ARM_CPU_PREFIXED(SCR_SLEEPDEEP_BITS);
#endif
}

static void furi_hal_power_goto_sleep(uint32_t delay_ms, hardware_alarm_callback_t callback) {
    clocks_hw->sleep_en0 = 0x0;
#if PICO_RP2350
    clocks_hw->sleep_en1 = CLOCKS_SLEEP_EN1_CLK_REF_TICKS_BITS | CLOCKS_SLEEP_EN1_CLK_SYS_TIMER0_BITS;
#else
#error Unknown processor
#endif
    wakeup_alarm_irq_num = hardware_alarm_claim_unused(true);
    hardware_alarm_set_callback(wakeup_alarm_irq_num, callback);
    absolute_time_t t = make_timeout_time_ms(delay_ms);
    if(hardware_alarm_set_target(wakeup_alarm_irq_num, t)) {
        hardware_alarm_set_callback(wakeup_alarm_irq_num, NULL);
        hardware_alarm_unclaim(wakeup_alarm_irq_num);
        return;
    }
    furi_hal_power_processor_deep_sleep();
    __WFI();
}

static void furi_hal_power_wokeup(void) {
    furi_hal_clock_rosc_enable();
    clocks_hw->sleep_en0 |= ~(0u);
    clocks_hw->sleep_en1 |= ~(0u);
    furi_hal_clock_init();
    if(hardware_alarm_is_claimed(wakeup_alarm_irq_num)) {
        hardware_alarm_set_callback(wakeup_alarm_irq_num, NULL);
        hardware_alarm_unclaim(wakeup_alarm_irq_num);
    }
}

uint32_t furi_hal_power_deep_sleep(uint32_t expected_idle_ticks) {
    uint64_t time_start = time_us_64();
    furi_hal_power_sleep_run_from_dormant_source();
    uint32_t calc_tick_to_ms = expected_idle_ticks * furi_kernel_get_tick_frequency() / 1000;
    furi_hal_power_goto_sleep(calc_tick_to_ms, &furi_hal_power_alarm_sleep_callback);
    furi_hal_power_wokeup();
    return (uint32_t)((time_us_64() - time_start) / 1000) * 1000 / furi_kernel_get_tick_frequency();
}
