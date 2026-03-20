/* See COPYING.txt for license details. */

/*
*
*  m1_settings.c
*
*  M1 RFID functions
*
* M1 Project
*
*/

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_settings.h"
#include "m1_buzzer.h"
#include "m1_lp5814.h"
#include "ff.h"
#include "u8g2.h"

/*************************** D E F I N E S ************************************/

#define SETTING_ABOUT_CHOICES_MAX		2 //5

#define ABOUT_BOX_Y_POS_ROW_1			10
#define ABOUT_BOX_Y_POS_ROW_2			20
#define ABOUT_BOX_Y_POS_ROW_3			30
#define ABOUT_BOX_Y_POS_ROW_4			40
#define ABOUT_BOX_Y_POS_ROW_5			50
#define SETTINGS_FILE_PATH  "settings.cfg"

//************************** S T R U C T U R E S *******************************

/* Persistent settings */
static uint8_t s_brightness_level = 4;
static uint8_t s_buzzer_on = 1;
static uint8_t s_led_on = 1;
static uint8_t s_orientation = 0;

static void settings_save(void)
{
    FIL fil;
    if (f_open(&fil, SETTINGS_FILE_PATH, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return;
    char buf[64];
    snprintf(buf, sizeof(buf), "brightness=%d\nbuzzer=%d\nled=%d\norient=%d\n",
             s_brightness_level, s_buzzer_on, s_led_on, s_orientation);
    UINT bw;
    f_write(&fil, buf, strlen(buf), &bw);
    f_close(&fil);
}

static void settings_load(void)
{
    FIL fil;
    if (f_open(&fil, SETTINGS_FILE_PATH, FA_READ) != FR_OK)
        return;
    char line[32];
    while (f_gets(line, sizeof(line), &fil) != NULL)
    {
        int val;
        if (sscanf(line, "brightness=%d", &val) == 1) s_brightness_level = (val > 4) ? 4 : val;
        else if (sscanf(line, "buzzer=%d", &val) == 1) s_buzzer_on = val ? 1 : 0;
        else if (sscanf(line, "led=%d", &val) == 1) s_led_on = val ? 1 : 0;
        else if (sscanf(line, "orient=%d", &val) == 1) s_orientation = (val > 2) ? 0 : val;
    }
    f_close(&fil);
}

void settings_load_and_apply(void)
{
    settings_load();
    static const uint8_t bv[] = {0, 64, 128, 192, 255};
    lp5814_backlight_on(bv[s_brightness_level]);
    m1_screen_orientation = s_orientation;
    if (s_orientation == M1_ORIENT_SOUTHPAW) u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R0);
    else if (s_orientation == M1_ORIENT_REMOTE) u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R1);
}
/***************************** V A R I A B L E S ******************************/

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

void menu_settings_init(void);
void menu_settings_exit(void);
void settings_system(void);
void settings_about(void);
void setting_switch_bank(void);
static void settings_about_display_choice(uint8_t choice);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/


/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void menu_settings_init(void)
{
	;
} // void menu_settings_init(void)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void menu_settings_exit(void)
{
	;
} // void menu_settings_exit(void)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void settings_lcd_and_notifications(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;

    #define LCD_SETTINGS_ITEMS  4
    #define LCD_SET_BRIGHTNESS  0
    #define LCD_SET_BUZZER      1
    #define LCD_SET_LED         2
    #define LCD_SET_ORIENT      3
    static const char *lcd_labels[] = {"Brightness", "Buzzer", "LED Notify", "Orientation"};
    static const char *orient_text[] = {"Normal", "Southpaw", "Remote"};
	/* Load saved settings from SD on first entry */
    static uint8_t first_load = 1;
    if (first_load)
    {
        settings_load();
        first_load = 0;
        /* Apply orientation */
        m1_screen_orientation = s_orientation;
        if (s_orientation == M1_ORIENT_SOUTHPAW) u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R0);
        else if (s_orientation == M1_ORIENT_REMOTE) u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R1);
        /* Apply brightness */
        static const uint8_t bv[] = {0, 64, 128, 192, 255};
        lp5814_backlight_on(bv[s_brightness_level]);
    }
    uint8_t brightness_level = s_brightness_level;
    uint8_t buzzer_on = s_buzzer_on;
    uint8_t led_on = s_led_on;
    static const uint8_t brightness_values[] = {0, 64, 128, 192, 255};
    static const char *brightness_text[] = {"Off", "Low", "Med", "High", "Max"};
    uint8_t sel = 0;
    uint8_t needs_redraw = 1;

    while (1)
    {
        if (needs_redraw)
        {
            needs_redraw = 0;
            char line[32];

            u8g2_FirstPage(&m1_u8g2);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 1, 10, "LCD & Notifications");

            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            uint8_t visible_start = 0;
            if (sel > 1 && LCD_SETTINGS_ITEMS > 3)
                visible_start = (sel - 1 > LCD_SETTINGS_ITEMS - 3) ? LCD_SETTINGS_ITEMS - 3 : sel - 1;
            for (uint8_t vi = 0; vi < 3 && (visible_start + vi) < LCD_SETTINGS_ITEMS; vi++)
            {
                uint8_t i = visible_start + vi;
                uint8_t y = 24 + vi * 12;
                const char *value = "";
                if (i == LCD_SET_BRIGHTNESS) value = brightness_text[brightness_level];
                else if (i == LCD_SET_BUZZER) value = buzzer_on ? "On" : "Off";
                else if (i == LCD_SET_LED) value = led_on ? "On" : "Off";
                else if (i == LCD_SET_ORIENT) value = orient_text[m1_screen_orientation];

                if (i == sel)
                {
                    u8g2_DrawBox(&m1_u8g2, 0, y - 9, 128, 11);
                    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
                    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
                    snprintf(line, sizeof(line), "< %s: %s >", lcd_labels[i], value);
                    u8g2_DrawStr(&m1_u8g2, 4, y, line);
                    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
                    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
                }
                else
                {
                    snprintf(line, sizeof(line), "  %s: %s", lcd_labels[i], value);
                    u8g2_DrawStr(&m1_u8g2, 4, y, line);
                }
            }

            u8g2_DrawBox(&m1_u8g2, 0, 52, 128, 12);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 2, 61, "U/D=Sel L/R=Change");
            m1_u8g2_nextpage();
        }

        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE) continue;
        if (q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

        ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
        if (ret != pdTRUE) continue;

        if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            s_brightness_level = brightness_level;
            s_buzzer_on = buzzer_on;
            s_led_on = led_on;
            s_orientation = m1_screen_orientation;
            settings_save();
            xQueueReset(main_q_hdl);
            break;
        }
        if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = (sel == 0) ? (LCD_SETTINGS_ITEMS - 1) : (sel - 1);
            needs_redraw = 1;
        }
        if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = (sel + 1) % LCD_SETTINGS_ITEMS;
            needs_redraw = 1;
        }
        if (this_button_status.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (sel == LCD_SET_BRIGHTNESS)
            {
                brightness_level = (brightness_level == 0) ? 4 : (brightness_level - 1);
                lp5814_backlight_on(brightness_values[brightness_level]);
            }
            else if (sel == LCD_SET_BUZZER)
                buzzer_on = !buzzer_on;
            else if (sel == LCD_SET_LED)
                led_on = !led_on;
            else if (sel == LCD_SET_ORIENT)
            {
                m1_screen_orientation = (m1_screen_orientation == 0) ? 2 : (m1_screen_orientation - 1);
                if (m1_screen_orientation == M1_ORIENT_NORMAL) u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R2);
                else if (m1_screen_orientation == M1_ORIENT_SOUTHPAW) u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R0);
                else u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R1);
            }
            needs_redraw = 1;
        }
        if (this_button_status.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (sel == LCD_SET_BRIGHTNESS)
            {
                brightness_level = (brightness_level >= 4) ? 0 : (brightness_level + 1);
                lp5814_backlight_on(brightness_values[brightness_level]);
            }
            else if (sel == LCD_SET_BUZZER)
            {
                buzzer_on = !buzzer_on;
                if (buzzer_on) m1_buzzer_notification(); /* Test sound */
            }
            else if (sel == LCD_SET_LED)
                led_on = !led_on;
            else if (sel == LCD_SET_ORIENT)
            {
                m1_screen_orientation = (m1_screen_orientation + 1) % 3;
                if (m1_screen_orientation == M1_ORIENT_NORMAL) u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R2);
                else if (m1_screen_orientation == M1_ORIENT_SOUTHPAW) u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R0);
                else u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R1);
            }
            needs_redraw = 1;
        }
    }
} // void settings_lcd_and_notifications(void)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void settings_buzzer(void)
{
	//buzzer_demo_play();
} // void settings_sound(void)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void settings_power(void)
{
	;
} // void settings_power(void)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void settings_system(void)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;

    /* Graphic work starts here */
    u8g2_FirstPage(&m1_u8g2); // This call required for page drawing in mode 1
    do
    {
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);

		u8g2_DrawStr(&m1_u8g2, 6, 25, "SYSTEM...");

    } while (u8g2_NextPage(&m1_u8g2));

	while (1 ) // Main loop of this task
	{
		;
		; // Do other parts of this task here
		;

		// Wait for the notification from button_event_handler_task to subfunc_handler_task.
		// This task is the sub-task of subfunc_handler_task.
		// The notification is given in the form of an item in the main queue.
		// So let read the main queue.
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret==pdTRUE)
		{
			if ( q_item.q_evt_type==Q_EVENT_KEYPAD )
			{
				// Notification is only sent to this task when there's any button activity,
				// so it doesn't need to wait when reading the event from the queue
				ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
				if ( this_button_status.event[BUTTON_BACK_KP_ID]==BUTTON_EVENT_CLICK ) // user wants to exit?
				{
					; // Do extra tasks here if needed

					xQueueReset(main_q_hdl); // Reset main q before return
					break; // Exit and return to the calling task (subfunc_handler_task)
				} // if ( m1_buttons_status[BUTTON_BACK_KP_ID]==BUTTON_EVENT_CLICK )
				else
				{
					; // Do other things for this task, if needed
				}
			} // if ( q_item.q_evt_type==Q_EVENT_KEYPAD )
			else
			{
				; // Do other things for this task
			}
		} // if (ret==pdTRUE)
	} // while (1 ) // Main loop of this task

} // void settings_system(void)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void settings_about(void)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint8_t choice;

	/* Graphic work starts here */
	u8g2_FirstPage(&m1_u8g2);
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	u8g2_DrawBox(&m1_u8g2, 0, 52, 128, 12); // Draw an inverted bar at the bottom to display options
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG); // Write text in inverted color
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	u8g2_DrawXBMP(&m1_u8g2, 1, 53, 8, 8, arrowleft_8x8); // draw arrowleft icon
	u8g2_DrawStr(&m1_u8g2, 11, 61, "Prev.");
	u8g2_DrawXBMP(&m1_u8g2, 119, 53, 8, 8, arrowright_8x8); // draw arrowright icon
	u8g2_DrawStr(&m1_u8g2, 97, 61, "Next");
	m1_u8g2_nextpage(); // Update display RAM

	choice = 0;
	settings_about_display_choice(choice);

	while (1 ) // Main loop of this task
	{
		;
		; // Do other parts of this task here
		;

		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret==pdTRUE)
		{
			if ( q_item.q_evt_type==Q_EVENT_KEYPAD )
			{
				// Notification is only sent to this task when there's any button activity,
				// so it doesn't need to wait when reading the event from the queue
				ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
				if ( this_button_status.event[BUTTON_BACK_KP_ID]==BUTTON_EVENT_CLICK ) // user wants to exit?
				{
					; // Do extra tasks here if needed
					xQueueReset(main_q_hdl); // Reset main q before return
					break; // Exit and return to the calling task (subfunc_handler_task)
				} // if ( this_button_status.event[BUTTON_BACK_KP_ID]==BUTTON_EVENT_CLICK )
				else if ( this_button_status.event[BUTTON_LEFT_KP_ID]==BUTTON_EVENT_CLICK ) // Previous?
				{
					choice--;
					if ( choice > SETTING_ABOUT_CHOICES_MAX )
						choice = SETTING_ABOUT_CHOICES_MAX;
					settings_about_display_choice(choice);
				} // else if ( this_button_status.event[BUTTON_LEFT_KP_ID]==BUTTON_EVENT_CLICK )
				else if ( this_button_status.event[BUTTON_RIGHT_KP_ID]==BUTTON_EVENT_CLICK ) // Next?
				{
					choice++;
					if ( choice > SETTING_ABOUT_CHOICES_MAX )
						choice = 0;
					settings_about_display_choice(choice);
				} // else if ( this_button_status.event[BUTTON_RIGHT_KP_ID]==BUTTON_EVENT_CLICK )
			} // if ( q_item.q_evt_type==Q_EVENT_KEYPAD )
			else
			{
				; // Do other things for this task
			}
		} // if (ret==pdTRUE)
	} // while (1 ) // Main loop of this task

} // void settings_about(void)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
static void settings_about_display_choice(uint8_t choice)
{
	uint8_t prn_name[20];

	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG); // Set background color
	u8g2_DrawBox(&m1_u8g2, 0, 0, M1_LCD_DISPLAY_WIDTH, ABOUT_BOX_Y_POS_ROW_5 + 1); // Clear old content
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT); // Set text color

	switch (choice)
	{
		case 0: // FW info
			u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B); // Set bold font
			u8g2_DrawStr(&m1_u8g2, 0, ABOUT_BOX_Y_POS_ROW_1, "FW version info:");
			u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N); // Set normal font
			sprintf(prn_name, "%d.%d.%d.%d", m1_device_stat.config.fw_version_major, m1_device_stat.config.fw_version_minor, m1_device_stat.config.fw_version_build, m1_device_stat.config.fw_version_rc);
			u8g2_DrawStr(&m1_u8g2, 0, ABOUT_BOX_Y_POS_ROW_2, prn_name);
			sprintf(prn_name, "Active bank: %d", (m1_device_stat.active_bank==BANK1_ACTIVE)?1:2);
			u8g2_DrawStr(&m1_u8g2, 0, ABOUT_BOX_Y_POS_ROW_3, prn_name);
			break;

		case 1: // Company info
			u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N); // Set small font
			u8g2_DrawStr(&m1_u8g2, 0, ABOUT_BOX_Y_POS_ROW_1, "MonstaTek Inc.");
			u8g2_DrawStr(&m1_u8g2, 0, ABOUT_BOX_Y_POS_ROW_2, "San Jose, CA, USA");
			break;

		default:
			u8g2_DrawXBMP(&m1_u8g2, 23, 1, 82, 36, m1_device_82x36);
			break;
	} // switch (choice)

	m1_u8g2_nextpage(); // Update display RAM
} // static void settings_about_display_choice(uint8_t choice)


/*============================================================================*/
/**
  * @brief  Switch firmware bank
  *         Shows current active bank and confirms before swapping
  * @note   SiN360 -- Bank switcher for multi-firmware support
  */
/*============================================================================*/
void setting_switch_bank(void)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint8_t confirmed = 0;
	char line[32];

	/* Draw initial screen */
	u8g2_FirstPage(&m1_u8g2);
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

	u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
	u8g2_DrawStr(&m1_u8g2, 4, 12, "Switch Bank");

	u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
	snprintf(line, sizeof(line), "Active bank: %d", (m1_device_stat.active_bank == BANK1_ACTIVE) ? 1 : 2);
	u8g2_DrawStr(&m1_u8g2, 4, 26, line);
	snprintf(line, sizeof(line), "Switch to bank: %d", (m1_device_stat.active_bank == BANK1_ACTIVE) ? 2 : 1);
	u8g2_DrawStr(&m1_u8g2, 4, 38, line);

	/* Bottom bar */
	u8g2_DrawBox(&m1_u8g2, 0, 52, 128, 12);
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 2, 61, "OK=Switch  Back=Cancel");
	m1_u8g2_nextpage();

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret != pdTRUE) continue;

		if (q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
			if (ret != pdTRUE) continue;

			if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				xQueueReset(main_q_hdl);
				break;
			}
			else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (!confirmed)
				{
					/* Show confirmation */
					confirmed = 1;
					u8g2_FirstPage(&m1_u8g2);
					u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
					u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
					u8g2_DrawStr(&m1_u8g2, 4, 12, "Are you sure?");
					u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
					u8g2_DrawStr(&m1_u8g2, 4, 28, "Device will reboot");
					u8g2_DrawStr(&m1_u8g2, 4, 40, "to the other bank.");
					u8g2_DrawBox(&m1_u8g2, 0, 52, 128, 12);
					u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
					u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
					u8g2_DrawStr(&m1_u8g2, 2, 61, "OK=Confirm Back=Cancel");
					m1_u8g2_nextpage();
				}
				else
				{
					/* Swap banks -- device will reboot */
					u8g2_FirstPage(&m1_u8g2);
					u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
					u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
					u8g2_DrawStr(&m1_u8g2, 4, 30, "Switching banks...");
					m1_u8g2_nextpage();
					vTaskDelay(pdMS_TO_TICKS(500));
					bl_swap_banks();
					/* bl_swap_banks triggers reboot -- execution won't reach here */
				}
			}
		}
	}
} // void setting_switch_bank(void)
