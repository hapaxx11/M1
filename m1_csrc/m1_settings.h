/* See COPYING.txt for license details. */

/*
*
*  m1_settings.h
*
*  M1 Settings functions
*
* M1 Project
*
*/

#ifndef M1_SETTINGS_H_
#define M1_SETTINGS_H_

#include <stdint.h>

void menu_settings_init(void);
void menu_settings_exit(void);

void settings_lcd_and_notifications(void);
void settings_buzzer(void);
void settings_power(void);
void settings_about(void);
void settings_load_from_sd(void);
void settings_save_to_sd(void);
void settings_ensure_sd_folders(void);
uint8_t settings_parse_hex_color(const char *str, uint8_t *r, uint8_t *g, uint8_t *b);

#endif /* M1_SETTINGS_H_ */
