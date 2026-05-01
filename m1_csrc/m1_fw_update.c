/* See COPYING.txt for license details. */

/*
*
* m1_fw_update.c
*
* Firmware update functionality
*
* M1 Project
*
*/

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_display.h"
#include "m1_fw_update.h"
#include "m1_fw_update_bl.h"
#include "m1_storage.h"
#include "m1_power_ctl.h"
#include "m1_wdt_hw.h"

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG		"FW-UPDATE"

#define THIS_LCD_MENU_TEXT_FIRST_ROW_Y			11
#define THIS_LCD_MENU_TEXT_ROW_SPACE			10

//************************** S T R U C T U R E S *******************************


/***************************** V A R I A B L E S ******************************/

static char *pfullpath = NULL;
static uint8_t fw_update_status = M1_FW_UPDATE_NOT_READY;
static uint32_t fw_version_new;
static FIL hfile_fw;
static S_M1_file_info *f_info = NULL;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

void firmware_update_init(void);
void firmware_update_exit(void);
void firmware_update_get_image_file(void);
void firmware_update_start(void);
void firmware_swap_banks(void);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/*============================================================================*/
/*
 * This function initializes display for this sub-menu item.
 */
/*============================================================================*/
void firmware_update_init(void)
{
	if ( !pfullpath )
		pfullpath = malloc(FW_FILE_PATH_LEN_MAX + FW_FILE_NAME_LEN_MAX);
	fw_update_status = M1_FW_UPDATE_NOT_READY;
} // void firmware_update_init(void)



/*============================================================================*/
/*
 * This function will exit this sub-menu and return to the upper level menu.
 */
/*============================================================================*/
void firmware_update_exit(void)
{
	if ( pfullpath )
	{
		free(pfullpath);
		pfullpath = NULL;
	}
} // void firmware_update_exit(void)




/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void firmware_update_start(void)
{
	uint8_t uret, old_op_mode;

	uret = M1_FW_UPDATE_NOT_READY;
	if ( !m1_check_battery_level(FW_UPDATE_MIN_BATTERY_PCT) ) // Battery too low and not charging?
    {
		fw_update_status = M1_FW_UPDATE_NOT_READY; // Force quit
    	uret = M1_FW_UPDATE_LOW_BATTERY;
    } // if ( !m1_check_battery_level(FW_UPDATE_MIN_BATTERY_PCT) )

	old_op_mode = m1_device_stat.op_mode;

	do
    {
        if ( fw_update_status != M1_FW_UPDATE_READY )
        {
        	break;
        }

        //
		// - Check for battery status before proceeding
		//

        m1_device_stat.op_mode = M1_OPERATION_MODE_FIRMWARE_UPDATE;

        m1_led_fw_update_on(NULL); // Turn on
    	startup_config_write(BK_REGS_SELECT_DEV_OP_STAT, DEV_OP_STATUS_FW_UPDATE_ACTIVE);

    	uret = bl_flash_app(&hfile_fw);

    	m1_led_fw_update_off(); // Turn off

    	if ( uret != BL_CODE_OK )
		{
			uret = M1_FW_UPDATE_FAILED;
			startup_config_write(BK_REGS_SELECT_DEV_OP_STAT, DEV_OP_STATUS_NO_OP);
		}
		else
		{
			uret = M1_FW_UPDATE_SUCCESS;
	    	startup_config_write(BK_REGS_SELECT_DEV_OP_STAT, DEV_OP_STATUS_FW_UPDATE_COMPLETE);
		}
    } while (0);

    if ( fw_update_status==M1_FW_UPDATE_READY )
    	m1_fb_close_file(&hfile_fw);

    fw_update_status = uret; // Update new status
	if ( fw_update_status==M1_FW_UPDATE_SUCCESS )
	{
		// Display warning message: device will reboot in n seconds...
		bl_swap_banks();
	} // if ( fw_update_status==M1_FW_UPDATE_SUCCESS )

	m1_device_stat.op_mode = old_op_mode;

    xQueueReset(main_q_hdl); // Reset main q before return
} // void firmware_update_start(void)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void firmware_update_get_image_file(void)
{
	uint8_t uret, ext, fret;
    uint8_t fw_payload[FW_IMAGE_CHUNK_SIZE];
    size_t count, sum;
    uint32_t crc32ret, image_size, fwver_old;
    S_M1_FW_CONFIG_t fwconfig;

	f_info = storage_browse(NULL);

	fw_update_status = M1_FW_IMAGE_FILE_TYPE_ERROR; // reset
	if ( f_info->file_is_selected )
	{
		do
		{
			uret = strlen(f_info->file_name);
			if ( !uret )
				break;
			ext = 0;
			while ( uret )
			{
				ext++;
				if ( f_info->file_name[--uret]=='.' ) // Find the dot
					break;
			} // while ( uret )
			if ( !uret || (ext!=4) ) // ext==length of '.bin'
				break;
			if ( strcmp(&f_info->file_name[uret], ".bin" ))
				break;
			fw_update_status = M1_FW_UPDATE_READY;
		} while (0);
	} // if ( f_info->file_is_selected )

	uret = M1_FW_UPDATE_NOT_READY;
	do
    {
        if ( fw_update_status != M1_FW_UPDATE_READY )
        {
        	break;
        }

		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG); // set to background color
		// Draw box with background color to clear the area for the hourglass icon
		u8g2_DrawBox(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - 28, THIS_LCD_MENU_TEXT_FIRST_ROW_Y + THIS_LCD_MENU_TEXT_ROW_SPACE + 2, 24, THIS_LCD_MENU_TEXT_ROW_SPACE);
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT); // return to text color
    	u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - 24, 16, 18, 32, hourglass_18x32); // Draw icon
    	m1_u8g2_nextpage(); // Update display RAM

        uret = m1_fb_dyn_strcat(pfullpath, 2, "",  f_info->dir_name, f_info->file_name);
        uret = m1_fb_open_file(&hfile_fw, pfullpath);
		if ( !uret )
		{
			image_size = f_size(&hfile_fw);
		    // Both the address and image size must be aligned to 4 bytes
		    if ( (!image_size) || (image_size % 4 != 0) || (image_size > (FW_IMAGE_SIZE_MAX + FW_IMAGE_CRC_SIZE)) )
		    {
		    	uret = M1_FW_IMAGE_SIZE_INVALID;
		    	break;
		    } // if ( (!image_size) || (image_size % 4 != 0) || (image_size > (FW_IMAGE_SIZE_MAX + FW_IMAGE_CRC_SIZE) )

		    // Call the function to initialize this transaction.
		    // Do not take the return value in this case. Input data can be anything.
		    bl_get_crc_chunk(&crc32ret, 0, true, false); // CRC init
			sum = image_size;
			while ( sum )
			{
				m1_wdt_reset(); // firmware_update_get_image_file() blocks the main task; kick IWDG each chunk
				count = m1_fb_read_from_file(&hfile_fw, fw_payload, FW_IMAGE_CHUNK_SIZE);
				if ( !count || (count % 4 != 0) ) // Read failed?
				{
					uret = M1_FW_IMAGE_FILE_ACCESS_ERROR;
					break;
				} // if ( !count || (count % 4 != 0) )
				sum -= count;
				if ( count < FW_IMAGE_CHUNK_SIZE ) // possible last chunk?
				{
					count -= FW_IMAGE_CRC_SIZE; // exclude CRC data at the end
					if ( count )
						crc32ret = bl_get_crc_chunk((uint32_t *)fw_payload, count/FW_IMAGE_CRC_SIZE, false, true); // complete
					else // last chunk is the appended CRC data
					{
						// Call the function to complete the transaction.
						// Do not take the return value in this case. Input data can be anything.
						bl_get_crc_chunk(&crc32ret, 1, false, true); // complete
					}
					break; // exit
				} // if ( count < FW_IMAGE_CHUNK_SIZE )
				else
				{
					if ( !sum ) // end of file?
					{
						count -= FW_IMAGE_CRC_SIZE; // exclude CRC data at the end
						crc32ret = bl_get_crc_chunk((uint32_t *)fw_payload, count/FW_IMAGE_CRC_SIZE, false, true); // complete
						break; // exit
					} // if ( !sum )
					else
						crc32ret = bl_get_crc_chunk((uint32_t *)fw_payload, count/FW_IMAGE_CRC_SIZE, false, false);
				} // else
			} // while ( sum )

			if ( !sum )
			{
			    // Compare CRC here
			    uret = memcmp(&fw_payload[count], &crc32ret, FW_IMAGE_CRC_SIZE);
			    if ( uret )
			    {
			    	uret = M1_FW_CRC_CHECKSUM_UNMATCHED;
			    	break;
			    }
			} // if ( !sum )
			else
			{
				uret = M1_FW_IMAGE_FILE_ACCESS_ERROR;
				break;
			}

			// Check for fw version here
			//M1_FW_VERSION_ERROR,
			fret = f_lseek(&hfile_fw, FW_START_ADDRESS ^ FW_CONFiG_ADDRESS); // Move file pointer to the config data
			if ( fret != FR_OK )
			{
				uret = M1_FW_IMAGE_FILE_ACCESS_ERROR;
				break;
			} // if ( fret != FR_OK )
			count = m1_fb_read_from_file(&hfile_fw, (char *)&fwconfig, sizeof(S_M1_FW_CONFIG_t));
			if ( count != sizeof(S_M1_FW_CONFIG_t) )
			{
				uret = M1_FW_IMAGE_FILE_ACCESS_ERROR;
				break;
			}
			fwver_old = *(uint32_t *)&m1_device_stat.config.fw_version_rc;
			fw_version_new = *(uint32_t *)&fwconfig.fw_version_rc;
		} // if ( !uret )
		else
		{
			uret = M1_FW_IMAGE_FILE_ACCESS_ERROR;
			break;
		}
    } while (0);

    if ( fw_update_status==M1_FW_UPDATE_READY )
    {
    	if ( uret ) // error?
    	{
    		m1_fb_close_file(&hfile_fw);
    		fw_update_status = uret;
    	} // if ( uret )
    } // if ( fw_update_status==M1_FW_UPDATE_READY )

	xQueueReset(main_q_hdl); // Reset main q before return

} // void firmware_update_get_image_file(void)



/*============================================================================*/
/**
  * @brief  Swap boot banks from the device menu.
  *         Shows bank status, confirms with user, then swaps.
  * @param  None
  * @retval None
  */
/*============================================================================*/
void firmware_swap_banks(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    uint16_t active_bank;
    uint8_t target_bank;
    char line1[30], line2[30];
    bool bank_valid;

    active_bank = bl_get_active_bank();
    target_bank = (active_bank == BANK1_ACTIVE) ? 2 : 1;
    bank_valid = bl_is_inactive_bank_valid();

    /* Draw status screen */
    m1_u8g2_firstpage();
    do {
        /* Standard scene title with separator */
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        m1_draw_text(&m1_u8g2, 2, 9, 120, "Swap Banks", TEXT_ALIGN_CENTER);
        u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

        sprintf(line1, "Active: Bank %d", (active_bank == BANK1_ACTIVE) ? 1 : 2);
        u8g2_DrawStr(&m1_u8g2, 4, 24, line1);

        if (bank_valid) {
            sprintf(line2, "Swap to Bank %d?", target_bank);
            u8g2_DrawStr(&m1_u8g2, 4, 36, line2);

            m1_info_box_display_init(false);
            m1_info_box_display_draw(INFO_BOX_ROW_1, (const uint8_t *)"OK=Swap  BACK=Cancel");
        } else {
            u8g2_DrawStr(&m1_u8g2, 4, 36, "Bank empty!");

            m1_info_box_display_init(false);
            m1_info_box_display_draw(INFO_BOX_ROW_1, (const uint8_t *)"No FW. BACK=Cancel");
        }
    } while (m1_u8g2_nextpage());

    /* Wait for button input */
    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE) continue;
        if (q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

        ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
        if (ret != pdTRUE) continue;

        if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            xQueueReset(main_q_hdl);
            break;  /* Cancel — return to menu */
        }
        else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK && bank_valid)
        {
            /* Show swapping message */
            m1_u8g2_firstpage();
            do {
                u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
                u8g2_DrawStr(&m1_u8g2, 4, 32, "Swapping...");
            } while (m1_u8g2_nextpage());

            bl_swap_banks();
            /* Does not return — system resets */
        }
    }
} // void firmware_swap_banks(void)
