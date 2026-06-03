/* See COPYING.txt for license details. */

/*
 * m1_rgb_backlight.h
 *
 * RGB backlight driver for the M1 RGB Mod (SK6805-1515 NeoPixel, 2 LEDs on PD3).
 * State/animation logic is platform-independent; the hardware transport backend
 * in m1_rgb_backlight.c drives the SK6805 via GPIO bit-bang on STM32H573.
 */

#ifndef M1_RGB_BACKLIGHT_H_
#define M1_RGB_BACKLIGHT_H_

#include <stdint.h>
#include <stdbool.h>

#define RGB_BACKLIGHT_MAX_LEDS 16U

typedef enum {
    RGB_BACKLIGHT_MODE_OFF = 0,
    RGB_BACKLIGHT_MODE_STATIC,
    RGB_BACKLIGHT_MODE_BREATHING,
    RGB_BACKLIGHT_MODE_RAINBOW,
    RGB_BACKLIGHT_MODE_COUNT,
} rgb_backlight_mode_t;

typedef enum {
    RGB_BACKLIGHT_ORDER_GRB = 0,
    RGB_BACKLIGHT_ORDER_RGB,
} rgb_backlight_color_order_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_backlight_color_t;

typedef struct {
    uint8_t led_count;
    rgb_backlight_color_order_t color_order;
} rgb_backlight_hw_config_t;

void rgb_backlight_init(void);
void rgb_backlight_set_color(uint8_t r, uint8_t g, uint8_t b);
void rgb_backlight_set_brightness(uint8_t brightness);
void rgb_backlight_update(void);

/*
 * rgb_backlight_decide_action() - pure, host-testable gating decision for
 * rgb_backlight_update().  Single source of truth so the install-gating
 * cannot silently regress.
 *
 * RGB_BACKLIGHT_ACTION_SKIP is the regression guard for the boot-hang fix:
 * on a board with no SK6805 backlight, rgb_backlight_update() must do nothing
 * at all - it must never enter the SK6805 bit-bang path, which relies on
 * DWT->CYCCNT timing and previously could hang devices without a debugger.
 */
typedef enum {
    RGB_BACKLIGHT_ACTION_SKIP = 0,   /* not installed: do nothing */
    RGB_BACKLIGHT_ACTION_OFF,        /* installed, off/zero brightness: blank LEDs */
    RGB_BACKLIGHT_ACTION_RENDER,     /* installed and lit: render a frame */
} rgb_backlight_action_t;

static inline rgb_backlight_action_t
rgb_backlight_decide_action(bool installed, rgb_backlight_mode_t mode, uint8_t brightness)
{
    if (!installed)
    {
        return RGB_BACKLIGHT_ACTION_SKIP;
    }
    if (mode == RGB_BACKLIGHT_MODE_OFF || brightness == 0U)
    {
        return RGB_BACKLIGHT_ACTION_OFF;
    }
    return RGB_BACKLIGHT_ACTION_RENDER;
}

/**
 * Probe for a connected SK6805 chain by temporarily driving PD3 low through
 * a pull-down and reading the line level.  Returns true if a chain is detected.
 * After calling this function PD3 is left in input mode; call
 * rgb_backlight_init() to restore it to the SK6805 output configuration.
 */
bool rgb_backlight_detect(void);

/**
 * Hardware transport — writes one GRB frame to the SK6805 chain via GPIO
 * bit-bang on PD3.  Interrupts are fully disabled for the duration of the
 * transmission so the tight NeoPixel timing is not disturbed.
 */
void rgb_backlight_hw_write(const uint8_t *data, uint16_t len);

void rgb_backlight_set_mode(rgb_backlight_mode_t mode);
rgb_backlight_mode_t rgb_backlight_get_mode(void);

void rgb_backlight_set_installed(bool installed);
bool rgb_backlight_is_installed(void);

void rgb_backlight_set_config(const rgb_backlight_hw_config_t *cfg);
rgb_backlight_hw_config_t rgb_backlight_get_config(void);

rgb_backlight_color_t rgb_backlight_get_color(void);
uint8_t rgb_backlight_get_brightness(void);

void rgb_backlight_hsv_to_rgb(uint16_t hue_deg, uint8_t sat, uint8_t val,
                              uint8_t *out_r, uint8_t *out_g, uint8_t *out_b);
uint32_t rgb_backlight_rgb_to_grb(uint8_t r, uint8_t g, uint8_t b);
uint8_t rgb_backlight_breathing_brightness(uint32_t phase_ms,
                                           uint16_t period_ms,
                                           uint8_t max_brightness);
void rgb_backlight_rainbow_color(uint32_t phase_ms,
                                 uint16_t period_ms,
                                 uint8_t sat,
                                 uint8_t val,
                                 uint8_t *out_r,
                                 uint8_t *out_g,
                                 uint8_t *out_b);

#endif /* M1_RGB_BACKLIGHT_H_ */
