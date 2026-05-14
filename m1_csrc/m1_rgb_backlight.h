/* See COPYING.txt for license details. */

/*
 * m1_rgb_backlight.h
 *
 * RGB backlight state/animation driver for the upcoming M1 RGB Mod.
 * Hardware transport is intentionally abstracted because final accessory
 * pinout/protocol details are still in flux.
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

/**
 * Hardware transport hook — override this in the platform backend to
 * drive the actual LED strip.  The default (weak) implementation is a no-op.
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
