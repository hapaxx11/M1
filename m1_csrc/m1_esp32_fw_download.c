/* See COPYING.txt for license details. */

/*
 * m1_esp32_fw_download.c
 *
 * ESP32-C6 AT firmware download manager.
 * Downloads firmware images (.bin + .md5) from GitHub Releases to SD card.
 *
 * The key difference from M1 firmware download (m1_fw_download.c) is that
 * ESP32 firmware requires BOTH a .bin and a .md5 sidecar file.  The .md5
 * URL is constructed from the .bin URL by replacing the extension.
 *
 * M1 Project
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "app_freertos.h"
#include "m1_esp32_fw_download.h"
#include "m1_fw_source.h"
#include "m1_http_client.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_system.h"
#include "m1_tasks.h"
#include "m1_watchdog.h"
#include "m1_file_browser.h"
#include "m1_log_debug.h"

#define ESP32_DL_TAG "ESP32_DL"

/* SD card directory for downloaded ESP32 firmware images */
#define ESP32_FW_DOWNLOAD_DIR     "0:/ESP32_FW"

/* Display layout constants (matching m1_fw_download.c) */
#define DL_TITLE_Y          10
#define DL_LIST_Y_START     22
#define DL_LIST_ROW_H       10
#define DL_LIST_MAX_VISIBLE  4
#define DL_PROGRESS_BAR_X    4
#define DL_PROGRESS_BAR_Y   30
#define DL_PROGRESS_BAR_W  120
#define DL_PROGRESS_BAR_H   10
#define DL_PROGRESS_UPDATE_BYTES  8192

/* State for progress callback */
static uint32_t s_dl_downloaded;
static uint32_t s_dl_total;

/* Large buffers in static storage to avoid stack overflow in menu task.
 * Safe: only one blocking delegate runs at a time. */
static fw_source_t  s_edl_sources[FW_SOURCE_MAX];
static fw_release_t s_edl_releases[FW_RELEASE_MAX];
static char         s_edl_sd_path[128];
static char         s_edl_md5_url[FW_RELEASE_URL_LEN];
static char         s_edl_md5_name[FW_RELEASE_ASSET_LEN];

/*
 * Draw a two-line message on screen.
 */
static void edl_show_message(const char *line1, const char *line2)
{
	m1_u8g2_firstpage();
	do {
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		if (line1)
			u8g2_DrawStr(&m1_u8g2, 4, 20, line1);
		if (line2)
			u8g2_DrawStr(&m1_u8g2, 4, 36, line2);
	} while (m1_u8g2_nextpage());
}

/*
 * Wait for OK or BACK button press.
 * Returns true if OK pressed, false if BACK pressed.
 */
static bool edl_wait_ok_or_back(void)
{
	S_M1_Main_Q_t q_item;
	S_M1_Buttons_Status btn;

	for (;;)
	{
		if (xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY) == pdTRUE)
		{
			if (q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				xQueueReceive(button_events_q_hdl, &btn, 0);
				if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return true;
				}
				if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return false;
				}
			}
		}
	}
}

/*
 * Draw a selection list on screen.
 */
static void edl_draw_list(const char *title, const char **items, uint8_t count,
                           uint8_t selected, uint8_t scroll_off)
{
	uint8_t i;
	uint8_t y;
	uint8_t vis_count = count - scroll_off;
	if (vis_count > DL_LIST_MAX_VISIBLE)
		vis_count = DL_LIST_MAX_VISIBLE;

	m1_u8g2_firstpage();
	do {
		u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, DL_TITLE_Y, title);

		for (i = 0; i < vis_count; i++)
		{
			uint8_t idx = scroll_off + i;
			y = DL_LIST_Y_START + i * DL_LIST_ROW_H;

			if (idx == selected)
			{
				u8g2_DrawBox(&m1_u8g2, 0, y - DL_LIST_ROW_H + 2, M1_LCD_DISPLAY_WIDTH, DL_LIST_ROW_H);
				u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
				u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
				u8g2_DrawStr(&m1_u8g2, 4, y, items[idx]);
				u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
				u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
			}
			else
			{
				u8g2_DrawStr(&m1_u8g2, 4, y, items[idx]);
			}
		}

		if (count > DL_LIST_MAX_VISIBLE)
		{
			if (scroll_off > 0)
				u8g2_DrawXBMP(&m1_u8g2, 120, 14, 8, 8, arrowup_8x8);
			if (scroll_off + DL_LIST_MAX_VISIBLE < count)
				u8g2_DrawXBMP(&m1_u8g2, 120, 52, 8, 8, arrowdown_8x8);
		}
	} while (m1_u8g2_nextpage());
}

/*
 * Generic list selection with scrolling.
 */
static bool edl_select_from_list(const char *title, const char **items,
                                  uint8_t count, uint8_t *selected)
{
	S_M1_Main_Q_t q_item;
	S_M1_Buttons_Status btn;
	uint8_t sel = 0;
	uint8_t scroll = 0;

	if (count == 0)
		return false;

	for (;;)
	{
		if (sel < scroll)
			scroll = sel;
		if (sel >= scroll + DL_LIST_MAX_VISIBLE)
			scroll = sel - DL_LIST_MAX_VISIBLE + 1;

		edl_draw_list(title, items, count, sel, scroll);

		if (xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY) == pdTRUE)
		{
			if (q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				xQueueReceive(button_events_q_hdl, &btn, 0);

				if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (sel > 0) sel--;
					else sel = count - 1;
				}
				else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (sel < count - 1) sel++;
					else sel = 0;
				}
				else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					*selected = sel;
					xQueueReset(main_q_hdl);
					return true;
				}
				else if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return false;
				}
			}
		}
	}
}

/*
 * Draw download progress screen.
 */
static void edl_draw_progress(const char *label, uint32_t downloaded, uint32_t total)
{
	char status_str[24];
	char size_str[24];
	uint8_t pct = 0;
	uint8_t bar_fill;

	if (total > 0)
		pct = (uint8_t)((uint64_t)downloaded * 100 / total);

	bar_fill = (uint8_t)((uint32_t)pct * DL_PROGRESS_BAR_W / 100);

	snprintf(status_str, sizeof(status_str), "%s %u%%", label, pct);

	if (total > 0)
		snprintf(size_str, sizeof(size_str), "%lu / %lu KB",
		         (unsigned long)(downloaded / 1024),
		         (unsigned long)(total / 1024));
	else
		snprintf(size_str, sizeof(size_str), "%lu KB",
		         (unsigned long)(downloaded / 1024));

	m1_u8g2_firstpage();
	do {
		u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_DrawStr(&m1_u8g2, 4, 14, status_str);
		u8g2_DrawFrame(&m1_u8g2, DL_PROGRESS_BAR_X, DL_PROGRESS_BAR_Y,
		               DL_PROGRESS_BAR_W, DL_PROGRESS_BAR_H);
		if (bar_fill > 2)
			u8g2_DrawBox(&m1_u8g2, DL_PROGRESS_BAR_X + 1, DL_PROGRESS_BAR_Y + 1,
			             bar_fill - 2, DL_PROGRESS_BAR_H - 2);
		u8g2_DrawStr(&m1_u8g2, 4, 52, size_str);
	} while (m1_u8g2_nextpage());
}

/*
 * Progress callback for http_download_to_file.
 */
static bool edl_progress_callback(uint32_t downloaded, uint32_t total)
{
	s_dl_downloaded = downloaded;
	s_dl_total = total;

	m1_wdt_reset();

	static uint32_t last_display_update = 0;
	if (downloaded == 0)
		last_display_update = 0;
	if (downloaded - last_display_update >= DL_PROGRESS_UPDATE_BYTES
	    || downloaded == 0 || (total > 0 && downloaded >= total))
	{
		edl_draw_progress("Downloading...", downloaded, total);
		last_display_update = downloaded;
	}

	/* Non-blocking check for BACK button to cancel */
	S_M1_Main_Q_t q_item;
	S_M1_Buttons_Status btn;
	if (xQueueReceive(main_q_hdl, &q_item, 0) == pdTRUE)
	{
		if (q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				return false;
		}
	}

	return true;
}

/*
 * Construct the .md5 URL from the .bin URL by replacing the extension.
 */
static bool build_md5_url(const char *bin_url, char *md5_url, size_t md5_url_size)
{
	size_t len = strlen(bin_url);

	/* Must end with ".bin" */
	if (len < 4 || strcmp(bin_url + len - 4, ".bin") != 0)
		return false;
	/* Output is same length as input (".md5" == ".bin") + NUL */
	if (len + 1 > md5_url_size)
		return false;

	memcpy(md5_url, bin_url, len - 4);
	memcpy(md5_url + len - 4, ".md5", 5); /* includes NUL */
	return true;
}

/*
 * Construct the .md5 filename from the .bin filename.
 */
static bool build_md5_filename(const char *bin_name, char *md5_name, size_t md5_name_size)
{
	size_t len = strlen(bin_name);

	if (len < 4 || strcmp(bin_name + len - 4, ".bin") != 0)
		return false;
	if (len + 1 > md5_name_size)
		return false;

	memcpy(md5_name, bin_name, len - 4);
	memcpy(md5_name + len - 4, ".md5", 5);
	return true;
}

/*
 * Main entry point for ESP32 firmware download UI.
 */
void esp32_fw_download_start(void)
{
	fw_source_t  *sources  = s_edl_sources;
	fw_release_t *releases = s_edl_releases;
	char         *sd_path  = s_edl_sd_path;
	char         *md5_url  = s_edl_md5_url;
	char         *md5_name = s_edl_md5_name;
	uint8_t source_count, release_count;
	uint8_t selected;
	http_status_t dl_status;

	/* Step 1: Check WiFi */
	if (!http_is_ready())
	{
		edl_show_message("WiFi not connected", "Connect first");
		vTaskDelay(pdMS_TO_TICKS(2500));
		xQueueReset(main_q_hdl);
		return;
	}

	/* Step 2: Load ESP32 firmware sources */
	edl_show_message("Loading sources...", NULL);
	source_count = fw_source_load_config_filtered(sources, "esp32");
	if (source_count == 0)
	{
		edl_show_message("No ESP32 sources", "Check fw_sources.txt");
		vTaskDelay(pdMS_TO_TICKS(2500));
		xQueueReset(main_q_hdl);
		return;
	}

source_selection:
	;
	/* Step 3: Show source list */
	const char *src_names[FW_SOURCE_MAX];
	for (uint8_t i = 0; i < source_count; i++)
		src_names[i] = sources[i].name;

	if (!edl_select_from_list("ESP32 FW Source", src_names, source_count, &selected))
	{
		xQueueReset(main_q_hdl);
		return;
	}

	/* Step 4: Fetch releases */
	edl_show_message("Fetching releases...", sources[selected].name);
	m1_wdt_reset();
	release_count = fw_source_fetch_releases(&sources[selected], releases);
	if (release_count == 0)
	{
		edl_show_message("No releases found", NULL);
		vTaskDelay(pdMS_TO_TICKS(2000));
		goto source_selection;
	}

	/* Step 5: Show release list */
	uint8_t rel_sel;
	const char *rel_names[FW_RELEASE_MAX];
	char rel_display[FW_RELEASE_MAX][FW_RELEASE_NAME_LEN];
	for (uint8_t i = 0; i < release_count; i++)
	{
		if (releases[i].tag[0])
			snprintf(rel_display[i], sizeof(rel_display[i]), "%s", releases[i].tag);
		else
			snprintf(rel_display[i], sizeof(rel_display[i]), "%s", releases[i].name);
		rel_names[i] = rel_display[i];
	}

	if (!edl_select_from_list("Select Release", rel_names, release_count, &rel_sel))
		goto source_selection;

	/* Step 6: Confirm download */
	{
		const fw_release_t *rel = &releases[rel_sel];
		char size_str[24];

		if (rel->asset_size >= 1024 * 1024)
			snprintf(size_str, sizeof(size_str), "Size: %lu.%02lu MB",
			         (unsigned long)(rel->asset_size / (1024 * 1024)),
			         (unsigned long)((rel->asset_size % (1024 * 1024)) * 100 / (1024 * 1024)));
		else if (rel->asset_size > 0)
			snprintf(size_str, sizeof(size_str), "Size: %lu KB",
			         (unsigned long)(rel->asset_size / 1024));
		else
			strncpy(size_str, "Size: unknown", sizeof(size_str));

		char name_str[25];
		strncpy(name_str, rel->asset_name, 22);
		name_str[22] = '\0';
		if (strlen(rel->asset_name) > 22)
			strcpy(name_str + 20, "..");

		m1_u8g2_firstpage();
		do {
			u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
			u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
			u8g2_DrawStr(&m1_u8g2, 2, DL_TITLE_Y, "Download ESP32 FW");
			u8g2_DrawStr(&m1_u8g2, 4, 24, name_str);
			u8g2_DrawStr(&m1_u8g2, 4, 34, size_str);
			u8g2_DrawStr(&m1_u8g2, 4, 44, rel->tag);
			m1_draw_bottom_bar(&m1_u8g2, NULL, NULL, "OK:Download", NULL);
		} while (m1_u8g2_nextpage());

		if (!edl_wait_ok_or_back())
			goto source_selection;
	}

	/* Step 7: Download .bin */
	{
		const fw_release_t *rel = &releases[rel_sel];

		m1_fb_make_dir(ESP32_FW_DOWNLOAD_DIR);

		snprintf(sd_path, sizeof(s_edl_sd_path), "%s/%s",
		         ESP32_FW_DOWNLOAD_DIR, rel->asset_name);

		s_dl_downloaded = 0;
		s_dl_total = rel->asset_size;

		edl_draw_progress("Downloading...", 0, rel->asset_size);

		dl_status = http_download_to_file(rel->download_url, sd_path,
		                                   edl_progress_callback, 60);

		if (dl_status != HTTP_OK)
		{
			const char *err_msg;
			switch (dl_status)
			{
				case HTTP_ERR_NO_WIFI:        err_msg = "WiFi not connected"; break;
				case HTTP_ERR_CONNECT_FAIL:   err_msg = "Connection failed"; break;
				case HTTP_ERR_TIMEOUT:        err_msg = "Download timed out"; break;
				case HTTP_ERR_HTTP_ERROR:      err_msg = "Server error"; break;
				case HTTP_ERR_SD_WRITE_FAIL:  err_msg = "SD write error"; break;
				case HTTP_ERR_SD_OPEN_FAIL:   err_msg = "Cannot create file"; break;
				case HTTP_ERR_CANCELLED:      err_msg = "Cancelled"; break;
				default:                       err_msg = "Download failed"; break;
			}
			edl_show_message("Error:", err_msg);
			vTaskDelay(pdMS_TO_TICKS(3000));
			xQueueReset(main_q_hdl);
			return;
		}

		/* Step 8: Download .md5 sidecar */
		if (!build_md5_url(rel->download_url, md5_url, FW_RELEASE_URL_LEN) ||
		    !build_md5_filename(rel->asset_name, md5_name, FW_RELEASE_ASSET_LEN))
		{
			edl_show_message("BIN downloaded", "MD5 name error");
			vTaskDelay(pdMS_TO_TICKS(3000));
			xQueueReset(main_q_hdl);
			return;
		}

		snprintf(sd_path, sizeof(s_edl_sd_path), "%s/%s",
		         ESP32_FW_DOWNLOAD_DIR, md5_name);

		edl_show_message("Downloading MD5...", md5_name);
		m1_wdt_reset();

		dl_status = http_download_to_file(md5_url, sd_path, NULL, 30);

		if (dl_status != HTTP_OK)
		{
			edl_show_message("BIN OK, MD5 failed", "Flash manually");
			vTaskDelay(pdMS_TO_TICKS(3000));
			xQueueReset(main_q_hdl);
			return;
		}

		/* Step 9: Show success */
		char msg[32];
		snprintf(msg, sizeof(msg), "%lu KB downloaded",
		         (unsigned long)(s_dl_downloaded / 1024));

		m1_u8g2_firstpage();
		do {
			u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
			u8g2_DrawStr(&m1_u8g2, 4, 16, "Download complete!");
			u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
			u8g2_DrawStr(&m1_u8g2, 4, 30, msg);
			u8g2_DrawStr(&m1_u8g2, 4, 42, rel->asset_name);
			u8g2_DrawStr(&m1_u8g2, 4, 54, "Flash via Image File");
			m1_draw_bottom_bar(&m1_u8g2, NULL, NULL, "OK:Continue", NULL);
		} while (m1_u8g2_nextpage());

		edl_wait_ok_or_back();
	}

	xQueueReset(main_q_hdl);
}
