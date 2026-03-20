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

void menu_settings_init(void);
void settings_load_and_apply(void);
void menu_settings_exit(void);
void settings_lcd_and_notifications(void);
void settings_buzzer(void);
void settings_power(void);
void settings_system(void);
void settings_about(void);
void setting_switch_bank(void);

#endif /* M1_SETTINGS_H_ */
