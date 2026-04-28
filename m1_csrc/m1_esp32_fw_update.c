/* See COPYING.txt for license details. */

/*
*
* m1_esp32_fw_update.c
*
* M1 source for ESP32 firmware update
*
* M1 Project
*
*/

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_esp32_hal.h"
#include "stm32_port.h"
#include "esp_loader.h"
#include "app_common.h"
#include "m1_storage.h"
#include "m1_md5_hash.h"
#include "m1_fw_update_bl.h"
#include "m1_power_ctl.h"

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG				"ESP32-FW"

#define ESP32_IO9_GPIO_Port						BUTTON_RIGHT_GPIO_Port
#define ESP32_IO9_Pin							BUTTON_RIGHT_Pin
#define ESP32_RESET_GPIO_Port					ESP32_EN_GPIO_Port
#define ESP32_RESET_Pin							ESP32_EN_Pin

#define THIS_LCD_MENU_TEXT_FIRST_ROW_Y			11
#define THIS_LCD_MENU_TEXT_FRAME_FIRST_ROW_Y	1
#define THIS_LCD_MENU_TEXT_ROW_SPACE			10

#define	ESP32_BOOT_MESSAGE_LF					0x0A
#define	ESP32_BOOT_MESSAGE_CR					0x0D
#define	ESP32_BOOT_MESSAGE_SEPARATOR			':'

#define ESP32_IMAGE_SIZE_MAX					(uint32_t)0x400000 // 4Mbytes
#define ESP32_IMAGE_CHUNK_SIZE					1024 // bytes

#define ESP32_START_ADDRESS_MIN					0x0000 // default
#define ESP32_START_ADDRESS_DEF					0x10000 // default app address, 64K
#define ESP32_START_ADDRESS_MAX					0x100000 // 1024K
#define ESP32_START_ADDRESS_LO_INC				0x1000 // normal address increment, 4K each step
#define ESP32_START_ADDRESS_HI_INC				0x10000 // fast address increment, 64K each step

//************************** C O N S T A N T **********************************/

//************************** S T R U C T U R E S *******************************


/***************************** V A R I A B L E S ******************************/

static char *pfullpath = NULL;
static char *pfilename_md5 = NULL;
static uint8_t esp32_update_status = M1_FW_UPDATE_NOT_READY;
static uint32_t start_address = ESP32_START_ADDRESS_MIN;
static size_t image_size = 0;
static uint8_t progress_percent_count = 0;
static 	S_M1_file_info *f_info = NULL;
static FIL hfile_fw;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

void setting_esp32_init(void);
void setting_esp32_exit(void);
void setting_esp32_gui_update(const S_M1_Menu_t *phmenu, uint8_t sel_item);
void setting_esp32_xkey_handler(S_M1_Key_Event event, uint8_t button_id, uint8_t sel_item);
void setting_esp32_image_file(void);
void setting_esp32_start_address(void);
void setting_esp32_firmware_update(void);

static esp_loader_error_t m1_fw_app(FIL *hfile);
static esp_loader_error_t m1_fw_flash_binary(uint8_t *payload, size_t size);
static uint16_t esp32_get_boot_info(uint8_t *boot_msg, uint16_t boot_msg_len, uint16_t *len);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/


/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void setting_esp32_init(void)
{
	if ( !pfullpath )
		pfullpath = malloc(ESP_FILE_PATH_LEN_MAX + ESP_FILE_NAME_LEN_MAX);
	assert(pfullpath!=NULL);
	if ( !pfilename_md5 )
		pfilename_md5 = malloc(ESP_FILE_NAME_LEN_MAX);
	assert(pfilename_md5!=NULL);

	start_address = ESP32_START_ADDRESS_MIN;
	esp32_update_status = M1_FW_UPDATE_NOT_READY; // Reset
} // void setting_esp32_init(void)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void setting_esp32_exit(void)
{
	if ( pfullpath )
	{
		free(pfullpath);
		pfullpath = NULL;
	}
	if ( pfilename_md5 )
	{
		free(pfilename_md5);
		pfilename_md5 = NULL;
	}
	esp32_UART_deinit();
} // void setting_esp32_exit(void)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void setting_esp32_xkey_handler(S_M1_Key_Event event, uint8_t button_id, uint8_t sel_item)
{
	uint8_t prn_name[GUI_DISP_LINE_LEN_MAX + 1] = {0};

	if ( sel_item != 1) // Not the Start Address?
		return;

	if ( event==BUTTON_EVENT_CLICK )
	{
		if ( button_id==BUTTON_LEFT_KP_ID ) // Left arrow key to decrease the start address
		{
			if ( start_address )
			{
				if ( start_address > ESP32_START_ADDRESS_DEF ) // High address range?
					start_address -= ESP32_START_ADDRESS_HI_INC; // High decrement
				else
					start_address -= ESP32_START_ADDRESS_LO_INC; // Low decrement
			} // if ( start_address )
		} // if ( button_id==BUTTON_LEFT_KP_ID )
		else if ( button_id==BUTTON_RIGHT_KP_ID ) // Right arrow key to increase the start address
		{
			if ( start_address < ESP32_START_ADDRESS_MAX )
			{
				if ( start_address >= ESP32_START_ADDRESS_DEF ) // High address range?
					start_address += ESP32_START_ADDRESS_HI_INC; // High increment
				else
					start_address += ESP32_START_ADDRESS_LO_INC; // Low increment
			} // if ( start_address < ESP32_START_ADDRESS_MAX )
		}
    	// Display start address
    	sprintf(prn_name, "0x%06lX:", start_address);
    	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG); // set to background color
    	u8g2_DrawBox(&m1_u8g2, 4, INFO_BOX_Y_POS_ROW_1 - M1_SUB_MENU_FONT_HEIGHT, 120, M1_SUB_MENU_FONT_HEIGHT + 1);
    	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT); // set to text color
    	m1_info_box_display_draw(INFO_BOX_ROW_1, prn_name);
    	m1_u8g2_nextpage();
	} // if ( event==BUTTON_EVENT_CLICK )
} // void setting_esp32_xkey_handler(S_M1_Key_Event event, uint8_t button_id, uint8_t)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void setting_esp32_image_file(void)
{
	uint8_t uret, ext;
    uint8_t payload[ESP32_IMAGE_CHUNK_SIZE];
    uint8_t raw_md5[16] = {0};
    /* Zero termination require 1 byte */
    uint8_t hex_md5[MAX(MD5_SIZE_ROM, MD5_SIZE_STUB) + 1] = {0};
    uint8_t hex_md5_infile[MAX(MD5_SIZE_ROM, MD5_SIZE_STUB) + 1] = {0};
    size_t count, sum;

	f_info = storage_browse();

	esp32_update_status = M1_FW_IMAGE_FILE_TYPE_ERROR; // reset
	if ( f_info->file_is_selected )
	{
		do
		{
			uret = strlen(f_info->file_name);
			if ( !uret )
				break;
			strcpy(pfilename_md5, f_info->file_name);
			ext = 0;
			while ( uret )
			{
				ext++;
				if ( pfilename_md5[--uret]=='.' ) // Find the dot
					break;
			} // while ( uret )
			if ( !uret || (ext!=4) ) // ext==length of '.bin'
				break;
			if ( strcmp(&pfilename_md5[uret], ".bin" ))
				break;
			// Get the filename of the md5
			strcpy(&pfilename_md5[uret + 1], "md5");
			// Open MD5 file to check its existing
			m1_fb_dyn_strcat(pfullpath, 2, "",  f_info->dir_name, pfilename_md5);
			uret = m1_fb_open_file(&hfile_fw, pfullpath);
			if ( !uret )
			{
				esp32_update_status = M1_FW_UPDATE_READY;
				m1_fb_close_file(&hfile_fw);
			}
			else
			{
				esp32_update_status = M1_FW_CRC_FILE_ACCESS_ERROR;
				break;
			}
		} while (0);
	} // if ( f_info->file_is_selected )

	uret = M1_FW_UPDATE_NOT_READY;
    do
    {
        if ( esp32_update_status != M1_FW_UPDATE_READY )
        {
        	break;
        }

		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG); // set to background color
		// Draw box with background color to clear the area for the hourglass icon
		u8g2_DrawBox(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - 28, THIS_LCD_MENU_TEXT_FIRST_ROW_Y + THIS_LCD_MENU_TEXT_ROW_SPACE + 2, 24, THIS_LCD_MENU_TEXT_ROW_SPACE);
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT); // return to text color
    	u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - 24, 16, 18, 32, hourglass_18x32); // Draw icon
    	m1_u8g2_nextpage(); // Update display RAM

        m1_fb_dyn_strcat(pfullpath, 2, "",  f_info->dir_name, pfilename_md5);
        uret = m1_fb_open_file(&hfile_fw, pfullpath);
        if ( !uret )
        {
        	image_size = f_size(&hfile_fw);
		    // MD5 hash is a 32-character string (16 hex values)
		    if ( image_size != MD5_SIZE_ROM )
		    {
		    	m1_fb_close_file(&hfile_fw);
		    	uret = M1_FW_CRC_FILE_INVALID;
		    	break;
		    } // if ( image_size != MD5_SIZE_ROM )
		    count = m1_fb_read_from_file(&hfile_fw, hex_md5_infile, MD5_SIZE_ROM);
		    m1_fb_close_file(&hfile_fw);
		    if ( count != MD5_SIZE_ROM )
		    {
		    	uret = M1_FW_CRC_FILE_ACCESS_ERROR;
		    	break;
		    }
        } // if ( !uret )
        else
        {
        	uret = M1_FW_CRC_FILE_ACCESS_ERROR;
        	break;
        }

        uret = m1_fb_dyn_strcat(pfullpath, 2, "",  f_info->dir_name, f_info->file_name);
        uret = m1_fb_open_file(&hfile_fw, pfullpath);
		if ( !uret )
		{
			image_size = f_size(&hfile_fw);
		    // Both the address and image size must be aligned to 4 bytes
		    if ( (!image_size) || (image_size % 4 != 0) )
		    {
		    	m1_fb_close_file(&hfile_fw);
		    	uret = M1_FW_IMAGE_SIZE_INVALID;
		    	break;
		    } // if ( (!image_size) || (image_size % 4 != 0) )
		    // Check for image_size
		    // It should be smaller than 4Mb
#ifdef MD5_ENABLED
			mh_md5_init(start_address, image_size);
#endif
			sum = image_size;
			while ( sum )
			{
				count = m1_fb_read_from_file(&hfile_fw, payload, ESP32_IMAGE_CHUNK_SIZE);
				if ( !count ) // Read failed?
				{
					uret = M1_FW_IMAGE_FILE_ACCESS_ERROR;
					break;
				}
				sum -= count;
#ifdef MD5_ENABLED
				mh_md5_update(payload, (count + 3) & ~3);
#endif
			} // while ( sum )

			if ( !sum )
			{
			    mh_md5_final(raw_md5);
			    mh_hexify(raw_md5, hex_md5);
			    // Compare md5 here
			    uret = memcmp(hex_md5_infile, hex_md5, MD5_SIZE_ROM);
			    if ( uret )
			    {
			    	uret = M1_FW_CRC_CHECKSUM_UNMATCHED;
			    	m1_fb_close_file(&hfile_fw);
			    	break;
			    }
			} // if ( !sum )
			else
			{
				uret = M1_FW_IMAGE_FILE_ACCESS_ERROR;
				break;
			}
		} // if ( !uret )
		else
		{
			uret = M1_FW_IMAGE_FILE_ACCESS_ERROR;
			break;
		}
    } while (0);

    if ( esp32_update_status==M1_FW_UPDATE_READY )
    {
    	if ( uret ) // error?
    	{
    		//m1_fb_close_file(&hfile_fw);
    		esp32_update_status = uret;
    	} // if ( uret )
    } // if ( fw_update_status==M1_FW_UPDATE_READY )

	xQueueReset(main_q_hdl); // Reset main q before return

} // void setting_esp32_image_file(void)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void setting_esp32_start_address(void)
{

} // void setting_esp32_start_address(void)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void setting_esp32_firmware_update(void)
{
	uint8_t uret, old_op_mode;

	uret = M1_FW_UPDATE_NOT_READY;
	if ( !m1_check_battery_level(50) ) // Is battery level less than 50%?
    {
    	esp32_update_status = M1_FW_UPDATE_NOT_READY; // Force quit
    	uret = M1_FW_UPDATE_LOW_BATTERY;
    } // if ( !m1_check_battery_level(50) )

	old_op_mode = m1_device_stat.op_mode;

    do
    {
        if ( esp32_update_status!=M1_FW_UPDATE_READY )
        {
        	break;
        }
		//
		// - Check for battery status before proceeding
		// - Write status to RTC backup registers. If something goes wrong and causes reboot,
		// the device will read the status from those registers after reset.
		//

        m1_device_stat.op_mode = M1_OPERATION_MODE_FIRMWARE_UPDATE;

        m1_led_fw_update_on(NULL); // Turn on
		esp32_UART_deinit(); // Disable the ESP32 module first
    	uret = m1_fw_app(&hfile_fw);
		if ( uret != ESP_LOADER_SUCCESS )
		{
			uret = M1_FW_UPDATE_FAILED;
		}
		else
		{
			uret = M1_FW_UPDATE_SUCCESS;
		}
		m1_fb_close_file(&hfile_fw);
		m1_led_fw_update_off(); // Turn off
    } while (0);

    esp32_update_status = uret;

	if ( (uret==M1_FW_UPDATE_SUCCESS) || (uret==M1_FW_UPDATE_FAILED) )
	{
		esp32_UART_change_baudrate(ESP32_UART_BAUDRATE); // Change to ESP32 default baud rate
		m1_ringbuffer_reset(&esp32_rb_hdl);
		esp_loader_reset_target(); // Reset ESP32 to get the boot message

		// Delay for skipping the boot message of the targets
		HAL_Delay(100);
		esp32_UART_deinit(); // Disable UART GPIO after update process is done
	} // if ( (uret==M1_FW_UPDATE_FAILED) || (uret==M1_FW_UPDATE_SUCCESS) )

	m1_device_stat.op_mode = old_op_mode;

    xQueueReset(main_q_hdl); // Reset main q before return
} // void setting_esp32_firmware_update(void)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
static esp_loader_error_t m1_fw_app(FIL *hfile)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	esp_loader_error_t flash_err;
	size_t write_size, count;
    uint8_t buffer[ESP32_IMAGE_CHUNK_SIZE];

	loader_stm32_config_t config = {
		.huart = &huart_esp,
	    .port_io0 = ESP32_IO9_GPIO_Port,
	    .pin_num_io0 = ESP32_IO9_Pin,
	    .port_rst = ESP32_RESET_GPIO_Port,
	    .pin_num_rst = ESP32_RESET_Pin,
	};

	write_size = image_size;
	fw_gui_progress_update(write_size);

	loader_port_stm32_init(&config);
	esp32_UART_init();

	// Configure the BUTTON_RIGHT to become an output pin
	// This GPIO is shared with the ESP32 boot-mode pin
	GPIO_InitStruct.Pin = ESP32_IO9_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(ESP32_IO9_GPIO_Port, &GPIO_InitStruct);

	HAL_GPIO_WritePin(ESP32_IO9_GPIO_Port, ESP32_IO9_Pin, GPIO_PIN_SET);

	flash_err = ESP_LOADER_ERROR_FAIL;
	while (connect_to_target(ESP32_UART_HIGH_BAUDRATE)==ESP_LOADER_SUCCESS)
	{
		f_lseek(hfile, 0); // Move file pointer to the beginning of the file
		//write_size = image_size;
		progress_percent_count = 0;
		while ( write_size )
		{
			flash_err = ESP_LOADER_ERROR_FAIL;
			count = m1_fb_read_from_file(hfile, buffer, ESP32_IMAGE_CHUNK_SIZE);
			if ( !count ) // Read failed?
				break;
			flash_err = m1_fw_flash_binary(buffer, count);
			if ( flash_err != ESP_LOADER_SUCCESS )
				break;
			write_size -= count;
			fw_gui_progress_update(write_size);
		} // while ( write_size )

		if ( write_size || (flash_err != ESP_LOADER_SUCCESS) )
		{
			break;
		}
		// Last step to verify MD5
		flash_err = m1_fw_flash_binary(NULL, 0);
	    break;
	} // while (connect_to_target(ESP32_UART_BAUDRATE) == ESP_LOADER_SUCCESS)

	// Configure the BUTTON_RIGHT to input again
	GPIO_InitStruct.Pin = ESP32_IO9_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(ESP32_IO9_GPIO_Port, &GPIO_InitStruct);

	return flash_err;
} // static esp_loader_error_t m1_fw_app(FIL *hfile)



/******************************************************************************/
/**
  * @brief  Backup ESP32 flash to SD card.
  *         Connects to ESP32 in download mode, reads 4MB flash, writes to
  *         /esp32_backup.bin on SD card.
  * @param  None
  * @retval None
  */
/******************************************************************************/
void setting_esp32_backup_flash(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	esp_loader_error_t err;
	static FIL hfile;
	FRESULT fres;
	uint8_t old_op_mode;
	static uint8_t buffer[ESP32_IMAGE_CHUNK_SIZE];
	const uint32_t flash_size = 0x400000; /* 4MB */
	uint32_t offset = 0;
	UINT bw;

	old_op_mode = m1_device_stat.op_mode;
	m1_device_stat.op_mode = M1_OPERATION_MODE_FIRMWARE_UPDATE;

	loader_stm32_config_t config = {
		.huart = &huart_esp,
		.port_io0 = ESP32_IO9_GPIO_Port,
		.pin_num_io0 = ESP32_IO9_Pin,
		.port_rst = ESP32_RESET_GPIO_Port,
		.pin_num_rst = ESP32_RESET_Pin,
	};

	/* Clear info box area and show debug status */
	#define BACKUP_DBG_CLEAR() do { \
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG); \
		u8g2_DrawBox(&m1_u8g2, 0, INFO_BOX_Y_POS_ROW_1 - M1_SUB_MENU_FONT_HEIGHT, 128, 30); \
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT); \
	} while(0)

	BACKUP_DBG_CLEAR();
	m1_info_box_display_draw(INFO_BOX_ROW_1, "1:UART deinit");
	m1_u8g2_nextpage();
	HAL_Delay(1000);

	esp32_UART_deinit();

	BACKUP_DBG_CLEAR();
	m1_info_box_display_draw(INFO_BOX_ROW_1, "2:loader init");
	m1_u8g2_nextpage();
	HAL_Delay(1000);

	loader_port_stm32_init(&config);
	esp32_UART_init();

	BACKUP_DBG_CLEAR();
	m1_info_box_display_draw(INFO_BOX_ROW_1, "3:IO9 setup");
	m1_u8g2_nextpage();
	HAL_Delay(1000);

	/* Configure IO9 as output (shared with BUTTON_RIGHT) */
	GPIO_InitStruct.Pin = ESP32_IO9_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(ESP32_IO9_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(ESP32_IO9_GPIO_Port, ESP32_IO9_Pin, GPIO_PIN_SET);

	BACKUP_DBG_CLEAR();
	m1_info_box_display_draw(INFO_BOX_ROW_1, "4:Connecting+Stub");
	m1_u8g2_nextpage();
	HAL_Delay(1000);

	err = connect_to_target_with_stub(ESP32_UART_BAUDRATE, ESP32_UART_BAUDRATE);
	if (err != ESP_LOADER_SUCCESS)
	{
		BACKUP_DBG_CLEAR();
		m1_info_box_display_draw(INFO_BOX_ROW_1, "5:Connect FAIL!");
		m1_u8g2_nextpage();
		HAL_Delay(5000);
		goto cleanup;
	}

	BACKUP_DBG_CLEAR();
	m1_info_box_display_draw(INFO_BOX_ROW_1, "6:Connected OK");
	m1_u8g2_nextpage();
	HAL_Delay(1000);

	/* Open output file on SD card */
	fres = f_open(&hfile, "/esp32_backup.bin", FA_CREATE_ALWAYS | FA_WRITE);
	if (fres != FR_OK)
	{
		uint8_t sdmsg[30];
		sprintf(sdmsg, "SD open err:%d", (int)fres);
		BACKUP_DBG_CLEAR();
		m1_info_box_display_draw(INFO_BOX_ROW_1, sdmsg);
		m1_u8g2_nextpage();
		HAL_Delay(5000);
		goto cleanup;
	}

	/* Clear ring buffer before flash operations */
	m1_ringbuffer_reset(&esp32_rb_hdl);

	/* Test: try init_flash_params separately to isolate failure */
	BACKUP_DBG_CLEAR();
	m1_info_box_display_draw(INFO_BOX_ROW_1, "7:Flash detect..");
	m1_u8g2_nextpage();

	{
		/* Manually call flash detect to see if it works */
		uint32_t detected_size = 0;
		err = esp_loader_flash_detect_size(&detected_size);
		uint8_t dbgmsg[30];
		if (err == ESP_LOADER_SUCCESS)
			sprintf(dbgmsg, "8:Flash=%luMB", detected_size/(1024*1024));
		else
			sprintf(dbgmsg, "8:Detect err:%d", (int)err);
		BACKUP_DBG_CLEAR();
		m1_info_box_display_draw(INFO_BOX_ROW_1, dbgmsg);
		m1_u8g2_nextpage();
		HAL_Delay(3000);

		if (err != ESP_LOADER_SUCCESS)
		{
			f_close(&hfile);
			goto cleanup;
		}
	}

	image_size = flash_size;
	progress_percent_count = 0;

	while (offset < flash_size)
	{
		uint32_t chunk = ESP32_IMAGE_CHUNK_SIZE;
		if (offset + chunk > flash_size)
			chunk = flash_size - offset;

		err = esp_loader_flash_read(buffer, offset, chunk);
		if (err != ESP_LOADER_SUCCESS)
		{
			{
				uint8_t errmsg[30];
				sprintf(errmsg, "Read err:%d @%luK", (int)err, offset/1024);
				BACKUP_DBG_CLEAR();
				m1_info_box_display_draw(INFO_BOX_ROW_1, errmsg);
				m1_u8g2_nextpage();
			}
			HAL_Delay(5000);
			f_close(&hfile);
			goto cleanup;
		}

		fres = f_write(&hfile, buffer, chunk, &bw);
		if (fres != FR_OK || bw != chunk)
		{
			uint8_t sdmsg[30];
			sprintf(sdmsg, "SD wr err:%d bw:%lu", (int)fres, (unsigned long)bw);
			BACKUP_DBG_CLEAR();
			m1_info_box_display_draw(INFO_BOX_ROW_1, sdmsg);
			m1_u8g2_nextpage();
			HAL_Delay(5000);
			f_close(&hfile);
			goto cleanup;
		}

		offset += chunk;
		fw_gui_progress_update(flash_size - offset);
	}

	f_close(&hfile);
	{
		uint8_t donemsg[30];
		sprintf(donemsg, "Done! %luKB", offset/1024);
		BACKUP_DBG_CLEAR();
		m1_info_box_display_draw(INFO_BOX_ROW_1, donemsg);
		m1_u8g2_nextpage();
	}
	HAL_Delay(5000);

cleanup:
	/* Restore IO9 to input */
	GPIO_InitStruct.Pin = ESP32_IO9_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(ESP32_IO9_GPIO_Port, &GPIO_InitStruct);

	/* Reset ESP32 back to normal boot */
	esp_loader_reset_target();
	HAL_Delay(100);
	esp32_UART_change_baudrate(ESP32_UART_BAUDRATE);
	m1_ringbuffer_reset(&esp32_rb_hdl);
	esp32_UART_deinit();

	m1_device_stat.op_mode = old_op_mode;
	xQueueReset(main_q_hdl);
} // void setting_esp32_backup_flash(void)


/******************************************************************************/
/**
  * @brief  Display ESP32 boot info by resetting the ESP32 and reading UART output
  * @param  None
  * @retval None
  */
/******************************************************************************/
void setting_esp32_check_info(void)
{
	uint8_t line[GUI_DISP_LINE_LEN_MAX + 1];
	uint16_t n, total = 0;
	uint16_t buf_size = 8192;
	uint8_t *buf = (uint8_t *)malloc(buf_size);
	if (!buf)
	{
		m1_info_box_display_draw(INFO_BOX_ROW_1, "malloc failed");
		m1_u8g2_nextpage();
		HAL_Delay(3000);
		return;
	}

	/* Show "Resetting..." */
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
	u8g2_DrawBox(&m1_u8g2, 0, INFO_BOX_Y_POS_ROW_1 - M1_SUB_MENU_FONT_HEIGHT, 128, 40);
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	m1_info_box_display_draw(INFO_BOX_ROW_1, "Resetting ESP32..");
	m1_u8g2_nextpage();

	/* Init UART, reset ESP32, wait for boot messages */
	esp32_UART_deinit();
	esp32_UART_init();
	m1_ringbuffer_reset(&esp32_rb_hdl);

	/* Toggle EN pin to reset ESP32 */
	HAL_GPIO_WritePin(ESP32_RESET_GPIO_Port, ESP32_RESET_Pin, GPIO_PIN_RESET);
	HAL_Delay(100);
	HAL_GPIO_WritePin(ESP32_RESET_GPIO_Port, ESP32_RESET_Pin, GPIO_PIN_SET);

	/* Capture all boot output for 6 seconds */
	total = 0;
	for (int wait = 0; wait < 60; wait++)
	{
		HAL_Delay(100);
		n = m1_ringbuffer_read(&esp32_rb_hdl, buf + total, buf_size - 1 - total);
		total += n;
		if (total >= buf_size - 1) break;
	}
	buf[total] = '\0';
	n = total;

	/* Clear display area */
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
	u8g2_DrawBox(&m1_u8g2, 0, INFO_BOX_Y_POS_ROW_1 - M1_SUB_MENU_FONT_HEIGHT, 128, 40);
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

	if (n == 0)
	{
		m1_info_box_display_draw(INFO_BOX_ROW_1, "No response");
		m1_u8g2_nextpage();
		HAL_Delay(3000);
		free(buf);
		esp32_UART_deinit();
		xQueueReset(main_q_hdl);
		return;
	}

	/* Search for our firmware identifier "M1 ESP32" in the boot output */
	/* Show the last 3 printable lines (app logs come after ROM/bootloader) */
	uint16_t line_starts[128];
	uint16_t line_lens[128];
	uint8_t line_count = 0;
	uint16_t i = 0;
	while (i < n && line_count < 128)
	{
		while (i < n && (buf[i] < 0x20 || buf[i] == '\r' || buf[i] == '\n'))
			i++;
		if (i >= n) break;

		uint16_t start = i;
		while (i < n && buf[i] != '\r' && buf[i] != '\n')
			i++;
		line_starts[line_count] = start;
		line_lens[line_count] = i - start;
		line_count++;
	}

	if (line_count == 0)
	{
		m1_info_box_display_draw(INFO_BOX_ROW_1, "No printable data");
		m1_u8g2_nextpage();
		HAL_Delay(3000);
		free(buf);
		esp32_UART_deinit();
		xQueueReset(main_q_hdl);
		return;
	}

	/* Row 1: stats + first line preview */
	{
		char hdr[25];
		uint16_t flen = line_lens[0];
		if (flen > 12) flen = 12;
		memcpy(line, &buf[line_starts[0]], flen);
		line[flen] = '\0';
		sprintf(hdr, "%d/%dL %s", n, line_count, (char*)line);
		hdr[GUI_DISP_LINE_LEN_MAX] = '\0';
		m1_info_box_display_draw(INFO_BOX_ROW_1, hdr);
	}
	/* Row 2: second-to-last line */
	if (line_count >= 2)
	{
		uint8_t li = line_count - 2;
		uint16_t len = line_lens[li];
		if (len > GUI_DISP_LINE_LEN_MAX) len = GUI_DISP_LINE_LEN_MAX;
		memcpy(line, &buf[line_starts[li]], len);
		line[len] = '\0';
		m1_info_box_display_draw(INFO_BOX_ROW_2, (char *)line);
	}
	/* Row 3: last line */
	{
		uint8_t li = line_count - 1;
		uint16_t len = line_lens[li];
		if (len > GUI_DISP_LINE_LEN_MAX) len = GUI_DISP_LINE_LEN_MAX;
		memcpy(line, &buf[line_starts[li]], len);
		line[len] = '\0';
		m1_info_box_display_draw(INFO_BOX_ROW_3, (char *)line);
	}

	m1_u8g2_nextpage();
	HAL_Delay(8000);
	free(buf);
	esp32_UART_deinit();
	xQueueReset(main_q_hdl);
} // void setting_esp32_check_info(void)


/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
static esp_loader_error_t m1_fw_flash_binary(uint8_t *payload, size_t size)
{
    esp_loader_error_t err;
    size_t written;
    static bool init_done = false;

    if ( !init_done )
    {
        printf("Erasing flash (this may take a while)...\r\n");
        err = esp_loader_flash_start(start_address, image_size, ESP32_IMAGE_CHUNK_SIZE);
        if (err != ESP_LOADER_SUCCESS)
        {
            printf("Erasing flash failed with error: %s.\r\n", get_error_string(err));
            if (err == ESP_LOADER_ERROR_INVALID_PARAM)
            {
                printf("If using Secure Download Mode, double check that the specified "
                       "target flash size is correct.\r\n");
            }
            return err;
        } // if (err != ESP_LOADER_SUCCESS)
        printf("Start programming\r\n");
        written = 0;
        init_done = true;
    } // if ( !init_done )

    if ( size )
    {
        err = esp_loader_flash_write(payload, size);
        if (err != ESP_LOADER_SUCCESS)
        {
            printf("\nPacket could not be written! Error %s.\r\n", get_error_string(err));
            return err;
        }

        written += size;

        int progress = (int)(((float)written / image_size) * 100);
        printf("\rProgress: %d %%", progress);

        return ESP_LOADER_SUCCESS;
    };

    printf("\nFinished programming\r\n");
    init_done = false; // reset

#ifdef MD5_ENABLED
    err = esp_loader_flash_verify();
    if (err == ESP_LOADER_ERROR_UNSUPPORTED_FUNC)
    {
        printf("ESP8266 does not support flash verify command.\r\n");
        return err;
    }
    else if (err != ESP_LOADER_SUCCESS)
    {
        printf("MD5 does not match. Error: %s\r\n", get_error_string(err));
        return err;
    }
    printf("Flash verified\r\n");
#endif

    return ESP_LOADER_SUCCESS;
} // static esp_loader_error_t m1_fw_flash_binary(uint8_t *payload, size_t size)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
static uint16_t esp32_get_boot_info(uint8_t *boot_msg, uint16_t boot_msg_len, uint16_t *len)
{
	uint16_t scan, run, start, next_start;

	if ( boot_msg==NULL )
		return 0;

	run = 0;
	start = 0;
	next_start = 0;
	while ( run < boot_msg_len )
	{
		if ( boot_msg[run]==ESP32_BOOT_MESSAGE_LF ) // End of line found?
		{
			start = next_start;
			next_start = run + 1;
			for (scan=start; scan<run; scan++)
			{
				if ( boot_msg[scan]==ESP32_BOOT_MESSAGE_SEPARATOR )
					break;
			} // for (k=start; k<run; k++)
			break; // EOL found, let break
		} // if ( boot_msg[run]==ESP32_BOOT_MESSAGE_LF )
		run++;
	} // while ( run < boot_msg_len )

	*len = run + 1; // length of message ended by EOL
	if ( run < boot_msg_len ) // Possible search found?
	{
		if ( scan < run ) // Search found?
			return (scan + 1); // Skip the position of the ESP32_BOOT_MESSAGE_SEPARATOR
	} // if ( run < boot_msg_len )

	return 0;
} // static uint16_t esp32_get_boot_info(uint8_t *boot_msg, uint16_t boot_msg_len, uint16_t *len)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void setting_esp32_gui_update(const S_M1_Menu_t *phmenu, uint8_t sel_item)
{
	uint8_t i, n_items;
	uint8_t menu_text_y;
	bool trunc;
	uint8_t prn_name[GUI_DISP_LINE_LEN_MAX + 1] = {0};
	uint16_t msg_len, msg_id;
	uint8_t *pboot_info;
	uint8_t win_start, win_end;
	const uint8_t max_visible = 3;

	n_items = phmenu->num_submenu_items;

	/* Compute scroll window: show max 3 items, scroll to keep sel_item visible */
	if ( n_items <= max_visible )
	{
		win_start = 0;
		win_end = n_items;
	}
	else
	{
		if ( sel_item < max_visible )
		{
			win_start = 0;
		}
		else
		{
			win_start = sel_item - max_visible + 1;
		}
		win_end = win_start + max_visible;
		if ( win_end > n_items )
		{
			win_end = n_items;
			win_start = win_end - max_visible;
		}
	}

	menu_text_y = THIS_LCD_MENU_TEXT_FIRST_ROW_Y;

	/* Graphic work starts here */
	m1_u8g2_firstpage(); // This call required for page drawing in mode 1
	do
	{
		for (i=win_start; i<win_end; i++)
		{
    		if ( i==sel_item )
    		{
    			// Draw box for selected menu item with text color
    			u8g2_DrawBox(&m1_u8g2, 0, menu_text_y - THIS_LCD_MENU_TEXT_ROW_SPACE + 2, M1_LCD_SUB_MENU_TEXT_FRAME_W, THIS_LCD_MENU_TEXT_ROW_SPACE);
    			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG); // set to background color
    			u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
    			u8g2_DrawStr(&m1_u8g2, 4, menu_text_y, phmenu->submenu[i]->title);
    			if ( i==1 ) // Index of the Start Address field
    			{
				// Draw arrows left and right
				u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - 40, menu_text_y + 2, 10, 10, arrowleft_10x10);
				u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - 20, menu_text_y + 2, 10, 10, arrowright_10x10);
    			}
    			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT); // return to text color
    			u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N); // return to default font
    		}
    		else
    		{
    			u8g2_DrawStr(&m1_u8g2, 4, menu_text_y, phmenu->submenu[i]->title);
    		}
    		menu_text_y += THIS_LCD_MENU_TEXT_ROW_SPACE;
		} // for (i=win_start; i<win_end; i++)

    	// Draw info box at the bottom
    	m1_info_box_display_init(true);

    	switch ( sel_item )
    	{
    		case 0: // Image file
    	    	if ( f_info && f_info->file_is_selected )
    	    	{
    	    		if ( strlen(f_info->file_name) > GUI_DISP_LINE_LEN_MAX )
    	    		{
    	    			strncpy(prn_name, f_info->file_name, GUI_DISP_LINE_LEN_MAX - 2);
    	    			prn_name[GUI_DISP_LINE_LEN_MAX - 2] = 0;
    	    			strcat(prn_name, "..");
    	    		}
    	    		else
    	    		{
    	    			strcpy(prn_name, f_info->file_name);
    	    		}
    	    	} // if ( f_info && f_info->file_is_selected )

    			switch ( esp32_update_status )
    			{
    				case M1_FW_UPDATE_READY:
    					m1_info_box_display_draw(INFO_BOX_ROW_1, prn_name);
    					break;

    				case M1_FW_IMAGE_FILE_ACCESS_ERROR:
    					m1_info_box_display_draw(INFO_BOX_ROW_1, "Image file error!");
    					break;

    				case M1_FW_CRC_FILE_ACCESS_ERROR:
    					m1_info_box_display_draw(INFO_BOX_ROW_1, "MD5 file error!");
    					break;

    				case M1_FW_CRC_FILE_INVALID:
		    			m1_info_box_display_draw(INFO_BOX_ROW_1, "Invalid MD5 file!");
						break;

    				case M1_FW_IMAGE_FILE_TYPE_ERROR:
    				case M1_FW_IMAGE_SIZE_INVALID:
		    			m1_info_box_display_draw(INFO_BOX_ROW_1, "Invalid image file!");
		    			break;

		    		case M1_FW_CRC_CHECKSUM_UNMATCHED:
		    			m1_info_box_display_draw(INFO_BOX_ROW_1, "Checksum failed!");
		    			break;

    				default:
    					break;
    			} // switch ( esp32_update_status )
    			break;

    		case 1: // Start address
    	    	sprintf(prn_name, "0x%06lX:", start_address);
    	    	m1_info_box_display_draw(INFO_BOX_ROW_1, prn_name);
    			break;

    		case 2: // Firmware update
    			switch ( esp32_update_status )
    			{
    				case M1_FW_UPDATE_READY:
    					m1_info_box_display_draw(INFO_BOX_ROW_1, "Ready to flash!");
    					break;

		    		case M1_FW_UPDATE_SUCCESS:
		    			m1_info_box_display_draw(INFO_BOX_ROW_1, "Update successfully!");
		    			esp32_update_status = M1_FW_UPDATE_NOT_READY; // Reset after process complete
		    			i = 0;
		    			msg_len = m1_ringbuffer_get_read_len(&esp32_rb_hdl);
		    			while ( msg_len )
		    			{
		    				pboot_info = m1_ringbuffer_get_read_address(&esp32_rb_hdl);
		    				msg_id = esp32_get_boot_info(pboot_info, msg_len, &msg_len);
		    				if ( msg_id )
		    				{
		    					if ( !i ) // First boot message
		    					{
		    						// Read part of the info message
		    						strncpy(prn_name, pboot_info + msg_id, msg_len - msg_id);
		    						m1_info_box_display_draw(INFO_BOX_ROW_2, prn_name);
		    						i++; // Move to next boot message
		    					} // if ( !i )
		    					else
		    					{
		    						// Read full info message
		    						msg_len = (msg_len <= GUI_DISP_LINE_LEN_MAX)?msg_len:GUI_DISP_LINE_LEN_MAX;
		    						strncpy(prn_name, pboot_info, msg_len);
		    						m1_info_box_display_draw(INFO_BOX_ROW_3, prn_name);
		    						break; // Having read enough info messages, let break
		    					} // else
		    				} // if ( msg_id )
		    				else
		    					break; // Do nothing if nothing found
		    				m1_ringbuffer_advance_read(&esp32_rb_hdl, msg_len); // Skip old message
		    				msg_len = m1_ringbuffer_get_read_len(&esp32_rb_hdl);
		    			} // while ( msg_len )
		    			break;

		    		case M1_FW_UPDATE_FAILED:
		    	    	m1_info_box_display_draw(INFO_BOX_ROW_1, "Update failed!");
		    			break;

		    		case M1_FW_UPDATE_LOW_BATTERY:
		    			m1_info_box_display_draw(INFO_BOX_ROW_1, "Battery level < 50%!");
		    			break;

    				default:
    					break;
    			} // switch ( esp32_update_status )
    			break;

		case 3: // Backup flash
			m1_info_box_display_draw(INFO_BOX_ROW_1, "Backup ESP32 flash");
			break;

		case 4: // ESP32 Info
			m1_info_box_display_draw(INFO_BOX_ROW_1, "Read ESP32 boot info");
			break;

    		default: // Unknown selection
    			break;
    	} // switch ( sel_item )
    } while (m1_u8g2_nextpage());

} // void setting_esp32_gui_update(const S_M1_Menu_t *phmenu, uint8_t sel_item)
