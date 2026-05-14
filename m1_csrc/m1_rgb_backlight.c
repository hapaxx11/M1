/* See COPYING.txt for license details. */

/*
 * m1_rgb_backlight.c
 *
 * RGB backlight driver for the M1 RGB Mod (SK6805-1515, 2 LEDs on PD3).
 *
 * Hardware backend: GPIO bit-bang on PD3 (GPIOD).  The SK6805 uses the same
 * single-wire NRZ protocol as WS2812B but operates at 3.3 V logic, making it
 * directly compatible with the STM32H573 without a level shifter.
 *
 * Color byte order: G-R-B (Green first, then Red, then Blue), MSB first.
 * Timing constants are calibrated for the 250 MHz Cortex-M33 core clock.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m1_rgb_backlight.h"

#if defined(STM32H573xx)
#include "stm32h5xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "m1_io_defs.h"

/* -------------------------------------------------------------------------
 * SK6805 hardware constants
 * PD3 is defined as PD3_Pin / PD3_GPIO_Port in m1_io_defs.h.
 * -------------------------------------------------------------------------*/
#define SK6805_PIN          PD3_Pin        /* GPIO_PIN_3  */
#define SK6805_PORT         PD3_GPIO_Port  /* GPIOD       */

/*
 * Bit timing at 250 MHz (4 ns / cycle).
 * rgb_bl_delay_cycles() burns exactly 3 CPU cycles per loop iteration via
 * the SUBS + BHI pair, so each unit represents 12 ns.
 *
 *   T1H  60 × 12 ns =  720 ns  (spec: 600–1000 ns)
 *   T1L  25 × 12 ns =  300 ns  (spec: 250–700  ns)
 *   T0H  20 × 12 ns =  240 ns  (spec: 200–400  ns)
 *   T0L  55 × 12 ns =  660 ns  (spec: 600–1000 ns)
 */
#define SK6805_T1H_CYCLES   60U
#define SK6805_T1L_CYCLES   25U
#define SK6805_T0H_CYCLES   20U
#define SK6805_T0L_CYCLES   55U
#define SK6805_RESET_US     60U   /* >50 µs reset latch */

static inline void rgb_bl_delay_cycles(uint32_t cycles)
{
    __asm volatile (
        "1: subs %[c], %[c], #3 \n"
        "   bhi  1b              \n"
        : [c] "+r" (cycles)
        :
        : "cc"
    );
}

static void rgb_bl_send_byte(uint8_t byte)
{
    const uint32_t pin_set   = SK6805_PIN;
    const uint32_t pin_clear = (uint32_t)SK6805_PIN << 16U;

    for (int8_t bit = 7; bit >= 0; bit--)
    {
        if (byte & (1U << bit))
        {
            SK6805_PORT->BSRR = pin_set;
            rgb_bl_delay_cycles(SK6805_T1H_CYCLES);
            SK6805_PORT->BSRR = pin_clear;
            rgb_bl_delay_cycles(SK6805_T1L_CYCLES);
        }
        else
        {
            SK6805_PORT->BSRR = pin_set;
            rgb_bl_delay_cycles(SK6805_T0H_CYCLES);
            SK6805_PORT->BSRR = pin_clear;
            rgb_bl_delay_cycles(SK6805_T0L_CYCLES);
        }
    }
}

static void rgb_bl_send_reset(void)
{
    HAL_GPIO_WritePin(SK6805_PORT, SK6805_PIN, GPIO_PIN_RESET);
    /* DWT busy-wait: 250 cycles/µs × reset duration */
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < (250U * SK6805_RESET_US)) {}
}

#endif /* STM32H573xx */

#define RGB_BACKLIGHT_DEFAULT_LED_COUNT     2U
#define RGB_BACKLIGHT_BREATHE_PERIOD_MS  1800U
#define RGB_BACKLIGHT_RAINBOW_PERIOD_MS  3000U

typedef struct {
    bool installed;
    uint8_t brightness;
    rgb_backlight_mode_t mode;
    rgb_backlight_color_t static_color;
    rgb_backlight_hw_config_t hw;
    uint32_t frame_phase_ms;
} rgb_backlight_state_t;

static rgb_backlight_state_t s_rgb = {
    .installed = false,
    .brightness = 128U,
    .mode = RGB_BACKLIGHT_MODE_OFF,
    .static_color = { .r = 0xFF, .g = 0xA0, .b = 0x40 },
    .hw = {
        .led_count = RGB_BACKLIGHT_DEFAULT_LED_COUNT,
        .color_order = RGB_BACKLIGHT_ORDER_GRB,
    },
    .frame_phase_ms = 0U,
};

/*
 * SK6805 bit-bang transport.
 *
 * Interrupts must be fully disabled for the entire transmission window.
 * The '0' bit high pulse is only ~240 ns, which is shorter than most ISR
 * latencies.  taskENTER_CRITICAL() is not sufficient here because it only
 * masks FreeRTOS-managed interrupts up to configMAX_SYSCALL_INTERRUPT_PRIORITY;
 * hardware interrupts at higher priority (TIM1 capture, SPI DMA) would still
 * fire and corrupt the waveform.
 */
void rgb_backlight_hw_write(const uint8_t *data, uint16_t len)
{
#if defined(STM32H573xx)
    __disable_irq();
    for (uint16_t i = 0U; i < len; i++)
    {
        rgb_bl_send_byte(data[i]);
    }
    __enable_irq();
    rgb_bl_send_reset();
#else
    (void)data;
    (void)len;
#endif
}

static void rgb_backlight_lock(void)
{
#if defined(STM32H573xx)
    taskENTER_CRITICAL();
#endif
}

static void rgb_backlight_unlock(void)
{
#if defined(STM32H573xx)
    taskEXIT_CRITICAL();
#endif
}

static uint32_t rgb_backlight_now_ms(void)
{
#if defined(STM32H573xx)
    return HAL_GetTick();
#else
    static uint32_t host_tick;
    host_tick += 16U;
    return host_tick;
#endif
}

static uint8_t apply_brightness(uint8_t value, uint8_t brightness)
{
    return (uint8_t)(((uint16_t)value * (uint16_t)brightness) / 255U);
}

static uint8_t clamp_led_count(uint8_t led_count)
{
    if (led_count == 0U)
    {
        return RGB_BACKLIGHT_DEFAULT_LED_COUNT;
    }
    if (led_count > RGB_BACKLIGHT_MAX_LEDS)
    {
        return RGB_BACKLIGHT_MAX_LEDS;
    }
    return led_count;
}

void rgb_backlight_hsv_to_rgb(uint16_t hue_deg, uint8_t sat, uint8_t val,
                              uint8_t *out_r, uint8_t *out_g, uint8_t *out_b)
{
    uint16_t hue = (uint16_t)(hue_deg % 360U);
    uint8_t region = (uint8_t)(hue / 60U);
    uint16_t remainder = (uint16_t)(hue % 60U);

    uint16_t p = ((uint16_t)val * (uint16_t)(255U - sat)) / 255U;
    uint16_t q = ((uint16_t)val * (uint16_t)(255U - ((uint16_t)sat * remainder) / 60U)) / 255U;
    uint16_t t = ((uint16_t)val * (uint16_t)(255U - ((uint16_t)sat * (60U - remainder)) / 60U)) / 255U;

    uint8_t r = 0U, g = 0U, b = 0U;
    switch (region)
    {
    case 0: r = val;        g = (uint8_t)t; b = (uint8_t)p; break;
    case 1: r = (uint8_t)q; g = val;        b = (uint8_t)p; break;
    case 2: r = (uint8_t)p; g = val;        b = (uint8_t)t; break;
    case 3: r = (uint8_t)p; g = (uint8_t)q; b = val;        break;
    case 4: r = (uint8_t)t; g = (uint8_t)p; b = val;        break;
    default:r = val;        g = (uint8_t)p; b = (uint8_t)q; break;
    }

    if (out_r) *out_r = r;
    if (out_g) *out_g = g;
    if (out_b) *out_b = b;
}

uint32_t rgb_backlight_rgb_to_grb(uint8_t r, uint8_t g, uint8_t b)
{
    return (((uint32_t)g << 16U) | ((uint32_t)r << 8U) | (uint32_t)b);
}

uint8_t rgb_backlight_breathing_brightness(uint32_t phase_ms,
                                           uint16_t period_ms,
                                           uint8_t max_brightness)
{
    if (period_ms < 2U || max_brightness == 0U)
    {
        return 0U;
    }

    uint32_t half = (uint32_t)period_ms / 2U;
    uint32_t phase = phase_ms % period_ms;
    uint32_t ramp = (phase < half) ? phase : (period_ms - phase);

    return (uint8_t)((ramp * max_brightness) / half);
}

void rgb_backlight_rainbow_color(uint32_t phase_ms,
                                 uint16_t period_ms,
                                 uint8_t sat,
                                 uint8_t val,
                                 uint8_t *out_r,
                                 uint8_t *out_g,
                                 uint8_t *out_b)
{
    if (period_ms == 0U)
    {
        rgb_backlight_hsv_to_rgb(0U, sat, val, out_r, out_g, out_b);
        return;
    }

    uint16_t hue = (uint16_t)((phase_ms % period_ms) * 360U / period_ms);
    rgb_backlight_hsv_to_rgb(hue, sat, val, out_r, out_g, out_b);
}

void rgb_backlight_set_config(const rgb_backlight_hw_config_t *cfg)
{
    rgb_backlight_lock();
    if (cfg)
    {
        s_rgb.hw.led_count = clamp_led_count(cfg->led_count);
        s_rgb.hw.color_order = cfg->color_order;
    }
    rgb_backlight_unlock();
}

rgb_backlight_hw_config_t rgb_backlight_get_config(void)
{
    return s_rgb.hw;
}

void rgb_backlight_set_installed(bool installed)
{
    rgb_backlight_lock();
    s_rgb.installed = installed;
    rgb_backlight_unlock();
}

bool rgb_backlight_is_installed(void)
{
    return s_rgb.installed;
}

bool rgb_backlight_detect(void)
{
#if defined(STM32H573xx)
    /*
     * Temporarily configure PD3 as an input with internal pull-down.
     * If an SK6805 chain is connected its data line floats high through the
     * first LED's internal pull-up, producing a HIGH reading.  A bare pad
     * stays LOW.  Allow 1 ms for the pull resistor to settle before sampling.
     * Note: called only from rgb_backlight_init() at startup — the 1 ms block
     * is acceptable in that context.
     */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitTypeDef g = {
        .Pin   = SK6805_PIN,
        .Mode  = GPIO_MODE_INPUT,
        .Pull  = GPIO_PULLDOWN,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_Init(SK6805_PORT, &g);
    HAL_Delay(1U);
    return (HAL_GPIO_ReadPin(SK6805_PORT, SK6805_PIN) == GPIO_PIN_SET);
#else
    return false;
#endif
}

void rgb_backlight_init(void)
{
    bool detected = false;

#if defined(STM32H573xx)
    /* Auto-detect SK6805 chain before configuring PD3 as output. */
    detected = rgb_backlight_detect();

    /* Configure PD3 as push-pull output at maximum speed for clean
     * NeoPixel waveform edges, then drive low (idle / reset state). */
    {
        GPIO_InitTypeDef g = {
            .Pin   = SK6805_PIN,
            .Mode  = GPIO_MODE_OUTPUT_PP,
            .Pull  = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
        };
        HAL_GPIO_WritePin(SK6805_PORT, SK6805_PIN, GPIO_PIN_RESET);
        HAL_GPIO_Init(SK6805_PORT, &g);
    }

    /* Enable DWT cycle counter used by the reset-pulse busy-wait. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
#endif

    rgb_backlight_lock();
    s_rgb.installed = detected;
    s_rgb.hw.led_count = clamp_led_count(s_rgb.hw.led_count);
    s_rgb.frame_phase_ms = 0U;
    rgb_backlight_unlock();

    rgb_backlight_update();
}

void rgb_backlight_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    rgb_backlight_lock();
    s_rgb.static_color.r = r;
    s_rgb.static_color.g = g;
    s_rgb.static_color.b = b;
    rgb_backlight_unlock();
}

rgb_backlight_color_t rgb_backlight_get_color(void)
{
    return s_rgb.static_color;
}

void rgb_backlight_set_brightness(uint8_t brightness)
{
    rgb_backlight_lock();
    s_rgb.brightness = brightness;
    rgb_backlight_unlock();
}

uint8_t rgb_backlight_get_brightness(void)
{
    return s_rgb.brightness;
}

void rgb_backlight_set_mode(rgb_backlight_mode_t mode)
{
    rgb_backlight_lock();
    s_rgb.mode = (mode < RGB_BACKLIGHT_MODE_COUNT) ? mode : RGB_BACKLIGHT_MODE_OFF;
    rgb_backlight_unlock();
}

rgb_backlight_mode_t rgb_backlight_get_mode(void)
{
    return s_rgb.mode;
}

void rgb_backlight_update(void)
{
    uint8_t led_count;
    rgb_backlight_mode_t mode;
    rgb_backlight_color_t color;
    uint8_t brightness;
    bool installed;
    rgb_backlight_color_order_t color_order;
    uint8_t local_frame[RGB_BACKLIGHT_MAX_LEDS * 3U];
    uint32_t now_ms;

    now_ms = rgb_backlight_now_ms();

    /* Narrow critical section: snapshot mutable state and advance phase */
    rgb_backlight_lock();
    led_count = s_rgb.hw.led_count;
    color_order = s_rgb.hw.color_order;
    mode = s_rgb.mode;
    color = s_rgb.static_color;
    brightness = s_rgb.brightness;
    installed = s_rgb.installed;
    s_rgb.frame_phase_ms = now_ms;
    rgb_backlight_unlock();

    if (led_count > RGB_BACKLIGHT_MAX_LEDS)
    {
        led_count = RGB_BACKLIGHT_MAX_LEDS;
    }

    if (!installed || mode == RGB_BACKLIGHT_MODE_OFF || brightness == 0U)
    {
        memset(local_frame, 0, led_count * 3U);
        rgb_backlight_hw_write(local_frame, (uint16_t)(led_count * 3U));
        return;
    }

    if (mode == RGB_BACKLIGHT_MODE_BREATHING)
    {
        uint8_t b = rgb_backlight_breathing_brightness(now_ms,
                                                       RGB_BACKLIGHT_BREATHE_PERIOD_MS,
                                                       brightness);
        color.r = apply_brightness(color.r, b);
        color.g = apply_brightness(color.g, b);
        color.b = apply_brightness(color.b, b);
    }
    else if (mode == RGB_BACKLIGHT_MODE_RAINBOW)
    {
        rgb_backlight_rainbow_color(now_ms,
                                    RGB_BACKLIGHT_RAINBOW_PERIOD_MS,
                                    255U,
                                    brightness,
                                    &color.r,
                                    &color.g,
                                    &color.b);
    }
    else
    {
        color.r = apply_brightness(color.r, brightness);
        color.g = apply_brightness(color.g, brightness);
        color.b = apply_brightness(color.b, brightness);
    }

    for (uint8_t i = 0U; i < led_count; i++)
    {
        uint8_t base = (uint8_t)(i * 3U);
        if (color_order == RGB_BACKLIGHT_ORDER_GRB)
        {
            local_frame[base + 0U] = color.g;
            local_frame[base + 1U] = color.r;
            local_frame[base + 2U] = color.b;
        }
        else
        {
            local_frame[base + 0U] = color.r;
            local_frame[base + 1U] = color.g;
            local_frame[base + 2U] = color.b;
        }
    }

    rgb_backlight_hw_write(local_frame, (uint16_t)(led_count * 3U));
}
