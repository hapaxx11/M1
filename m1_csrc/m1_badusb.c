/* See COPYING.txt for license details. */

/*
 * m1_badusb.c
 *
 * BadUSB — USB HID keyboard injection with DuckyScript parser.
 * Reads script files from SD card and types keystrokes via USB HID.
 */

/*************************** I N C L U D E S **********************************/
#include "app_freertos.h"
#include "cmsis_os.h"
#include "main.h"
#include "m1_lcd.h"
#include "m1_display.h"
#include "m1_file_browser.h"
#include "m1_usb_cdc_msc.h"
#include "m1_menu.h"
#include "m1_tasks.h"
#include "m1_system.h"
#include "m1_sdcard.h"
#include "m1_compile_cfg.h"
#include "m1_badusb.h"
#include "usbd_hid.h"
#include "m1_log_debug.h"
#include "badusb_parser.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef M1_APP_BADUSB_ENABLE

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG  "BUSB"

/* Timing */
#define BADUSB_ENUM_TIMEOUT_MS    5000  /* Max wait for host to enumerate HID */
#define BADUSB_ENUM_POLL_MS       50    /* Poll interval for enumeration check */
#define BADUSB_KEY_PRESS_MS       6     /* Hold key down (must exceed bInterval=2ms) */
#define BADUSB_KEY_RELEASE_MS     6     /* Delay after release */
#define BADUSB_INTER_CHAR_MS      2     /* Between characters in STRING */
#define BADUSB_TX_WAIT_MS         20    /* Max wait for HID TX complete */
#define BADUSB_HID_SETTLE_MS      3000  /* Extra delay for OS to load HID drivers */

/************************** S T R U C T U R E S *******************************/

/***************************** V A R I A B L E S ******************************/

static badusb_state_t badusb_state;

/* USB HID report buffer: [modifier, reserved, key1..key6] */
static uint8_t hid_report[8];

/* ASCII to HID scancode table is provided by badusb_parser (busb_ascii_to_hid()) */

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static void badusb_wait_tx_idle(void);
void badusb_send_key(uint8_t modifier, uint8_t keycode);
static void badusb_release_all(void);
void badusb_type_char(char c);
void badusb_type_string(const char *str);
static bool badusb_parse_line(const char *line);
static void badusb_show_progress(const char *filename);
static bool badusb_check_abort(void);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/*============================================================================*/
/**
  * @brief  Show a crash breadcrumb on the LCD (survives until reboot splash)
  */
/*============================================================================*/
static void badusb_breadcrumb(uint8_t phase)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "BadUSB Phase: %d", phase);
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 30, 124, buf, TEXT_ALIGN_CENTER);
    m1_u8g2_nextpage();
}

/*============================================================================*/
/**
  * @brief  Wait for previous HID report transfer to complete (IDLE state)
  */
/*============================================================================*/
static void badusb_wait_tx_idle(void)
{
    USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (hhid == NULL) return;

    uint32_t start = osKernelGetTickCount();
    while (hhid->state != HID_IDLE)
    {
        if ((osKernelGetTickCount() - start) > BADUSB_TX_WAIT_MS)
            break;
        osDelay(1);
    }
}

/*============================================================================*/
/**
  * @brief  Send a key press (modifier + keycode), wait, then release
  */
/*============================================================================*/
void badusb_send_key(uint8_t modifier, uint8_t keycode)
{
    badusb_wait_tx_idle();
    memset(hid_report, 0, sizeof(hid_report));
    hid_report[0] = modifier;
    hid_report[2] = keycode;
    USBD_HID_SendReport(&hUsbDeviceFS, hid_report, sizeof(hid_report));
    osDelay(BADUSB_KEY_PRESS_MS);

    badusb_release_all();
    osDelay(BADUSB_KEY_RELEASE_MS);
}


/*============================================================================*/
/**
  * @brief  Release all keys (send empty report)
  */
/*============================================================================*/
static void badusb_release_all(void)
{
    badusb_wait_tx_idle();
    memset(hid_report, 0, sizeof(hid_report));
    USBD_HID_SendReport(&hUsbDeviceFS, hid_report, sizeof(hid_report));
}


/*============================================================================*/
/**
  * @brief  Type a single ASCII character via HID keyboard
  */
/*============================================================================*/
void badusb_type_char(char c)
{
    if (c < 0x20 || c > 0x7E)
    {
        if (c == '\n' || c == '\r')
        {
            badusb_send_key(0, BUSB_KEY_ENTER);
        }
        else if (c == '\t')
        {
            badusb_send_key(0, BUSB_KEY_TAB);
        }
        return;
    }

    const busb_ascii_hid_map_t *entry = busb_ascii_to_hid((unsigned char)c);
    if (!entry) return;
    uint8_t mod = entry->shift ? BUSB_MOD_LSHIFT : 0;
    badusb_send_key(mod, entry->keycode);
}


/*============================================================================*/
/**
  * @brief  Type a string character by character
  */
/*============================================================================*/
void badusb_type_string(const char *str)
{
    while (*str && badusb_state.running)
    {
        badusb_type_char(*str++);
        osDelay(BADUSB_INTER_CHAR_MS);
    }
}

/**
 * @brief  Type a string directly from an app.
 * Switches to HID, waits for enumeration, types string, and switches back to normal.
 */
void badusb_type_string_forced(const char *str)
{
    if (str == NULL || *str == '\0') return;

    /* If not already in HID mode, switch now */
    if (m1_usb_get_current_mode() != M1_USB_MODE_HID) {
        m1_usb_switch_to_hid();
        /* Wait for enumeration (same as badusb_execute_file) */
        uint32_t t0 = osKernelGetTickCount();
        while (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
        {
            if ((osKernelGetTickCount() - t0) > BADUSB_ENUM_TIMEOUT_MS) break;
            osDelay(BADUSB_ENUM_POLL_MS);
        }
        /* Extra settle delay for OS to load HID drivers */
        osDelay(BADUSB_HID_SETTLE_MS);
    }

    /* Set running flag for this direct call */
    badusb_state.running = 1;

    while (*str && badusb_state.running)
    {
        badusb_type_char(*str++);
        osDelay(BADUSB_INTER_CHAR_MS);
    }

    badusb_state.running = 0;
}


/*============================================================================*/
/**
  * @brief  Parse and execute a single DuckyScript line
  * @retval true if line was valid, false on error
  */
/*============================================================================*/
static bool badusb_parse_line(const char *line)
{
    busb_parsed_line_t parsed;
    busb_classify_line(line, &parsed);

    switch (parsed.type)
    {
    case BUSB_LINE_EMPTY:
    case BUSB_LINE_COMMENT:
        return true;

    case BUSB_LINE_DELAY:
        if (parsed.u.delay_ms > 0)
            osDelay(parsed.u.delay_ms);
        return true;

    case BUSB_LINE_DEFAULT_DELAY:
        badusb_state.default_delay_ms = (uint16_t)parsed.u.delay_ms;
        return true;

    case BUSB_LINE_STRING:
        badusb_type_string(parsed.u.string_text);
        return true;

    case BUSB_LINE_REPEAT:
        if (parsed.u.repeat_count > 0 && badusb_state.last_line[0] != '\0')
        {
            for (int i = 0; i < parsed.u.repeat_count && badusb_state.running; i++)
            {
                badusb_parse_line(badusb_state.last_line);
                if (badusb_state.default_delay_ms > 0)
                    osDelay(badusb_state.default_delay_ms);
            }
        }
        return true;  /* Don't update last_line for REPEAT */

    case BUSB_LINE_MODIFIER_KEY:
    case BUSB_LINE_STANDALONE_KEY:
        badusb_send_key(parsed.u.key.modifiers, parsed.u.key.keycode);
        return true;

    default:
        M1_LOG_W(M1_LOGDB_TAG, "Unknown cmd: %s\r\n", line);
        return true;  /* Skip unrecognized lines rather than aborting */
    }
}


/*============================================================================*/
/**
  * @brief  Show execution progress on LCD
  */
/*============================================================================*/
static void badusb_show_progress(const char *filename)
{
    char buf[32];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);

    m1_draw_text(&m1_u8g2, 2, 10, 124, "BadUSB", TEXT_ALIGN_CENTER);

    /* Show filename (truncated) */
    char name_buf[24];
    strncpy(name_buf, filename, sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    m1_draw_text(&m1_u8g2, 2, 24, 124, name_buf, TEXT_ALIGN_CENTER);

    /* Progress */
    snprintf(buf, sizeof(buf), "Line %d / %d",
             badusb_state.current_line, badusb_state.total_lines);
    m1_draw_text(&m1_u8g2, 2, 38, 124, buf, TEXT_ALIGN_CENTER);

    if (badusb_state.running)
        m1_draw_text(&m1_u8g2, 2, 52, 124, "Running...", TEXT_ALIGN_CENTER);
    else
        m1_draw_text(&m1_u8g2, 2, 52, 124, "Done", TEXT_ALIGN_CENTER);

    m1_draw_bottom_bar(&m1_u8g2, arrowleft_8x8, "Stop", "OK", arrowright_8x8);

    m1_u8g2_nextpage();
}


/*============================================================================*/
/**
  * @brief  Check if user pressed BACK to abort
  * @retval true if abort requested
  */
/*============================================================================*/
static bool badusb_check_abort(void)
{
    S_M1_Main_Q_t q_item;
    S_M1_Buttons_Status btn;

    /* Non-blocking check */
    if (xQueueReceive(main_q_hdl, &q_item, 0) == pdTRUE)
    {
        if (q_item.q_evt_type == Q_EVENT_KEYPAD)
        {
            if (xQueueReceive(button_events_q_hdl, &btn, 0) == pdTRUE)
            {
                if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK
                 || btn.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK
                 || btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK
                 || btn.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
                {
                    return true;
                }
            }
        }
    }
    return false;
}


/*============================================================================*/
/**
  * @brief  Execute a BadUSB script file
  * @param  filepath: full path to script on SD card (e.g., "0:/BadUSB/test.txt")
  * @retval true on success, false on error
  */
/*============================================================================*/
bool badusb_execute_file(const char *filepath)
{
    FIL fp;
    FRESULT fres;
    UINT bytes_read;
    char script_buf[BADUSB_MAX_SCRIPT_SIZE];
    char line_buf[BADUSB_MAX_LINE_LEN];

    /* Phase 1: File read */
    badusb_breadcrumb(1);
    fres = f_open(&fp, filepath, FA_READ);
    if (fres != FR_OK)
    {
        M1_LOG_E(M1_LOGDB_TAG, "Failed to open: %s (err=%d)\r\n", filepath, fres);
        return false;
    }

    /* Read entire file */
    fres = f_read(&fp, script_buf, BADUSB_MAX_SCRIPT_SIZE - 1, &bytes_read);
    f_close(&fp);

    if (fres != FR_OK || bytes_read == 0)
    {
        M1_LOG_E(M1_LOGDB_TAG, "Read failed (err=%d, bytes=%lu)\r\n", fres, (unsigned long)bytes_read);
        return false;
    }
    script_buf[bytes_read] = '\0';

    /* Init state */
    memset(&badusb_state, 0, sizeof(badusb_state));
    badusb_state.running = 1;
    badusb_state.total_lines = busb_count_lines(script_buf, bytes_read);

    /* Extract just the filename for display */
    const char *fname = strrchr(filepath, '/');
    if (fname) fname++; else fname = filepath;

    /* Reset HID debug counters before switching */
    USBD_HID_ResetDbgCounters();

    /* Phase 2: USB switch to HID */
    badusb_breadcrumb(2);
    m1_usb_switch_to_hid();

    /* Phase 3: Wait for enumeration */
    badusb_breadcrumb(3);
    {
        uint32_t t0 = osKernelGetTickCount();
        while (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
        {
            if ((osKernelGetTickCount() - t0) > BADUSB_ENUM_TIMEOUT_MS)
                break;
            osDelay(BADUSB_ENUM_POLL_MS);
        }

        if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
        {
            badusb_state.running = 0;
        }

        /* Phase 4: Settle delay (3s) */
        badusb_breadcrumb(4);
        osDelay(BADUSB_HID_SETTLE_MS);

        /* Phase 5: Prime endpoint */
        badusb_breadcrumb(5);
        if (badusb_state.running)
        {
            memset(hid_report, 0, sizeof(hid_report));
            USBD_HID_SendReport(&hUsbDeviceFS, hid_report, sizeof(hid_report));
            osDelay(50);
        }
    }

    /* Phase 6: Execute script lines */
    badusb_breadcrumb(6);

    if (badusb_state.running)
    {
        const char *p = script_buf;
        const char *end = script_buf + bytes_read;

        while (p < end && badusb_state.running)
        {
            /* Extract one line */
            const char *line_end = p;
            while (line_end < end && *line_end != '\n')
                line_end++;

            size_t line_len = (size_t)(line_end - p);
            if (line_len >= BADUSB_MAX_LINE_LEN)
                line_len = BADUSB_MAX_LINE_LEN - 1;

            memcpy(line_buf, p, line_len);
            line_buf[line_len] = '\0';

            /* Trim trailing CR */
            if (line_len > 0 && line_buf[line_len - 1] == '\r')
                line_buf[line_len - 1] = '\0';

            badusb_state.current_line++;

            /* Execute the line */
            badusb_parse_line(line_buf);

            /* Save for REPEAT (unless it was a REPEAT command itself) */
            if (strncmp(line_buf, "REPEAT ", 7) != 0)
            {
                strncpy(badusb_state.last_line, line_buf,
                        sizeof(badusb_state.last_line) - 1);
                badusb_state.last_line[sizeof(badusb_state.last_line) - 1] = '\0';
            }

            /* Default delay between commands */
            if (badusb_state.default_delay_ms > 0)
                osDelay(badusb_state.default_delay_ms);

            /* Update progress display every 4 lines */
            if ((badusb_state.current_line & 3) == 0)
                badusb_show_progress(fname);

            /* Check for abort */
            if (badusb_check_abort())
            {
                badusb_state.running = 0;
                break;
            }

            /* Advance past newline */
            p = line_end;
            if (p < end && *p == '\n')
                p++;
        }
    }

    /* Ensure all keys released */
    badusb_release_all();

    /* Show final progress */
    badusb_state.running = 0;
    badusb_show_progress(fname);
    osDelay(500);

    /* Switch USB back to CDC+MSC */
    m1_usb_switch_to_normal();

    return true;
}


/*============================================================================*/
/**
  * @brief  Stop a running BadUSB script
  */
/*============================================================================*/
void badusb_stop(void)
{
    badusb_state.running = 0;
}


/*============================================================================*/
/**
  * @brief  BadUSB menu entry point — browse files, select, execute
  */
/*============================================================================*/
void badusb_run(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    S_M1_file_info *f_info;
    BaseType_t ret;
    char filepath[128];

    /* Check SD card */
    if (m1_sdcard_get_status() != SD_access_OK)
    {
        m1_message_box(&m1_u8g2, "BadUSB", "SD card not", "available", " OK ");
        return;
    }

    /* Ensure BadUSB directory exists */
    if (!m1_fb_check_existence(BADUSB_DIR))
    {
        m1_fb_make_dir(BADUSB_DIR);
    }

    /* Init file browser */
    m1_fb_init(&m1_u8g2);

    /* Setup display */
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    m1_u8g2_nextpage();

    m1_fb_display(NULL);

    /* File browser event loop */
    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE)
            continue;

        if (q_item.q_evt_type != Q_EVENT_KEYPAD)
            continue;

        ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
        if (ret != pdTRUE)
            continue;

        /* BACK exits to menu */
        if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            m1_fb_deinit();
            break;
        }

        /* Pass button events to file browser */
        f_info = m1_fb_display(&this_button_status);
        if (f_info->status == FB_OK && f_info->file_is_selected)
        {
            /* Build full file path */
            snprintf(filepath, sizeof(filepath), "%s/%s",
                     f_info->dir_name, f_info->file_name);

            /* Show confirmation */
            char msg_line[28];
            strncpy(msg_line, f_info->file_name, sizeof(msg_line) - 1);
            msg_line[sizeof(msg_line) - 1] = '\0';

            /* Confirmation screen */
            m1_u8g2_firstpage();
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
            m1_draw_text(&m1_u8g2, 2, 10, 124, "Run BadUSB?", TEXT_ALIGN_CENTER);
            m1_draw_text(&m1_u8g2, 2, 28, 124, msg_line, TEXT_ALIGN_CENTER);
            m1_draw_bottom_bar(&m1_u8g2, arrowleft_8x8, NULL, "Run", arrowright_8x8);
            m1_u8g2_nextpage();

            /* Wait for OK or BACK */
            bool confirmed = false;
            while (1)
            {
                ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
                if (ret != pdTRUE) continue;
                if (q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

                ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
                if (ret != pdTRUE) continue;

                if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK ||
                    this_button_status.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
                {
                    confirmed = true;
                    break;
                }
                if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK ||
                    this_button_status.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
                {
                    break;
                }
            }

            if (confirmed)
            {
                /* Execute the script */
                bool ok = badusb_execute_file(filepath);

                /* Show result */
                if (ok)
                {
                    m1_message_box(&m1_u8g2, "BadUSB", "Script", "complete", " OK ");
                }
                else
                {
                    m1_message_box(&m1_u8g2, "BadUSB", "Script", "error", " OK ");
                }
            }

            /* Return to file browser */
            m1_fb_display(NULL);
        }
    }
}

#endif /* M1_APP_BADUSB_ENABLE */
