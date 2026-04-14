/* See COPYING.txt for license details. */

/*
 * m1_fw_download.c
 *
 * WiFi firmware download manager and UI.
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
#include "m1_fw_download.h"
#include "m1_fw_source.h"
#include "m1_http_client.h"
#include "m1_fw_update.h"
#include "m1_fw_update_bl.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_system.h"
#include "m1_tasks.h"
#include "m1_watchdog.h"
#include "m1_file_browser.h"
#include "m1_log_debug.h"
#include "m1_scene.h"

#define FW_DL_TAG "FW_DL"

/* SD card directory for downloaded firmware images */
#define FW_DOWNLOAD_DIR     "0:/Firmware"

/* Display layout constants */
#define DL_TITLE_Y          10
#define DL_LIST_Y_START     22
#define DL_LIST_MAX_VISIBLE  ((uint8_t)(42 / m1_menu_item_h()))
#define DL_PROGRESS_BAR_X    4
#define DL_PROGRESS_BAR_Y   30
#define DL_PROGRESS_BAR_W  120
#define DL_PROGRESS_BAR_H   10
#define DL_PROGRESS_UPDATE_BYTES  8192  /* Update display every 8KB */

/* Forward declarations */
static bool dl_show_source_list(fw_source_t *sources, uint8_t count, uint8_t *selected);
static bool dl_show_release_list(fw_release_t *releases, uint8_t count, uint8_t *selected);
static bool dl_confirm_download(const fw_release_t *release);
static http_status_t dl_perform_download(const fw_release_t *release);
static bool dl_progress_callback(uint32_t downloaded, uint32_t total);
static void dl_show_result(http_status_t status, const fw_release_t *release);
static void dl_show_message(const char *line1, const char *line2);
static bool dl_wait_ok_or_back(void);

/* State for progress callback (used by dl_progress_callback) */
static uint32_t s_dl_downloaded;
static uint32_t s_dl_total;
static bool s_dl_cancelled;

/*
 * Draw a selection list on screen. Generic helper.
 *
 * title:       Title string at top
 * items:       Array of item name strings
 * count:       Number of items
 * selected:    Currently selected index (highlighted)
 * scroll_off:  Scroll offset for lists > DL_LIST_MAX_VISIBLE
 */
static void dl_draw_list(const char *title, const char **items, uint8_t count,
                          uint8_t selected, uint8_t scroll_off)
{
	uint8_t i;
	uint8_t y;
	const uint8_t item_h = m1_menu_item_h();
	const uint8_t text_ofs = item_h - 1;
	uint8_t vis_count = count - scroll_off;
	if (vis_count > DL_LIST_MAX_VISIBLE)
		vis_count = DL_LIST_MAX_VISIBLE;

	m1_u8g2_firstpage();
	do {
		/* Title bar */
		u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, DL_TITLE_Y, title);

		/* List items */
		u8g2_SetFont(&m1_u8g2, m1_menu_font());
		for (i = 0; i < vis_count; i++)
		{
			uint8_t idx = scroll_off + i;
			y = DL_LIST_Y_START + i * item_h;

			if (idx == selected)
			{
				u8g2_DrawBox(&m1_u8g2, 0, y - item_h + 2, M1_MENU_TEXT_W, item_h);
				u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
				u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
				u8g2_DrawStr(&m1_u8g2, 4, y, items[idx]);
				u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
				u8g2_SetFont(&m1_u8g2, m1_menu_font());
			}
			else
			{
				u8g2_DrawStr(&m1_u8g2, 4, y, items[idx]);
			}
		}

		/* Scroll indicator if needed */
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
 * Draw a two-line message on screen.
 */
static void dl_show_message(const char *line1, const char *line2)
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
static bool dl_wait_ok_or_back(void)
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
 * Generic list selection with scrolling.
 * Returns true if user selected (index in *selected), false if BACK pressed.
 */
static bool dl_select_from_list(const char *title, const char **items, uint8_t count, uint8_t *selected)
{
	S_M1_Main_Q_t q_item;
	S_M1_Buttons_Status btn;
	uint8_t sel = 0;
	uint8_t scroll = 0;

	if (count == 0)
		return false;

	for (;;)
	{
		/* Adjust scroll to keep selection visible */
		if (sel < scroll)
			scroll = sel;
		if (sel >= scroll + DL_LIST_MAX_VISIBLE)
			scroll = sel - DL_LIST_MAX_VISIBLE + 1;

		dl_draw_list(title, items, count, sel, scroll);

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
 * Show the source selection screen.
 */
static bool dl_show_source_list(fw_source_t *sources, uint8_t count, uint8_t *selected)
{
	const char *names[FW_SOURCE_MAX];
	for (uint8_t i = 0; i < count; i++)
		names[i] = sources[i].name;

	return dl_select_from_list("Select Source", names, count, selected);
}

/*
 * Show the release selection screen.
 */
static bool dl_show_release_list(fw_release_t *releases, uint8_t count,
                                  uint8_t *selected)
{
	const char *names[FW_RELEASE_MAX];
	char display_names[FW_RELEASE_MAX][FW_RELEASE_NAME_LEN];

	for (uint8_t i = 0; i < count; i++)
	{
		/* Show tag (version) - more concise than full name */
		if (releases[i].tag[0])
			snprintf(display_names[i], sizeof(display_names[i]), "%s", releases[i].tag);
		else
			snprintf(display_names[i], sizeof(display_names[i]), "%s", releases[i].name);
		names[i] = display_names[i];
	}

	return dl_select_from_list("Select Release", names, count, selected);
}

/*
 * Show the download confirmation screen.
 */
static bool dl_confirm_download(const fw_release_t *release)
{
	char size_str[24];

	/* Format file size */
	if (release->asset_size > 0)
	{
		if (release->asset_size >= 1024 * 1024)
			snprintf(size_str, sizeof(size_str), "Size: %lu.%02lu MB",
			         (unsigned long)(release->asset_size / (1024 * 1024)),
			         (unsigned long)((release->asset_size % (1024 * 1024)) * 100 / (1024 * 1024)));
		else
			snprintf(size_str, sizeof(size_str), "Size: %lu KB",
			         (unsigned long)(release->asset_size / 1024));
	}
	else
	{
		strncpy(size_str, "Size: unknown", sizeof(size_str));
	}

	/* Truncate asset name for display (max 22 chars + 2 for ".." + NUL) */
	char name_str[25];
	strncpy(name_str, release->asset_name, 22);
	name_str[22] = '\0';
	if (strlen(release->asset_name) > 22)
		strcpy(name_str + 20, "..");

	m1_u8g2_firstpage();
	do {
		u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

		/* Title */
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, DL_TITLE_Y, "Confirm Download");

		/* File info */
		u8g2_DrawStr(&m1_u8g2, 4, 24, name_str);
		u8g2_DrawStr(&m1_u8g2, 4, 34, size_str);
		u8g2_DrawStr(&m1_u8g2, 4, 44, release->tag);

		/* Bottom bar: OK to download */
		m1_draw_bottom_bar(&m1_u8g2, NULL, NULL, "OK:Download", NULL);
	} while (m1_u8g2_nextpage());

	return dl_wait_ok_or_back();
}

/*
 * Draw download progress screen.
 */
static void dl_draw_progress(uint32_t downloaded, uint32_t total)
{
	char status_str[24];
	char size_str[24];
	uint8_t pct = 0;
	uint8_t bar_fill;

	if (total > 0)
		pct = (uint8_t)((uint64_t)downloaded * 100 / total);

	bar_fill = (uint8_t)((uint32_t)pct * DL_PROGRESS_BAR_W / 100);

	/* Format status strings */
	snprintf(status_str, sizeof(status_str), "Downloading... %u%%", pct);

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

		/* Title */
		u8g2_DrawStr(&m1_u8g2, 4, 14, status_str);

		/* Progress bar outline */
		u8g2_DrawFrame(&m1_u8g2, DL_PROGRESS_BAR_X, DL_PROGRESS_BAR_Y,
		               DL_PROGRESS_BAR_W, DL_PROGRESS_BAR_H);

		/* Progress bar fill */
		if (bar_fill > 0)
			u8g2_DrawBox(&m1_u8g2, DL_PROGRESS_BAR_X + 1, DL_PROGRESS_BAR_Y + 1,
			             bar_fill - 2, DL_PROGRESS_BAR_H - 2);

		/* Size info */
		u8g2_DrawStr(&m1_u8g2, 4, 52, size_str);
	} while (m1_u8g2_nextpage());
}

/*
 * Progress callback for http_download_to_file.
 * Updates the display and checks for cancel (BACK button).
 */
static bool dl_progress_callback(uint32_t downloaded, uint32_t total)
{
	s_dl_downloaded = downloaded;
	s_dl_total = total;

	m1_wdt_reset();

	/* Update display periodically to avoid excessive redraws */
	static uint32_t last_display_update = 0;
	if (downloaded == 0)
		last_display_update = 0; /* reset on new download */
	if (downloaded - last_display_update >= DL_PROGRESS_UPDATE_BYTES
	    || downloaded == 0 || (total > 0 && downloaded >= total))
	{
		dl_draw_progress(downloaded, total);
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
			{
				s_dl_cancelled = true;
				return false; /* cancel download */
			}
		}
	}

	return true; /* continue */
}

/*
 * Perform the actual download of a firmware release.
 */
static http_status_t dl_perform_download(const fw_release_t *release)
{
	char sd_path[128];

	/* Ensure download directory exists */
	m1_fb_make_dir(FW_DOWNLOAD_DIR);

	/* Build SD card path */
	snprintf(sd_path, sizeof(sd_path), "%s/%s", FW_DOWNLOAD_DIR, release->asset_name);

	s_dl_cancelled = false;
	s_dl_downloaded = 0;
	s_dl_total = release->asset_size;

	/* Show initial progress */
	dl_draw_progress(0, release->asset_size);

	/* Download! */
	return http_download_to_file(release->download_url, sd_path,
	                              dl_progress_callback, 60);
}

/*
 * Show download result and offer to flash.
 */
static void dl_show_result(http_status_t status, const fw_release_t *release)
{
	if (status == HTTP_OK)
	{
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
			u8g2_DrawStr(&m1_u8g2, 4, 42, release->asset_name);
			m1_draw_bottom_bar(&m1_u8g2, NULL, NULL, "OK:Continue", NULL);
		} while (m1_u8g2_nextpage());

		dl_wait_ok_or_back();
	}
	else if (status == HTTP_ERR_CANCELLED)
	{
		dl_show_message("Download cancelled", NULL);
		vTaskDelay(pdMS_TO_TICKS(2000));
	}
	else
	{
		const char *err_msg;
		switch (status)
		{
			case HTTP_ERR_NO_WIFI:        err_msg = "WiFi not connected"; break;
			case HTTP_ERR_DNS_FAIL:       err_msg = "DNS lookup failed"; break;
			case HTTP_ERR_CONNECT_FAIL:   err_msg = "Connection failed"; break;
			case HTTP_ERR_TIMEOUT:        err_msg = "Download timed out"; break;
			case HTTP_ERR_HTTP_ERROR:      err_msg = "Server error"; break;
			case HTTP_ERR_SD_WRITE_FAIL:  err_msg = "SD write error"; break;
			case HTTP_ERR_SD_OPEN_FAIL:   err_msg = "Cannot create file"; break;
			case HTTP_ERR_REDIRECT_LOOP:  err_msg = "Too many redirects"; break;
			default:                       err_msg = "Download failed"; break;
		}
		dl_show_message("Error:", err_msg);
		vTaskDelay(pdMS_TO_TICKS(3000));
	}
}

/*
 * Main entry point for firmware download UI.
 */
void fw_download_start(void)
{
	fw_source_t sources[FW_SOURCE_MAX];
	fw_release_t releases[FW_RELEASE_MAX];
	uint8_t source_count, release_count;
	uint8_t selected;
	http_status_t dl_status;

	/* Step 1: Check WiFi */
	if (!http_is_ready())
	{
		dl_show_message("WiFi not connected", "Connect first");
		vTaskDelay(pdMS_TO_TICKS(2500));
		xQueueReset(main_q_hdl);
		return;
	}

	/* Step 2: Load source configuration (M1 firmware sources only) */
	dl_show_message("Loading sources...", NULL);
	source_count = fw_source_load_config_filtered(sources, "firmware");
	if (source_count == 0)
	{
		dl_show_message("No sources found", "Check fw_sources.txt");
		vTaskDelay(pdMS_TO_TICKS(2500));
		xQueueReset(main_q_hdl);
		return;
	}

source_selection:
	/* Step 3: Show source list */
	if (!dl_show_source_list(sources, source_count, &selected))
	{
		xQueueReset(main_q_hdl);
		return; /* BACK pressed */
	}

	/* Step 4: Fetch releases for selected source */
	dl_show_message("Fetching releases...", sources[selected].name);
	m1_wdt_reset();
	http_status_t http_err = HTTP_OK;
	release_count = fw_source_fetch_releases(&sources[selected], releases, &http_err);
	if (release_count == 0)
	{
		const char *reason = http_status_str(http_err);
		if (!reason && http_err == HTTP_OK)
			reason = "No matching assets";
		dl_show_message("No releases found", reason);
		vTaskDelay(pdMS_TO_TICKS(2000));
		goto source_selection;
	}

	/* Step 5: Show release list */
	uint8_t rel_sel;
	if (!dl_show_release_list(releases, release_count, &rel_sel))
		goto source_selection; /* BACK → return to source list */

	/* Step 6: Confirm download */
	if (!dl_confirm_download(&releases[rel_sel]))
		goto source_selection; /* BACK → return to source list */

	/* Step 7: Download */
	dl_status = dl_perform_download(&releases[rel_sel]);

	/* Step 8: Show result */
	dl_show_result(dl_status, &releases[rel_sel]);

	xQueueReset(main_q_hdl);
}
