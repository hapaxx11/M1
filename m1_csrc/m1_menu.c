/* See COPYING.txt for license details. */

/*
*
*  m1_menu.c
*
*  M1 menu handler
*
* M1 Project
*
*/
/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
//#include "mui.h"
//#include "u8x8.h"
//#include "U8g2lib.h"
#include "m1_gpio.h"
#include "m1_infrared.h"
#include "m1_nfc.h"
#include "m1_rfid.h"
#include "m1_field_detect.h"
#include "m1_settings.h"
#include "m1_sub_ghz.h"
#include "m1_power_ctl.h"
#include "m1_fw_update.h"
#include "m1_esp32_fw_update.h"
#include "m1_fw_download.h"
#include "m1_storage.h"
#include "m1_wifi.h"
#include "m1_bt.h"
#include "m1_802154.h"
#include "m1_esp32_hal.h"
#include "esp_app_main.h"
#include "m1_compile_cfg.h"

#ifdef M1_APP_FILE_IMPORT_ENABLE
#include "m1_flipper_integration.h"
#endif

#ifdef M1_APP_CAN_ENABLE
#include "m1_can.h"
#endif

/*************************** D E F I N E S ************************************/

//************************** C O N S T A N T **********************************/

/************************** S T R U C T U R E S *******************************/

/*----------------------------- > Sub-GHz ------------------------------------*/

S_M1_Menu_t menu_Sub_GHz =
{
    "Sub-GHz", sub_ghz_scene_entry, NULL, NULL, 0, 0, menu_m1_icon_wave, NULL, {0}
};

/*----------------------------- > 125KHz RFID --------------------------------*/

extern void rfid_scene_entry(void);

S_M1_Menu_t menu_125KHz_RFID =
{
    "RFID", rfid_scene_entry, NULL, NULL, 0, 0, menu_m1_icon_rfid, NULL, {0}
};

/*-------------------------------- > NFC -------------------------------------*/

extern void nfc_scene_entry(void);

S_M1_Menu_t menu_NFC =
{
    "NFC", nfc_scene_entry, NULL, NULL, 0, 0, menu_m1_icon_nfc, NULL, {0}
};

/*----------------------------- > Infrared -----------------------------------*/

extern void infrared_scene_entry(void);

S_M1_Menu_t menu_Infrared =
{
    "Infrared", infrared_scene_entry, NULL, NULL, 0, 0, menu_m1_icon_infrared, NULL, {0}
};

/*------------------------------- > GPIO -------------------------------------*/

extern void gpio_scene_entry(void);

S_M1_Menu_t menu_GPIO =
{
    "GPIO", gpio_scene_entry, NULL, NULL, 0, 0, menu_m1_icon_gpio, NULL, {0}
};

/*------------------------------- > Settings ---------------------------------*/

extern void settings_scene_entry(void);

S_M1_Menu_t menu_Settings =
{
    "Settings", settings_scene_entry, NULL, NULL, 0, 0, menu_m1_icon_setting, NULL, {0}
};

/*--------------------------------- > Wifi -----------------------------------*/

extern void wifi_scene_entry(void);

S_M1_Menu_t menu_Wifi =
{
    "Wifi", wifi_scene_entry, NULL, NULL, 0, 0, menu_m1_icon_wifi, NULL, {0}
};

/*--------------------------------- > BT ------------------------------------*/

extern void bt_scene_entry(void);

S_M1_Menu_t menu_Bluetooth =
{
    "Bluetooth", bt_scene_entry, NULL, NULL, 0, 0, menu_m1_icon_bluetooth, NULL, {0}
};

/*-------------------------------- > BadUSB ----------------------------------*/
#ifdef M1_APP_BADUSB_ENABLE
#include "m1_badusb.h"

S_M1_Menu_t menu_BadUSB =
{
    "BadUSB", badusb_run, NULL, NULL, 0, 0, menu_m1_icon_badusb, NULL, NULL
};
#endif /* M1_APP_BADUSB_ENABLE */

/*-------------------------------- > Games -----------------------------------*/
#ifdef M1_APP_GAMES_ENABLE

extern void games_scene_entry(void);

S_M1_Menu_t menu_Games =
{
    "Games", games_scene_entry, NULL, NULL, 0, 0, menu_m1_icon_games, NULL, {0}
};
#endif /* M1_APP_GAMES_ENABLE */

/*-------------------------------- > Apps ------------------------------------*/
#ifdef M1_APP_APPS_ENABLE
#include "m1_app_manager.h"

S_M1_Menu_t menu_Apps =
{
    "Apps", game_apps_browser_run, NULL, NULL, 0, 0, menu_m1_icon_apps, NULL, {NULL}
};
#endif /* M1_APP_APPS_ENABLE */

/*------------------------------- > MAIN MENU --------------------------------*/

const S_M1_Menu_t menu_Main =
{
#if defined(M1_APP_BADUSB_ENABLE) && defined(M1_APP_GAMES_ENABLE) && defined(M1_APP_APPS_ENABLE)
    "Main Menu", NULL, NULL, NULL, 11, 0, NULL, NULL,
    {&menu_Sub_GHz, &menu_125KHz_RFID, &menu_NFC, &menu_Infrared, &menu_GPIO, &menu_Wifi, &menu_Bluetooth, &menu_BadUSB, &menu_Games, &menu_Apps, &menu_Settings}
#elif defined(M1_APP_BADUSB_ENABLE)
    "Main Menu", NULL, NULL, NULL, 9, 0, NULL, NULL,
    {&menu_Sub_GHz, &menu_125KHz_RFID, &menu_NFC, &menu_Infrared, &menu_GPIO, &menu_Wifi, &menu_Bluetooth, &menu_BadUSB, &menu_Settings}
#elif defined(M1_APP_GAMES_ENABLE) && defined(M1_APP_APPS_ENABLE)
    "Main Menu", NULL, NULL, NULL, 10, 0, NULL, NULL,
    {&menu_Sub_GHz, &menu_125KHz_RFID, &menu_NFC, &menu_Infrared, &menu_GPIO, &menu_Wifi, &menu_Bluetooth, &menu_Games, &menu_Apps, &menu_Settings}
#else
    "Main Menu", NULL, NULL, NULL, 8, 0, NULL, NULL,
    {&menu_Sub_GHz, &menu_125KHz_RFID, &menu_NFC, &menu_Infrared, &menu_GPIO, &menu_Wifi, &menu_Bluetooth, &menu_Settings}
#endif
};


/***************************** V A R I A B L E S ******************************/

static S_M1_Menu_Control_t		menu_ctl;
static const S_M1_Menu_t 		*pthis_submenu;
TaskHandle_t					subfunc_handler_task_hdl;
TaskHandle_t					menu_main_handler_task_hdl;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static void menu_main_init(void);
void menu_main_handler_task(void *param);
void subfunc_handler_task(void *param);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/*============================================================================*/
/*
 * This function initializes the main menu.
*/
/*============================================================================*/
static void menu_main_init(void)
{
    menu_ctl.menu_level = 0; // main menu
    menu_ctl.menu_item_active = 0; // first menu item
    menu_ctl.last_selected_items[0] = 0; // last selected item is the current active item
    menu_ctl.main_menu_ptr[0] = &menu_Main; // level 0 should be the main menu
    menu_ctl.num_menu_items = menu_ctl.main_menu_ptr[0]->num_submenu_items;

    assert(menu_ctl.num_menu_items >= 3);

	pthis_submenu = menu_ctl.main_menu_ptr[menu_ctl.menu_level];
    menu_ctl.this_func = pthis_submenu->sub_func;

    m1_gui_init();

} // static void menu_main_init(void)



/*============================================================================*/
/*
 * This function handles all tasks of the M1 main menu.
*/
/*============================================================================*/
void menu_main_handler_task(void *param)
{
	uint8_t key, sel_item, n_items;
	uint8_t menu_update_stat;
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;

	settings_load_from_sd();  /* Load southpaw and other user settings (needs stack > sys_init) */

	vTaskDelay(POWER_UP_SYS_CONFIG_WAIT_TIME); // Give some time to startup_config_handler() during power-up
	while(1)
	{
		menu_update_stat = MENU_UPDATE_NONE;
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if ( ret!=pdTRUE )
			continue;
		if ( q_item.q_evt_type!=Q_EVENT_KEYPAD )
		{
			/* Refresh home screen content (battery/clock changed) without
			 * waking the backlight or resetting the inactivity timer. */
			if ( q_item.q_evt_type==Q_EVENT_BATTERY_UPDATED &&
			     m1_device_stat.op_mode==M1_OPERATION_MODE_DISPLAY_ON )
			{
				startup_home_screen_refresh();
#ifdef M1_APP_WIFI_CONNECT_ENABLE
				/* Silently re-sync RTC via NTP once every 30 minutes while
				 * WiFi is connected.  Must run here (menu-task context) so
				 * AT commands are never interleaved with other modules. */
				wifi_ntp_background_sync();
#endif
			}
			continue;
		}
		// Notification is only sent to this task when there's any button activity,
		// so it doesn't need to wait when reading the event from the queue
		ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
		if ( ret!=pdTRUE ) // This should never happen!
			continue; // Wait for a new notification when the attempt to read the button event fails

		for (key=0; key<NUM_BUTTONS_MAX; key++)
	    {
	    	if ( this_button_status.event[key]!=BUTTON_EVENT_IDLE )
	        {
	    		switch( key )
	    		{
	    			case BUTTON_OK_KP_ID:
	    				if ( this_button_status.event[BUTTON_OK_KP_ID]==BUTTON_EVENT_CLICK )
	    				{
	    					if ( m1_device_stat.op_mode==M1_OPERATION_MODE_MENU_ON )
	    					{
	                            n_items = pthis_submenu->submenu[menu_ctl.menu_item_active]->num_submenu_items;  // get the number of submenu items of the selected item
	                            menu_ctl.last_selected_items[menu_ctl.menu_level] = menu_ctl.menu_item_active; // save the selected item before going the next menu level
	                            if ( n_items != 0 ) // This menu item has another submenu?
	                            {
	                                menu_ctl.menu_level++; // go to next menu level
	                                menu_ctl.main_menu_ptr[menu_ctl.menu_level] = pthis_submenu->submenu[menu_ctl.menu_item_active];
	                                pthis_submenu = menu_ctl.main_menu_ptr[menu_ctl.menu_level];
	                                menu_ctl.this_func = pthis_submenu->sub_func;
	                            	menu_ctl.num_menu_items = n_items; // update this field
	                                menu_ctl.menu_item_active = 0; // default for new submenu
	                                sel_item = 0;
	                                if ( menu_ctl.this_func != NULL )
	                                {
	                                    menu_ctl.this_func(); // run the function of the selected submenu item to initialize it
	                                    // This function should complete quickly after initializing the display!!!
	                                } // if ( menu_ctl.this_func != NULL )
	                                menu_update_stat = MENU_UPDATE_RESET;
	                            } // if ( n_items != 0 )

	                            else if ( pthis_submenu->submenu[menu_ctl.menu_item_active]->sub_func != NULL ) // This menu item has no submenu. Does it have a function to run?
	                            {
	                                m1_device_stat.op_mode = M1_OPERATION_MODE_SUB_FUNC_RUNNING;
	                                m1_device_stat.sub_func = pthis_submenu->submenu[menu_ctl.menu_item_active]->sub_func; // let schedule to run the function of the selected submenu item
	                                //this_button_status.event[key].event = BUTTON_EVENT_IDLE; // clear before return
	                                // Notify the sub-function handler
	                                xTaskNotify(subfunc_handler_task_hdl, 0, eNoAction);
	                                // Wait for the sub-function to complete and notify this task from subfunc_handler_task
	                                xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
	                                xQueueReset(button_events_q_hdl); // Drain stale button events from module
	                        		m1_device_stat.op_mode = M1_OPERATION_MODE_MENU_ON;
	                                // Return from sub-function. Let update GUI.
	                                sel_item = menu_ctl.menu_item_active;
	                                menu_update_stat = MENU_UPDATE_REFRESH; // The sub-function may have changed the GUI. It needs update.
	                            } // else if ( menu_ctl.this_function != NULL )

	                            else // This case should never happen. It doesn't exist!
	                            {
	                            	assert(("num_menu_items=0, this_func=NULL", FALSE));
	                            }
	                            key = NUM_BUTTONS_MAX; // Exit condition to stop checking other buttons!
	    					} // if ( m1_device_stat.op_mode==M1_OPERATION_MODE_MENU_ON )
	    					else if ( m1_device_stat.op_mode==M1_OPERATION_MODE_DISPLAY_ON )
	    					{
	    						menu_main_init();
	    						sel_item = 0;
	    						menu_update_stat = MENU_UPDATE_INIT;
	    						m1_device_stat.op_mode = M1_OPERATION_MODE_MENU_ON; // update new state
	    					} // else if ( m1_device_stat.op_mode==M1_OPERATION_MODE_DISPLAY_ON )
#ifdef BUTTON_REPEATED_PRESS_ENABLE
	    					else if ( m1_device_stat.op_mode==M1_OPERATION_MODE_POWER_UP )
	    					{
	    						m1_device_stat.op_mode = M1_OPERATION_MODE_DISPLAY_ON; // update new state
	    						m1_gui_welcome_scr();
	    						; // Change settings/config to exit out of shutdown/sleep state
	    					} // if ( m1_device_stat.op_mode==M1_OPERATION_MODE_POWER_UP )
#endif // #ifdef BUTTON_REPEATED_PRESS_ENABLE
	    				} // if ( this_button_status.event[BUTTON_OK_KP_ID]==BUTTON_EVENT_CLICK )
#ifndef BUTTON_REPEATED_PRESS_ENABLE
	    				else if ( this_button_status.event[BUTTON_OK_KP_ID]==BUTTON_EVENT_LCLICK )
	    				{
	    					if ( m1_device_stat.op_mode==M1_OPERATION_MODE_POWER_UP )
	    					{
	    						m1_device_stat.op_mode = M1_OPERATION_MODE_DISPLAY_ON; // update new state
	    						m1_gui_welcome_scr();
	    						; // Change settings/config to exit out of shutdown/sleep state
	    					} // if ( m1_device_stat.op_mode==M1_OPERATION_MODE_POWER_UP )
	    					else if ( m1_device_stat.op_mode==M1_OPERATION_MODE_MENU_ON )
	    					{
	    						//m1_device_stat.op_mode = OPERATION_MODE_SHUTDOWN; // force to sleep mode immediately
	    						//System_Shutdown();
	    					} // else if ( m1_device_stat.op_mode==M1_OPERATION_MODE_MENU_ON )
	    				} // if ( this_button_status.event[BUTTON_OK_KP_ID]==BUTTON_EVENT_LCLICK )
#endif // #ifndef BUTTON_REPEATED_PRESS_ENABLE
	                    break;

	    			case BUTTON_UP_KP_ID:
						if ( m1_device_stat.op_mode==M1_OPERATION_MODE_MENU_ON )
						{
							if ( this_button_status.event[BUTTON_DOWN_KP_ID]==BUTTON_EVENT_CLICK ) // UP and DOWN pressed at the same time?
								break; // Do nothing
							sel_item = menu_ctl.menu_item_active; // take the current active menu item
							if ( sel_item==0 ) // first menu item?
							{
								sel_item = menu_ctl.num_menu_items - 1; // move to last menu item
								//menu_ctl.total_menu_items = menu_ctl.main_menu_ptr[menu_ctl.menu_level]->submenu_items;
							}
							else // not the first item
							{
								sel_item--;
							}
							menu_ctl.menu_item_active = sel_item; // update the active index
							menu_update_stat = MENU_UPDATE_MOVE_UP;
						} // if ( m1_device_stat.op_mode==M1_OPERATION_MODE_MENU_ON )
						else
						{
							; // Do something here if necessary. This case may never happen!
						}
	    				break;

	    			case BUTTON_DOWN_KP_ID:
						if ( m1_device_stat.op_mode==M1_OPERATION_MODE_MENU_ON )
						{
							if ( this_button_status.event[BUTTON_UP_KP_ID]==BUTTON_EVENT_CLICK ) // UP and DOWN pressed at the same time?
								break; // Do nothing
	                        sel_item = menu_ctl.menu_item_active; // take the current active menu item
	                        if ( sel_item==(menu_ctl.num_menu_items - 1) ) // last menu item?
	                        {
	                        	sel_item = 0; // move to first menu item
	                        }
	                        else // not the last item
	                        {
	                        	sel_item++;
	                        }
	                        menu_ctl.menu_item_active = sel_item; // update the active index
	                        menu_update_stat = MENU_UPDATE_MOVE_DOWN;
						}
						else
						{
							; // Do something here if necessary. This case may never happen!
							if ( this_button_status.event[BUTTON_DOWN_KP_ID]==BUTTON_EVENT_LCLICK )
							{
								m1_buzzer_notification();
							}
						}
	    				break;

	    			case BUTTON_LEFT_KP_ID:
						if ( m1_device_stat.op_mode==M1_OPERATION_MODE_MENU_ON )
						{
							if ( pthis_submenu->xkey_handler )
								pthis_submenu->xkey_handler(this_button_status.event[BUTTON_LEFT_KP_ID], BUTTON_LEFT_KP_ID, sel_item);
						}
						else if ( m1_device_stat.op_mode==M1_OPERATION_MODE_DISPLAY_ON )
						{
							storage_explore();
							m1_gui_welcome_scr();
						}
	    				break;

	    			case BUTTON_RIGHT_KP_ID:
						if ( m1_device_stat.op_mode==M1_OPERATION_MODE_MENU_ON )
						{
							if ( pthis_submenu->xkey_handler )
								pthis_submenu->xkey_handler(this_button_status.event[BUTTON_RIGHT_KP_ID], BUTTON_RIGHT_KP_ID, sel_item);
						}
						else
						{
							; // Do something here if necessary. This case may never happen!
						}
	    				break;

	    			case BUTTON_BACK_KP_ID:
						if ( m1_device_stat.op_mode==M1_OPERATION_MODE_MENU_ON )
						{
							if ( menu_update_stat!=MENU_UPDATE_NONE ) // Other buttons pressed?
								break; // Do nothing, let other buttons take their higher priority!
							if ( menu_ctl.menu_level==0 ) // already at main menu screen?
							{
	    						m1_device_stat.op_mode = M1_OPERATION_MODE_DISPLAY_ON; // update new state
	    						m1_gui_welcome_scr();
								; // Do something before going to default home screen
								//
							} // if ( menu_ctl.menu_level==0 )
							else
							{
								if ( menu_ctl.num_menu_items ) // Submenu with active items?
								{
									if ( pthis_submenu->deinit_func )
										pthis_submenu->deinit_func(); // Run deinit function of this submenu before leaving
								} // if ( menu_ctl.num_menu_items )
								menu_ctl.menu_level--; // go back one level
								menu_ctl.menu_item_active = menu_ctl.last_selected_items[menu_ctl.menu_level]; // restore  previous selected item of the upper menu level
								pthis_submenu = menu_ctl.main_menu_ptr[menu_ctl.menu_level]; // save the current menu level index
								menu_ctl.this_func = pthis_submenu->sub_func;
								menu_ctl.num_menu_items = pthis_submenu->num_submenu_items;
								n_items = menu_ctl.num_menu_items;
								sel_item = menu_ctl.menu_item_active;
								if ( menu_ctl.this_func != NULL )
								{
									menu_ctl.this_func(); // run the function of the selected submenu item to initialize it
									// It's not necessary to set the flag sub_func_is_running here.
									// This function should complete quickly after initializing the display!!!
								} // if ( menu_ctl.this_func != NULL )
								menu_update_stat = MENU_UPDATE_RESTORE;
							} // else
						} // if ( m1_device_stat.op_mode==M1_OPERATION_MODE_MENU_ON )
						else
						{
							; // Do something here if necessary. This case may never happen!
						}
	    				break;

	    			default: // undefined buttons, or buttons do not exist.
	    				break;
	    		} // switch( key )

	        } // if ( this_button_status.event[key]!=BUTTON_EVENT_IDLE )
	    } // for (key=0; key<NUM_BUTTONS_MAX; key++)

	    if ( menu_update_stat!=MENU_UPDATE_NONE )
	    	m1_gui_menu_update(pthis_submenu, sel_item, menu_update_stat);
	} // while(1)

} // void menu_main_handler_task(void *param)



/*============================================================================*/
/*
 * This task handles the execution of any sub-function
*/
/*============================================================================*/
void subfunc_handler_task(void *param)
{
	while(1)
	{
		// Waiting for notification from menu_main_handler_task,
		// or from button_event_handler_task
		xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
		assert(m1_device_stat.sub_func!=NULL);
		// Run the sub-function
		m1_device_stat.sub_func();
		// Sub-function completes, let notify menu_main_handler_task
		xTaskNotify(menu_main_handler_task_hdl, 0, eNoAction);
	} // while(1)
} // void subfunc_handler_task(void *param)
