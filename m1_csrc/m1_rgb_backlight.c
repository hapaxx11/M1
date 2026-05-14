/* See COPYING.txt for license details. */

/*
 * m1_rgb_backlight.c
 *
 * RGB backlight state/animation driver for the upcoming M1 RGB Mod.
 * Emits GRB/RGB frames through a weak transport hook so the final
 * hardware backend (GPIO bit-bang or SPI-DMA) can be swapped in
 * without changing UI/settings logic.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m1_rgb_backlight.h"

#if defined(STM32H573xx)
#include "stm32h5xx_hal.h"
#endif

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
    uint8_t frame[RGB_BACKLIGHT_MAX_LEDS * 3U];
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

__attribute__((weak)) void rgb_backlight_hw_write(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
}

static uint32_t rgb_backlight_lock(void)
{
#if defined(STM32H573xx) && defined(__arm__)
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
#else
    return 0U;
#endif
}

static void rgb_backlight_unlock(uint32_t key)
{
#if defined(STM32H573xx) && defined(__arm__)
    if ((key & 1U) == 0U)
    {
        __enable_irq();
    }
#else
    (void)key;
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

static void frame_set_pixel(uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t base = (uint8_t)(idx * 3U);
    if (s_rgb.hw.color_order == RGB_BACKLIGHT_ORDER_GRB)
    {
        s_rgb.frame[base + 0U] = g;
        s_rgb.frame[base + 1U] = r;
        s_rgb.frame[base + 2U] = b;
    }
    else
    {
        s_rgb.frame[base + 0U] = r;
        s_rgb.frame[base + 1U] = g;
        s_rgb.frame[base + 2U] = b;
    }
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
    uint32_t lock = rgb_backlight_lock();
    if (cfg)
    {
        s_rgb.hw.led_count = clamp_led_count(cfg->led_count);
        s_rgb.hw.color_order = cfg->color_order;
    }
    rgb_backlight_unlock(lock);
}

rgb_backlight_hw_config_t rgb_backlight_get_config(void)
{
    return s_rgb.hw;
}

void rgb_backlight_set_installed(bool installed)
{
    uint32_t lock = rgb_backlight_lock();
    s_rgb.installed = installed;
    rgb_backlight_unlock(lock);
}

bool rgb_backlight_is_installed(void)
{
    return s_rgb.installed;
}

void rgb_backlight_init(void)
{
    uint32_t lock = rgb_backlight_lock();
    s_rgb.hw.led_count = clamp_led_count(s_rgb.hw.led_count);
    s_rgb.frame_phase_ms = 0U;
    memset(s_rgb.frame, 0, sizeof(s_rgb.frame));
    rgb_backlight_unlock(lock);

    rgb_backlight_update();
}

void rgb_backlight_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t lock = rgb_backlight_lock();
    s_rgb.static_color.r = r;
    s_rgb.static_color.g = g;
    s_rgb.static_color.b = b;
    rgb_backlight_unlock(lock);
}

rgb_backlight_color_t rgb_backlight_get_color(void)
{
    return s_rgb.static_color;
}

void rgb_backlight_set_brightness(uint8_t brightness)
{
    uint32_t lock = rgb_backlight_lock();
    s_rgb.brightness = brightness;
    rgb_backlight_unlock(lock);
}

uint8_t rgb_backlight_get_brightness(void)
{
    return s_rgb.brightness;
}

void rgb_backlight_set_mode(rgb_backlight_mode_t mode)
{
    uint32_t lock = rgb_backlight_lock();
    s_rgb.mode = (mode < RGB_BACKLIGHT_MODE_COUNT) ? mode : RGB_BACKLIGHT_MODE_OFF;
    rgb_backlight_unlock(lock);
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
    uint32_t now_ms;

    uint32_t lock = rgb_backlight_lock();
    led_count = s_rgb.hw.led_count;
    mode = s_rgb.mode;
    color = s_rgb.static_color;
    brightness = s_rgb.brightness;
    installed = s_rgb.installed;
    now_ms = rgb_backlight_now_ms();
    s_rgb.frame_phase_ms = now_ms;

    if (led_count > RGB_BACKLIGHT_MAX_LEDS)
    {
        led_count = RGB_BACKLIGHT_MAX_LEDS;
    }

    if (!installed || mode == RGB_BACKLIGHT_MODE_OFF || brightness == 0U)
    {
        memset(s_rgb.frame, 0, led_count * 3U);
        rgb_backlight_unlock(lock);
        rgb_backlight_hw_write(s_rgb.frame, (uint16_t)(led_count * 3U));
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
        frame_set_pixel(i, color.r, color.g, color.b);
    }

    rgb_backlight_unlock(lock);
    rgb_backlight_hw_write(s_rgb.frame, (uint16_t)(led_count * 3U));
}
