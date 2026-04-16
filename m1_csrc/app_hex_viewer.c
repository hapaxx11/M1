/* See COPYING.txt for license details. */

/**
 * @file   app_hex_viewer.c
 * @brief  Hex viewer utility — SD card hex/ASCII file viewer.
 *
 * User picks a file via storage_browse(), then views a hex dump with ASCII
 * preview.  Shows 4 rows × 6 bytes (24 bytes/page).  UP/DOWN scrolls by
 * row, LEFT/RIGHT scrolls by page, OK opens file browser, BACK exits.
 *
 * Ported from dagnazty/M1_T-1000 and adapted for Hapax:
 *   - Uses game_poll_button() blocking event loop (blocking delegate pattern)
 *   - Pure formatting logic extracted to hex_viewer.c/h for testability
 */

#include "m1_compile_cfg.h"

#ifdef M1_APP_GAMES_ENABLE

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "ff.h"
#include "m1_games.h"
#include "m1_storage.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "hex_viewer.h"

/*************************** D E F I N E S ************************************/

#define HEX_VIEWER_ROW_BYTES    6U
#define HEX_VIEWER_ROW_COUNT    4U
#define HEX_VIEWER_PAGE_BYTES  (HEX_VIEWER_ROW_BYTES * HEX_VIEWER_ROW_COUNT)
#define HEX_VIEWER_POLL_MS    120U

/************************** S T R U C T U R E S *******************************/

typedef struct
{
    FIL file;
    char path[192];
    char name[32];
    uint32_t file_size;
    uint32_t offset;
    bool file_open;
} hex_viewer_state_t;

/***************************** V A R I A B L E S ******************************/

static hex_viewer_state_t g_hex_viewer;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static void hex_viewer_close(hex_viewer_state_t *st);
static bool hex_viewer_pick_file(hex_viewer_state_t *st);
static void hex_viewer_draw(hex_viewer_state_t *st);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

static void hex_viewer_close(hex_viewer_state_t *st)
{
    if (st != NULL && st->file_open)
    {
        f_close(&st->file);
        st->file_open = false;
    }
}

static bool hex_viewer_pick_file(hex_viewer_state_t *st)
{
    S_M1_file_info *f_info;
    FRESULT res;

    if (st == NULL)
    {
        return false;
    }

    hex_viewer_close(st);
    f_info = storage_browse(NULL);
    if (f_info == NULL || !f_info->file_is_selected)
    {
        return false;
    }

    {
        int n = snprintf(st->path, sizeof(st->path), "%s/%s",
                         f_info->dir_name, f_info->file_name);
        if (n < 0 || (size_t)n >= sizeof(st->path))
        {
            st->file_open = false;
            st->file_size = 0U;
            st->offset = 0U;
            return false;
        }
    }
    strncpy(st->name, f_info->file_name, sizeof(st->name) - 1U);
    st->name[sizeof(st->name) - 1U] = '\0';

    res = f_open(&st->file, st->path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK)
    {
        st->file_open = false;
        st->file_size = 0U;
        st->offset = 0U;
        return false;
    }

    st->file_size = f_size(&st->file);
    st->offset = 0U;
    st->file_open = true;
    return true;
}

static void hex_viewer_draw(hex_viewer_state_t *st)
{
    uint8_t page_buf[HEX_VIEWER_PAGE_BYTES];
    UINT br = 0U;
    char badge[14];
    char row_buf[24];
    char ascii_buf[HEX_VIEWER_PAGE_BYTES + 1U];
    char status_buf[24];
    uint32_t row_offset;
    uint8_t row_y;
    uint16_t row_len;
    bool read_error = false;

    if (st == NULL)
    {
        return;
    }

    memset(page_buf, 0, sizeof(page_buf));
    memset(ascii_buf, 0, sizeof(ascii_buf));

    if (st->file_open)
    {
        if (f_lseek(&st->file, st->offset) != FR_OK ||
            f_read(&st->file, page_buf, sizeof(page_buf), &br) != FR_OK)
        {
            br = 0U;
            read_error = true;
        }
    }

    snprintf(badge, sizeof(badge), "%04lX/%04lX",
             (unsigned long)(st->offset & 0xFFFFUL),
             (unsigned long)(st->file_size & 0xFFFFUL));

    if (read_error)
    {
        strncpy(status_buf, "Read error", sizeof(status_buf) - 1U);
        status_buf[sizeof(status_buf) - 1U] = '\0';
    }
    else if (st->file_open)
    {
        strncpy(status_buf, st->name, sizeof(status_buf) - 1U);
        status_buf[sizeof(status_buf) - 1U] = '\0';
    }
    else
    {
        strncpy(status_buf, "No file selected", sizeof(status_buf) - 1U);
        status_buf[sizeof(status_buf) - 1U] = '\0';
    }

    hex_viewer_ascii_preview(page_buf, (uint16_t)br,
                             ascii_buf, sizeof(ascii_buf));

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title bar: "Hex Viewer" left, offset/size badge right */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 9, "Hex Viewer");
    {
        u8g2_uint_t bw = u8g2_GetStrWidth(&m1_u8g2, badge);
        u8g2_DrawStr(&m1_u8g2, (u8g2_uint_t)(M1_LCD_DISPLAY_WIDTH - 2 - bw), 9, badge);
    }
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    /* Content frame */
    u8g2_DrawFrame(&m1_u8g2, 2, 14, 124, 37);

    /* Filename / status */
    u8g2_DrawStr(&m1_u8g2, 8, 22, status_buf);

    row_y = 30U;
    for (uint8_t row = 0U; row < HEX_VIEWER_ROW_COUNT; row++)
    {
        row_offset = st->offset + ((uint32_t)row * HEX_VIEWER_ROW_BYTES);
        if (row_offset >= st->file_size || row_offset >= (st->offset + br))
        {
            break;
        }

        row_len = (uint16_t)(br - (row * HEX_VIEWER_ROW_BYTES));
        if (row_len > HEX_VIEWER_ROW_BYTES)
        {
            row_len = HEX_VIEWER_ROW_BYTES;
        }

        hex_viewer_format_row(&page_buf[row * HEX_VIEWER_ROW_BYTES],
                              row_len, row_offset,
                              row_buf, sizeof(row_buf));
        u8g2_DrawStr(&m1_u8g2, 8, row_y, row_buf);
        row_y = (uint8_t)(row_y + 8U);
    }

    u8g2_DrawStr(&m1_u8g2, 8, 50, ascii_buf);
    m1_draw_bottom_bar(&m1_u8g2, NULL, NULL, "Browse",
                       arrowright_8x8);
    m1_u8g2_nextpage();
}

void app_hex_viewer_run(void)
{
    game_button_t btn;

    memset(&g_hex_viewer, 0, sizeof(g_hex_viewer));
    if (!hex_viewer_pick_file(&g_hex_viewer))
    {
        return;
    }

    for (;;)
    {
        hex_viewer_draw(&g_hex_viewer);
        btn = game_poll_button(HEX_VIEWER_POLL_MS);

        if (btn == GAME_BTN_BACK)
        {
            hex_viewer_close(&g_hex_viewer);
            return;
        }
        if (btn == GAME_BTN_UP)
        {
            if (g_hex_viewer.offset >= HEX_VIEWER_ROW_BYTES)
            {
                g_hex_viewer.offset -= HEX_VIEWER_ROW_BYTES;
            }
            else
            {
                g_hex_viewer.offset = 0U;
            }
        }
        else if (btn == GAME_BTN_DOWN)
        {
            if ((g_hex_viewer.offset + HEX_VIEWER_ROW_BYTES) <
                g_hex_viewer.file_size)
            {
                g_hex_viewer.offset += HEX_VIEWER_ROW_BYTES;
            }
        }
        else if (btn == GAME_BTN_LEFT)
        {
            if (g_hex_viewer.offset >= HEX_VIEWER_PAGE_BYTES)
            {
                g_hex_viewer.offset -= HEX_VIEWER_PAGE_BYTES;
            }
            else
            {
                g_hex_viewer.offset = 0U;
            }
        }
        else if (btn == GAME_BTN_RIGHT)
        {
            if ((g_hex_viewer.offset + HEX_VIEWER_PAGE_BYTES) <
                g_hex_viewer.file_size)
            {
                g_hex_viewer.offset += HEX_VIEWER_PAGE_BYTES;
            }
        }
        else if (btn == GAME_BTN_OK)
        {
            if (!hex_viewer_pick_file(&g_hex_viewer))
            {
                hex_viewer_close(&g_hex_viewer);
                return;
            }
        }
    }
}

#endif /* M1_APP_GAMES_ENABLE */
