/* See COPYING.txt for license details. */

/**
 * @file  m1_ir_custom.c
 * @brief Custom IR Remote builder — on-device learn, rename, delete.
 *
 * Blocking-delegate scene: called from m1_infrared_scene.c, runs to
 * completion, then returns.
 *
 * Directory layout:
 *   0:/IR/Custom/<remote_name>.ir   — standard Flipper .ir, parsed signals only
 *
 * Navigation:
 *   "Custom Remotes" list  →  per-remote button list  →  Send / Rename / Delete
 *   "[+ New Remote]" entry at top of remote list
 *   "[+ New Button]" entry at top of button list
 *
 * Port of dagnazty/M1_T-1000 Phase 3 (romulofer, commit 08cd5560), adapted to
 * Hapax's blocking-delegate pattern, m1_menu_item_h() font-aware layout, VKB,
 * m1_message_box(), and m1_message_box_choice() confirm API.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "stm32h5xx_hal.h"
#include "main.h"
#include "app_freertos.h"
#include "queue.h"

#include "m1_infrared.h"
#include "m1_ir_universal.h"
#include "m1_ir_custom.h"
#include "flipper_ir.h"
#include "flipper_file.h"
#include "ff.h"

#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_button_bar.h"
#include "m1_scene.h"
#include "m1_lib.h"
#include "m1_tasks.h"
#include "m1_buzzer.h"
#include "m1_led_indicator.h"
#include "m1_virtual_kb.h"
#include "irsnd.h"
#include "irmp.h"

/* ---- Constants ---------------------------------------------------------- */

/* Directory where custom remotes are stored (defined in m1_ir_universal.h) */
#define IR_CUSTOM_EXT            ".ir"

/* Hard limits to keep stack usage bounded */
#define IR_CUSTOM_MAX_REMOTES    16
#define IR_CUSTOM_MAX_BUTTONS    32
#define IR_CUSTOM_NAME_LEN       FLIPPER_IR_NAME_MAX_LEN   /* 32 */
#define IR_CUSTOM_PATH_LEN       80

/* Layout: header bar height + scrollable list items */
#define IR_CUST_HDR_H            11   /* pixels: same as status bar */
#define IR_CUST_ITEM_H           ((uint8_t)m1_menu_item_h())
#define IR_CUST_VISIBLE          4    /* max visible rows at once */

/* ---- List display -------------------------------------------------------- */

static void ir_cust_draw_list(const char   *title,
                               const char    items[][IR_CUSTOM_NAME_LEN],
                               uint16_t      count,
                               uint16_t      sel)
{
    uint16_t start, i;
    uint8_t  item_h = IR_CUST_ITEM_H;
    char     pos[12];

    if (count == 0) return;

    /* Scroll window: keep selection visible */
    start = (sel >= IR_CUST_VISIBLE) ? (uint16_t)(sel - IR_CUST_VISIBLE + 1) : 0;

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 2, 10, title);
    u8g2_DrawHLine(&m1_u8g2, 0, IR_CUST_HDR_H, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, m1_menu_font());

    for (i = 0; i < IR_CUST_VISIBLE && (start + i) < count; i++)
    {
        uint16_t idx = start + i;
        uint8_t  y   = (uint8_t)(IR_CUST_HDR_H + 2 + i * item_h);

        if (idx == sel)
        {
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            u8g2_DrawRBox(&m1_u8g2, 1, y, (u8g2_uint_t)(M1_LCD_DISPLAY_WIDTH - 12),
                          item_h, 2);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }
        else
        {
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }
        u8g2_DrawStr(&m1_u8g2, 4, (u8g2_uint_t)(y + item_h - 1), items[idx]);
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }

    /* Scroll indicator when list overflows the visible area */
    if (count > IR_CUST_VISIBLE)
    {
        uint8_t total_h   = (uint8_t)(IR_CUST_VISIBLE * item_h);
        uint8_t bar_h     = (uint8_t)((IR_CUST_VISIBLE * total_h) / count);
        uint8_t bar_y     = (uint8_t)(IR_CUST_HDR_H + 2 +
                            (start * (total_h - bar_h)) / (count - IR_CUST_VISIBLE));
        if (bar_h < 4) bar_h = 4;
        u8g2_DrawRBox(&m1_u8g2, (u8g2_uint_t)(M1_LCD_DISPLAY_WIDTH - 2),
                      bar_y, 2, bar_h, 1);
    }

    /* Bottom bar: position counter + OK hint */
    snprintf(pos, sizeof(pos), "%u/%u", (unsigned)(sel + 1), (unsigned)count);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_button_bar_draw(arrowleft_8x8, pos,
                       ok_circle_8x8, "OK",
                       NULL, NULL);
    m1_u8g2_nextpage();
}

/* ---- Blocking list navigation ------------------------------------------- */

/**
 * @brief  Event loop for a scrollable list.
 *
 * UP/DOWN scrolls, OK confirms, BACK cancels.
 *
 * @return Selected index, or UINT16_MAX if user pressed BACK.
 */
static uint16_t ir_cust_navigate(const char *title,
                                  const char  items[][IR_CUSTOM_NAME_LEN],
                                  uint16_t    count,
                                  uint16_t    start_sel)
{
    S_M1_Main_Q_t       q;
    S_M1_Buttons_Status bs;
    uint16_t            sel = start_sel;

    if (count == 0)
        return UINT16_MAX;

    ir_cust_draw_list(title, items, count, sel);

    while (1)
    {
        if (xQueueReceive(main_q_hdl, &q, portMAX_DELAY) != pdTRUE)
            continue;
        if (q.q_evt_type != Q_EVENT_KEYPAD)
            continue;
        xQueueReceive(button_events_q_hdl, &bs, 0);

        if (bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            xQueueReset(main_q_hdl);
            return UINT16_MAX;
        }
        else if (bs.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = (sel > 0) ? (uint16_t)(sel - 1) : (uint16_t)(count - 1);
        }
        else if (bs.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = ((uint16_t)(sel + 1) < count) ? (uint16_t)(sel + 1) : 0;
        }
        else if (bs.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            xQueueReset(main_q_hdl);
            return sel;
        }
        else
        {
            continue; /* unhandled key — skip redraw */
        }

        ir_cust_draw_list(title, items, count, sel);
    }
}

/* ---- Directory helpers --------------------------------------------------- */

static uint16_t ir_cust_list_remotes(char names[IR_CUSTOM_MAX_REMOTES][IR_CUSTOM_NAME_LEN])
{
    DIR     dir;
    FILINFO fno;
    FRESULT res;
    uint16_t count    = 0;
    size_t   ext_len  = strlen(IR_CUSTOM_EXT);

    res = f_opendir(&dir, IR_UNIVERSAL_IRDB_ROOT "/Custom");
    if (res != FR_OK)
        return 0;

    while (count < IR_CUSTOM_MAX_REMOTES)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == '\0')
            break;
        if (fno.fattrib & AM_DIR)
            continue;

        size_t flen = strlen(fno.fname);
        if (flen <= ext_len)
            continue;
        const char *ext = fno.fname + flen - ext_len;
        if (ext[0] != '.' || (ext[1] | 0x20) != 'i' || (ext[2] | 0x20) != 'r')
            continue;

        /* Strip the .ir extension for display */
        size_t base_len = flen - ext_len;
        if (base_len >= IR_CUSTOM_NAME_LEN)
            base_len = IR_CUSTOM_NAME_LEN - 1;
        strncpy(names[count], fno.fname, base_len);
        names[count][base_len] = '\0';
        count++;
    }

    f_closedir(&dir);
    return count;
}

static bool ir_cust_full_path(const char *stem, char *out, size_t out_size)
{
    int n = snprintf(out, out_size, "%s/Custom/%s%s",
                     IR_UNIVERSAL_IRDB_ROOT, stem, IR_CUSTOM_EXT);
    return n > 0 && n < (int)out_size;
}

/* ---- IR signal transmit -------------------------------------------------- */

static void ir_cust_send_signal(const flipper_ir_signal_t *sig)
{
    S_M1_Main_Q_t q;
    IRMP_DATA     irmp;
    uint8_t       tx_done = 0;

    if (sig == NULL || !sig->valid || sig->type != FLIPPER_IR_SIGNAL_PARSED)
        return;

    irmp.protocol = sig->parsed.protocol;
    irmp.address  = sig->parsed.address;
    irmp.command  = sig->parsed.command;
    irmp.flags    = 0;

    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);
    infrared_encode_sys_init();
    irsnd_generate_tx_data(irmp);
    infrared_transmit(1);

    uint32_t deadline = HAL_GetTick() + 3000;

    while (!tx_done)
    {
        infrared_transmit(0);

        /* Avoid a permanent wedge if the TX-complete event is never posted. */
        if (HAL_GetTick() >= deadline)
            break;

        if (xQueueReceive(main_q_hdl, &q, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            if (q.q_evt_type == Q_EVENT_IRRED_TX)
            {
                tx_done = 1;
            }
            else if (q.q_evt_type == Q_EVENT_KEYPAD)
            {
                S_M1_Buttons_Status bs;
                if (xQueueReceive(button_events_q_hdl, &bs, 0) == pdTRUE &&
                    bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
                    break;
            }
        }
    }

    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);
    infrared_encode_sys_deinit();
    xQueueReset(main_q_hdl);
}

/* ---- Create new remote --------------------------------------------------- */

static bool ir_cust_create_remote(void)
{
    char           base_name[IR_CUSTOM_NAME_LEN];
    char           full_path[IR_CUSTOM_PATH_LEN];
    flipper_file_t ff;

    if (!m1_vkb_get_filename("Remote name:", "", base_name))
        return false;
    if (base_name[0] == '\0')
        return false;

    if (!ir_cust_full_path(base_name, full_path, sizeof(full_path)))
        return false;

    if (!ff_open_write(&ff, full_path))
    {
        m1_message_box(&m1_u8g2, "Create failed",
                       "Could not create file", "", "BACK to return");
        return false;
    }
    bool ok = flipper_ir_write_header(&ff);
    ff_close(&ff);
    return ok;
}

/* ---- Learn and append a new button -------------------------------------- */

static bool ir_cust_learn_button(const char *remote_path)
{
    IRMP_DATA          irmp_data;
    char               button_name[IR_CUSTOM_NAME_LEN];
    flipper_ir_signal_t sig;

    if (!infrared_capture_one_signal(&irmp_data))
        return false; /* user cancelled */

    if (!m1_vkb_get_filename("Button name:", "", button_name))
        return false;
    if (button_name[0] == '\0')
        return false;

    /* Build a parsed signal from the captured data */
    memset(&sig, 0, sizeof(sig));
    strncpy(sig.name, button_name, sizeof(sig.name) - 1);
    sig.type             = FLIPPER_IR_SIGNAL_PARSED;
    sig.parsed.protocol  = irmp_data.protocol;
    sig.parsed.address   = irmp_data.address;
    sig.parsed.command   = irmp_data.command;
    sig.parsed.flags     = 0;
    sig.valid            = true;

    if (!flipper_ir_append_signal(remote_path, &sig))
    {
        m1_message_box(&m1_u8g2, "Save failed",
                       "Could not save button", "", "BACK to return");
        return false;
    }
    return true;
}

/* ---- Button action menu (Send / Rename / Delete) ------------------------ */

#define IR_CUST_ACTION_SEND    0
#define IR_CUST_ACTION_RENAME  1
#define IR_CUST_ACTION_DELETE  2
#define IR_CUST_ACTION_COUNT   3

static void ir_cust_button_action(const char *remote_path,
                                   uint16_t    button_idx,
                                   const char *button_name)
{
    /* Static so the 2-D array is not on the local stack frame */
    static const char action_labels[IR_CUST_ACTION_COUNT][IR_CUSTOM_NAME_LEN] = {
        "Send", "Rename", "Delete"
    };

    uint16_t sel = ir_cust_navigate("Button",
                                     action_labels, IR_CUST_ACTION_COUNT, 0);
    if (sel == UINT16_MAX)
        return;

    if (sel == IR_CUST_ACTION_SEND)
    {
        /* Re-read the signal at button_idx and send it */
        flipper_file_t      ff;
        flipper_ir_signal_t sig;
        uint16_t            n = 0;

        if (!flipper_ir_open(&ff, remote_path))
            return;

        while (flipper_ir_read_signal(&ff, &sig))
        {
            if (n == button_idx)
            {
                ff_close(&ff);
                ir_cust_send_signal(&sig);
                return;
            }
            n++;
        }
        ff_close(&ff);
    }
    else if (sel == IR_CUST_ACTION_RENAME)
    {
        char new_name[IR_CUSTOM_NAME_LEN];
        if (!m1_vkb_get_filename("Rename to:", button_name, new_name))
            return;
        if (new_name[0] == '\0')
            return;

        if (!flipper_ir_rename_signal(remote_path, button_idx, new_name))
            m1_message_box(&m1_u8g2, "Rename failed",
                           "Could not rename button", "", "BACK to return");
    }
    else  /* IR_CUST_ACTION_DELETE */
    {
        uint8_t confirm = m1_message_box_choice(&m1_u8g2,
                              "Delete button?", button_name, "",
                              "OK  /  Cancel");
        if (confirm == 1)
        {
            if (!flipper_ir_delete_signal(remote_path, button_idx))
                m1_message_box(&m1_u8g2, "Delete failed",
                               "Could not delete button", "", "BACK to return");
        }
    }
}

/* ---- Button list for one remote ----------------------------------------- */

static void ir_cust_show_buttons(const char *remote_path)
{
    char     btn_names[IR_CUSTOM_MAX_BUTTONS + 1][IR_CUSTOM_NAME_LEN];
    uint16_t btn_count;
    uint16_t sel;

    /* Slot 0 is always the "New Button" action */
    strncpy(btn_names[0], "[+ New Button]", IR_CUSTOM_NAME_LEN - 1);
    btn_names[0][IR_CUSTOM_NAME_LEN - 1] = '\0';

    while (1)
    {
        /* Reload button list each time (signals may have been renamed/deleted) */
        btn_count = 1;
        {
            flipper_file_t      ff;
            flipper_ir_signal_t sig;

            if (flipper_ir_open(&ff, remote_path))
            {
                while (btn_count <= IR_CUSTOM_MAX_BUTTONS &&
                       flipper_ir_read_signal(&ff, &sig))
                {
                    strncpy(btn_names[btn_count], sig.name, IR_CUSTOM_NAME_LEN - 1);
                    btn_names[btn_count][IR_CUSTOM_NAME_LEN - 1] = '\0';
                    btn_count++;
                }
                ff_close(&ff);
            }
        }

        /* Build a short title from the remote file's basename (strip .ir) */
        char title[IR_CUSTOM_NAME_LEN + 4];
        {
            const char *base = strrchr(remote_path, '/');
            base = base ? base + 1 : remote_path;
            strncpy(title, base, sizeof(title) - 1);
            title[sizeof(title) - 1] = '\0';
            char *dot = strrchr(title, '.');
            if (dot) *dot = '\0';
        }

        sel = ir_cust_navigate(title, btn_names, btn_count, 0);
        if (sel == UINT16_MAX)
            return;

        if (sel == 0)
        {
            ir_cust_learn_button(remote_path);
        }
        else
        {
            /* sel 1..N → button index sel-1 */
            ir_cust_button_action(remote_path, (uint16_t)(sel - 1), btn_names[sel]);
        }
    }
}

/* ============================================================================
 * Entry point
 * ========================================================================== */

void infrared_custom_remotes(void)
{
    char     remote_names[IR_CUSTOM_MAX_REMOTES][IR_CUSTOM_NAME_LEN];
    char     all_items[IR_CUSTOM_MAX_REMOTES + 1][IR_CUSTOM_NAME_LEN];
    uint16_t count;
    uint16_t sel = 0;

    /* Ensure Custom directory exists */
    f_mkdir(IR_UNIVERSAL_IRDB_ROOT);
    f_mkdir(IR_UNIVERSAL_IRDB_ROOT "/Custom");

    while (1)
    {
        /* Reload on every iteration so creates/deletes are reflected */
        count = ir_cust_list_remotes(remote_names);

        /* all_items[0] = "[+ New Remote]", [1..count] = remote names */
        strncpy(all_items[0], "[+ New Remote]", IR_CUSTOM_NAME_LEN - 1);
        all_items[0][IR_CUSTOM_NAME_LEN - 1] = '\0';
        for (uint16_t i = 0; i < count; i++)
        {
            strncpy(all_items[i + 1], remote_names[i], IR_CUSTOM_NAME_LEN - 1);
            all_items[i + 1][IR_CUSTOM_NAME_LEN - 1] = '\0';
        }

        sel = ir_cust_navigate("Custom Remotes", all_items, (uint16_t)(count + 1), 0);
        if (sel == UINT16_MAX)
            return;

        if (sel == 0)
        {
            ir_cust_create_remote();
        }
        else
        {
            char full_path[IR_CUSTOM_PATH_LEN];
            if (ir_cust_full_path(remote_names[sel - 1], full_path, sizeof(full_path)))
                ir_cust_show_buttons(full_path);
        }
    }
}
