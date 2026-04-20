/* See COPYING.txt for license details. */

/**
 * @file   m1_ir_quick_remote.c
 * @brief  Quick-Access Category Remote — visual button grid for IR control.
 *
 * Provides Momentum-style category quick-remotes (TV, AC, Audio, Projector,
 * Fan, LED) with visual button grids, brute-force power scanning, and
 * last-used device persistence.
 *
 * Hardware-independent: all IR transmission delegates to m1_ir_universal.c
 * and m1_infrared.c via their existing transmit_command() patterns.
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_ir_quick_remote.h"
#include "m1_ir_universal.h"
#include "m1_infrared.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_system.h"
#include "m1_tasks.h"
#include "m1_watchdog.h"
#include "m1_buzzer.h"
#include "m1_scene.h"
#include "m1_led_indicator.h"
#include "m1_file_browser.h"
#include "flipper_file.h"
#include "flipper_ir.h"
#include "irsnd.h"
#include "ff.h"

/*************************** D E F I N E S ************************************/

/** Grid layout: 3 columns × 3 rows in the 128×64 display */
#define GRID_COLS      3
#define GRID_ROWS      3
#define GRID_TOP_Y     13     /* Below title bar (12px + 1px separator) */
#define GRID_AREA_H    42     /* Grid area height (leaving 10px for bottom bar) */
#define GRID_CELL_W    42     /* 128 / 3 ≈ 42 px per column */
#define GRID_CELL_H    14     /* 42 / 3 = 14 px per row */
#define GRID_BOTTOM_Y  (GRID_TOP_Y + GRID_AREA_H)

/** 2-column × 2-row grid for smaller categories (Fan, LED) */
#define GRID2_COLS     2
#define GRID2_ROWS     2
#define GRID2_CELL_W   64
#define GRID2_CELL_H   21

/** Last-used device persistence file */
#define IR_LAST_USED_FILE  "0:/System/ir_last_used.txt"

/** Maximum command name length for matching */
#define CMD_MATCH_LEN  24

/** Maximum path length */
#define QR_PATH_MAX    256

/*************************** S T R U C T U R E S *****************************/

/** Button definition for a grid cell */
typedef struct {
    const char *label;               /**< Display label (max 7 chars) */
    const char *cmd_name;            /**< Primary command name to match */
    const char * const *cmd_alts;    /**< NULL-terminated list of fallback names, or NULL */
} ir_grid_button_t;

/** Category layout definition */
typedef struct {
    const char          *title;          /**< Category title for title bar */
    const char          *irdb_subdir;    /**< IRDB subdirectory (e.g. "TV") */
    const char          *universal_file; /**< Universal brute-force file */
    uint8_t              cols;           /**< Grid columns */
    uint8_t              rows;           /**< Grid rows */
    uint8_t              btn_count;      /**< Number of active buttons */
    const ir_grid_button_t *buttons;     /**< Button definitions */
} ir_category_layout_t;

/*************************** L A Y O U T S ***********************************/

/* --- TV 3×3 Grid --- */
static const ir_grid_button_t s_tv_buttons[IR_TV_BTN_COUNT] = {
    [IR_TV_POWER]  = { "Power",  "Power"    },
    [IR_TV_SOURCE] = { "Source", "Source"   },
    [IR_TV_MUTE]   = { "Mute",   "Mute"     },
    [IR_TV_VOL_UP] = { "Vol+",   "Vol_up"   },
    [IR_TV_OK]     = { "OK",     "OK"       },
    [IR_TV_CH_UP]  = { "Ch+",    "Ch_next"  },
    [IR_TV_VOL_DN] = { "Vol-",   "Vol_dn"   },
    [IR_TV_MENU]   = { "Menu",   "Menu"     },
    [IR_TV_CH_DN]  = { "Ch-",    "Ch_prev"  },
};

/* Fallback candidate names for AC Power (many remotes use "Power" instead of "Off") */
static const char * const s_ac_power_alts[] = { "Power", "Power_on", "On", NULL };

/* --- AC 3×3 Grid --- */
static const ir_grid_button_t s_ac_buttons[IR_AC_BTN_COUNT] = {
    [IR_AC_POWER]   = { "Power",  "Off",       s_ac_power_alts },
    [IR_AC_MODE]    = { "Mode",   "Mode"     },
    [IR_AC_SWING]   = { "Swing",  "Swing"    },
    [IR_AC_TEMP_UP] = { "Temp+",  "Temp_up"  },
    [IR_AC_FAN]     = { "Fan",    "Fan"      },
    [IR_AC_TIMER]   = { "Timer",  "Timer"    },
    [IR_AC_TEMP_DN] = { "Temp-",  "Temp_dn"  },
    [IR_AC_TURBO]   = { "Turbo",  "Turbo"    },
    [IR_AC_SLEEP]   = { "Sleep",  "Sleep"    },
};

/* --- Audio 3×3 Grid --- */
static const ir_grid_button_t s_aud_buttons[IR_AUD_BTN_COUNT] = {
    [IR_AUD_POWER]  = { "Power",  "Power"    },
    [IR_AUD_SOURCE] = { "Source", "Source"   },
    [IR_AUD_MUTE]   = { "Mute",   "Mute"     },
    [IR_AUD_VOL_UP] = { "Vol+",   "Vol_up"   },
    [IR_AUD_PLAY]   = { "Play",   "Play"     },
    [IR_AUD_NEXT]   = { "Next",   "Next"     },
    [IR_AUD_VOL_DN] = { "Vol-",   "Vol_dn"   },
    [IR_AUD_STOP]   = { "Stop",   "Stop"     },
    [IR_AUD_PREV]   = { "Prev",   "Prev"     },
};

/* --- Projector 3×2 Grid --- */
static const ir_grid_button_t s_prj_buttons[IR_PRJ_BTN_COUNT] = {
    [IR_PRJ_POWER]  = { "Power",  "Power"    },
    [IR_PRJ_SOURCE] = { "Source", "Source"   },
    [IR_PRJ_MUTE]   = { "Mute",   "Mute"     },
    [IR_PRJ_VOL_UP] = { "Vol+",   "Vol_up"   },
    [IR_PRJ_OK]     = { "OK",     "OK"       },
    [IR_PRJ_VOL_DN] = { "Vol-",   "Vol_dn"   },
};

/* --- Fan 2×2 Grid --- */
static const ir_grid_button_t s_fan_buttons[IR_FAN_BTN_COUNT] = {
    [IR_FAN_POWER]  = { "Power",  "Power"    },
    [IR_FAN_SPEED]  = { "Speed",  "Speed"    },
    [IR_FAN_OSC]    = { "Osc",    "Oscillate"},
    [IR_FAN_TIMER]  = { "Timer",  "Timer"    },
};

/* --- LED 2×2 Grid --- */
static const ir_grid_button_t s_led_buttons[IR_LED_BTN_COUNT] = {
    [IR_LED_ON]     = { "On",     "On"       },
    [IR_LED_OFF]    = { "Off",    "Off"      },
    [IR_LED_BRIGHT] = { "Brt+",   "Brighter" },
    [IR_LED_DIM]    = { "Brt-",   "Darker"   },
};

/* --- Category layout table --- */
static const ir_category_layout_t s_layouts[IR_CAT_COUNT] = {
    [IR_CAT_TV] = {
        .title          = "TV Remote",
        .irdb_subdir    = "TV",
        .universal_file = "0:/IR/TV/Universal_Power.ir",
        .cols = GRID_COLS, .rows = GRID_ROWS,
        .btn_count = IR_TV_BTN_COUNT,
        .buttons   = s_tv_buttons,
    },
    [IR_CAT_AC] = {
        .title          = "AC Remote",
        .irdb_subdir    = "AC",
        .universal_file = NULL,  /* No Universal_Power.ir for AC yet */
        .cols = GRID_COLS, .rows = GRID_ROWS,
        .btn_count = IR_AC_BTN_COUNT,
        .buttons   = s_ac_buttons,
    },
    [IR_CAT_AUDIO] = {
        .title          = "Audio Remote",
        .irdb_subdir    = "Audio",
        .universal_file = "0:/IR/Audio/Universal_Power.ir",
        .cols = GRID_COLS, .rows = GRID_ROWS,
        .btn_count = IR_AUD_BTN_COUNT,
        .buttons   = s_aud_buttons,
    },
    [IR_CAT_PROJECTOR] = {
        .title          = "Projector",
        .irdb_subdir    = "Projector",
        .universal_file = "0:/IR/Projector/Universal_Projector.ir",
        .cols = GRID_COLS, .rows = 2,
        .btn_count = IR_PRJ_BTN_COUNT,
        .buttons   = s_prj_buttons,
    },
    [IR_CAT_FAN] = {
        .title          = "Fan Remote",
        .irdb_subdir    = "Fan",
        .universal_file = "0:/IR/Fan/Universal_Fan.ir",
        .cols = GRID2_COLS, .rows = GRID2_ROWS,
        .btn_count = IR_FAN_BTN_COUNT,
        .buttons   = s_fan_buttons,
    },
    [IR_CAT_LED] = {
        .title          = "LED Remote",
        .irdb_subdir    = "LED",
        .universal_file = NULL,
        .cols = GRID2_COLS, .rows = GRID2_ROWS,
        .btn_count = IR_LED_BTN_COUNT,
        .buttons   = s_led_buttons,
    },
};

/*************************** S T A T I C  V A R S *****************************/

/** Loaded commands from the selected .ir file */
static ir_universal_cmd_t s_qr_commands[IR_UNIVERSAL_MAX_CMDS];
static uint16_t s_qr_cmd_count;

/** Mapping: grid button index → s_qr_commands[] index.  -1 = not mapped. */
static int8_t s_btn_to_cmd[IR_GRID_MAX_BUTTONS];

/** Current device file path */
static char s_qr_device_path[QR_PATH_MAX];

/** IRMP TX data (reuse structure for transmission) */
static IRMP_DATA s_qr_tx_data;

/** Raw TX buffer and signal storage */
static uint16_t s_qr_raw_ota[FLIPPER_IR_RAW_MAX_SAMPLES];
static flipper_ir_signal_t s_qr_raw_sig;

/********************* P R O T O T Y P E S ***********************************/

static bool load_last_used(ir_category_t cat, char *path, uint16_t path_len);
static void save_last_used(ir_category_t cat, const char *path);
static bool load_device(const char *filepath);
static void map_buttons_to_commands(const ir_category_layout_t *layout);
static void draw_grid(const ir_category_layout_t *layout, uint8_t sel,
                      const char *device_name, bool transmitting);
static void qr_transmit(uint8_t btn_idx);
static void qr_transmit_raw(const ir_universal_cmd_t *cmd);
static bool browse_for_device(ir_category_t cat, char *out_path, uint16_t path_len);

/*============================================================================*/
/*
 * Load last-used device path for a category from SD card.
 * File format: one line per category (index = line number).
 */
/*============================================================================*/
static bool load_last_used(ir_category_t cat, char *path, uint16_t path_len)
{
    FIL file;
    FRESULT res;
    UINT br;
    uint8_t line_idx = 0;
    char ch;
    uint16_t pos = 0;

    res = f_open(&file, IR_LAST_USED_FILE, FA_READ);
    if (res != FR_OK)
        return false;

    /* Read lines until we reach the category's line */
    while (line_idx <= (uint8_t)cat)
    {
        pos = 0;
        while (pos < path_len - 1)
        {
            res = f_read(&file, &ch, 1, &br);
            if (res != FR_OK || br == 0)
            {
                f_close(&file);
                if (line_idx == (uint8_t)cat && pos > 0)
                {
                    path[pos] = '\0';
                    return true;
                }
                return false;
            }
            if (ch == '\r')
            {
                /* Skip CR in CRLF sequences */
                continue;
            }
            if (ch == '\n')
            {
                /* Always count newline as a line boundary, even for empty lines */
                path[pos] = '\0';
                if (line_idx == (uint8_t)cat)
                {
                    f_close(&file);
                    return pos > 0;
                }
                line_idx++;
                break;
            }
            if (line_idx == (uint8_t)cat)
                path[pos] = ch;
            pos++;
        }
    }

    f_close(&file);
    return false;
}

/*============================================================================*/
/*
 * Save last-used device path for a category.
 * Reads existing file, updates the line for this category, writes back.
 */
/*============================================================================*/
static void save_last_used(ir_category_t cat, const char *path)
{
    char lines[IR_CAT_COUNT][QR_PATH_MAX];
    uint8_t i;

    /* Initialize with empty strings */
    for (i = 0; i < IR_CAT_COUNT; i++)
        lines[i][0] = '\0';

    /* Try to read existing file */
    {
        FIL file;
        if (f_open(&file, IR_LAST_USED_FILE, FA_READ) == FR_OK)
        {
            for (i = 0; i < IR_CAT_COUNT; i++)
            {
                uint16_t pos = 0;
                char ch;
                UINT br;

                while (pos < QR_PATH_MAX - 1)
                {
                    if (f_read(&file, &ch, 1, &br) != FR_OK || br == 0)
                        break;
                    if (ch == '\r')
                        continue;  /* skip CR in CRLF */
                    if (ch == '\n')
                        break;  /* always treat \n as line end, even for empty lines */
                    lines[i][pos++] = ch;
                }
                lines[i][pos] = '\0';
            }
            f_close(&file);
        }
    }

    /* Update the target category */
    strncpy(lines[(uint8_t)cat], path, QR_PATH_MAX - 1);
    lines[(uint8_t)cat][QR_PATH_MAX - 1] = '\0';

    /* Write back */
    {
        FIL file;
        UINT bw;
        f_mkdir("0:/System");
        if (f_open(&file, IR_LAST_USED_FILE, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK)
        {
            for (i = 0; i < IR_CAT_COUNT; i++)
            {
                f_write(&file, lines[i], strlen(lines[i]), &bw);
                f_write(&file, "\n", 1, &bw);
            }
            f_close(&file);
        }
    }
}

/*============================================================================*/
/*
 * Load a .ir file into s_qr_commands[].
 */
/*============================================================================*/
static bool load_device(const char *filepath)
{
    flipper_file_t ff;
    uint16_t count = 0;

    memset(s_qr_commands, 0, sizeof(s_qr_commands));
    s_qr_cmd_count = 0;

    if (!ff_open(&ff, filepath))
        return false;

    /* Accept both Flipper IR file types */
    if (!ff_validate_header(&ff, "IR signals file", 1))
    {
        ff_close(&ff);
        if (!ff_open(&ff, filepath))
            return false;
        if (!ff_validate_header(&ff, "IR library file", 1))
        {
            ff_close(&ff);
            return false;
        }
    }

    /* Parse signal blocks */
    while (count < IR_UNIVERSAL_MAX_CMDS)
    {
        ir_universal_cmd_t *cmd = &s_qr_commands[count];
        bool got_name = false;
        bool got_type = false;
        bool is_parsed = false;
        bool is_raw = false;

        memset(cmd, 0, sizeof(*cmd));

        while (ff_read_line(&ff))
        {
            if (ff_is_separator(&ff))
                break;
            if (!ff_parse_kv(&ff))
                continue;

            const char *key = ff_get_key(&ff);
            const char *val = ff_get_value(&ff);

            if (strcmp(key, "name") == 0)
            {
                strncpy(cmd->name, val, IR_UNIVERSAL_NAME_MAX_LEN - 1);
                cmd->name[IR_UNIVERSAL_NAME_MAX_LEN - 1] = '\0';
                got_name = true;
            }
            else if (strcmp(key, "type") == 0)
            {
                got_type = true;
                is_parsed = (strcmp(val, "parsed") == 0);
                is_raw    = (strcmp(val, "raw") == 0);
            }
            else if (strcmp(key, "protocol") == 0 && is_parsed)
            {
                cmd->protocol = flipper_ir_proto_to_irmp(val);
            }
            else if (strcmp(key, "address") == 0 && is_parsed)
            {
                uint8_t hex[4];
                uint8_t n = ff_parse_hex_bytes(val, hex, 4);
                cmd->address = (n >= 2) ? (uint16_t)(hex[0] | ((uint16_t)hex[1] << 8))
                                        : (uint16_t)hex[0];
            }
            else if (strcmp(key, "command") == 0 && is_parsed)
            {
                uint8_t hex[4];
                uint8_t n = ff_parse_hex_bytes(val, hex, 4);
                cmd->command = (n >= 2) ? (uint16_t)(hex[0] | ((uint16_t)hex[1] << 8))
                                        : (uint16_t)hex[0];
            }
            else if (strcmp(key, "frequency") == 0 && is_raw)
            {
                cmd->raw_freq = (uint32_t)strtoul(val, NULL, 10);
            }
            else if (strcmp(key, "data") == 0 && is_raw)
            {
                const char *p = val;
                uint16_t cnt = 0;
                while (*p)
                {
                    while (*p == ' ') p++;
                    if (*p == '\0') break;
                    cnt++;
                    while (*p && *p != ' ') p++;
                }
                cmd->raw_count = cnt;
            }
        }

        if (got_name && got_type)
        {
            if (is_parsed && cmd->protocol != 0)
            {
                cmd->is_raw = false;
                cmd->flags = 0;
                cmd->valid = true;
                count++;
            }
            else if (is_raw && cmd->raw_freq > 0)
            {
                cmd->is_raw = true;
                cmd->valid = true;
                count++;
            }
        }

        if (ff.eof)
            break;
    }

    ff_close(&ff);
    s_qr_cmd_count = count;
    return count > 0;
}

/*
 * Case-insensitive substring match helper.
 * Returns true if `needle` appears anywhere within `haystack`.
 */
static bool ci_substr(const char *haystack, const char *needle)
{
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    size_t k;
    size_t j;

    if (nlen == 0)
        return true;
    if (nlen > hlen)
        return false;

    for (k = 0; k <= hlen - nlen; k++)
    {
        bool match = true;
        for (j = 0; j < nlen; j++)
        {
            char ch = haystack[k + j];
            char cn = needle[j];
            if (ch >= 'A' && ch <= 'Z') ch += ('a' - 'A');
            if (cn >= 'A' && cn <= 'Z') cn += ('a' - 'A');
            if (ch != cn) { match = false; break; }
        }
        if (match)
            return true;
    }
    return false;
}

/*============================================================================*/
/*
 * Try to map one button slot to a command index using exact then
 * case-insensitive substring match.  Returns true if matched.
 */
/*============================================================================*/
static bool try_map_name(uint8_t btn_idx, const char *target)
{
    uint16_t c;

    if (target == NULL)
        return false;

    /* Exact match */
    for (c = 0; c < s_qr_cmd_count; c++)
    {
        if (strcmp(s_qr_commands[c].name, target) == 0)
        {
            s_btn_to_cmd[btn_idx] = (int8_t)c;
            return true;
        }
    }

    /* Case-insensitive substring match — bidirectional so that e.g.
     * target="Vol_up" matches command="TV_Vol_up" (target in name), and
     * target="Power" matches command="Power_on" (name in target prefix sense
     * handled by the reverse direction).  Both are desirable for AC/audio
     * remotes where command names vary across brands. */
    for (c = 0; c < s_qr_cmd_count; c++)
    {
        if (ci_substr(s_qr_commands[c].name, target) ||
            ci_substr(target, s_qr_commands[c].name))
        {
            s_btn_to_cmd[btn_idx] = (int8_t)c;
            return true;
        }
    }

    return false;
}

/*============================================================================*/
/*
 * Map grid button slots to loaded commands by matching command names.
 * Uses case-insensitive substring matching for flexibility.
 * If the primary cmd_name fails, tries each entry in cmd_alts in order.
 */
/*============================================================================*/
static void map_buttons_to_commands(const ir_category_layout_t *layout)
{
    uint8_t b;

    for (b = 0; b < IR_GRID_MAX_BUTTONS; b++)
        s_btn_to_cmd[b] = -1;

    for (b = 0; b < layout->btn_count; b++)
    {
        const ir_grid_button_t *btn = &layout->buttons[b];

        /* Try primary name */
        if (try_map_name(b, btn->cmd_name))
            continue;

        /* Try fallback alternatives if provided */
        if (btn->cmd_alts != NULL)
        {
            const char * const *alt = btn->cmd_alts;
            while (*alt != NULL)
            {
                if (try_map_name(b, *alt))
                    break;
                alt++;
            }
        }
    }
}

/*============================================================================*/
/*
 * Draw the visual button grid on the 128×64 display.
 */
/*============================================================================*/
static void draw_grid(const ir_category_layout_t *layout, uint8_t sel,
                      const char *device_name, bool transmitting)
{
    uint8_t cols = layout->cols;
    uint8_t rows = layout->rows;
    uint8_t cell_w, cell_h;

    if (cols == 2)
    {
        cell_w = GRID2_CELL_W;
        cell_h = GRID2_CELL_H;
    }
    else
    {
        cell_w = GRID_CELL_W;
        cell_h = GRID_CELL_H;
    }

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title bar: category + truncated device name */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
    {
        char title[32];
        if (device_name && device_name[0])
            snprintf(title, sizeof(title), "%s", device_name);
        else
            snprintf(title, sizeof(title), "%s", layout->title);
        u8g2_DrawStr(&m1_u8g2, 2, 9, title);
    }
    u8g2_DrawHLine(&m1_u8g2, 0, 11, 128);

    /* Draw grid buttons */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

    for (uint8_t b = 0; b < layout->btn_count; b++)
    {
        uint8_t col = b % cols;
        uint8_t row = b / cols;
        uint8_t x = col * cell_w;
        uint8_t y = GRID_TOP_Y + row * cell_h;

        bool is_selected = (b == sel);
        bool is_mapped = (s_btn_to_cmd[b] >= 0);
        bool is_transmitting = transmitting && is_selected;

        if (is_selected)
        {
            /* Draw filled box for selection */
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            u8g2_DrawBox(&m1_u8g2, x, y, cell_w, cell_h);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }
        else
        {
            /* Draw frame for each button cell */
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            u8g2_DrawFrame(&m1_u8g2, x, y, cell_w, cell_h);
        }

        /* Draw label */
        {
            const char *label = layout->buttons[b].label;
            if (is_transmitting)
                label = ">>>";

            /* Center text in the cell */
            u8g2_uint_t tw = u8g2_GetStrWidth(&m1_u8g2, label);
            u8g2_uint_t tx = x + (cell_w - tw) / 2;
            u8g2_uint_t ty = y + cell_h - 3;

            if (!is_mapped && !is_selected)
            {
                /* Dim unmapped buttons by drawing dotted */
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            }

            u8g2_DrawStr(&m1_u8g2, tx, ty, label);
        }

        /* Reset color after selected item */
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }

    /* Bottom bar: navigation hints */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

    /* Show "Scan" hint on left if universal file exists, device name on right */
    {
        const char *left_hint = layout->universal_file ? "< Scan" : "";
        const char *right_hint = "> File";
        u8g2_DrawStr(&m1_u8g2, 1, 63, left_hint);
        u8g2_uint_t rw = u8g2_GetStrWidth(&m1_u8g2, right_hint);
        u8g2_DrawStr(&m1_u8g2, 128 - rw - 1, 63, right_hint);
    }

    m1_u8g2_nextpage();
}

/*============================================================================*/
/*
 * Transmit a parsed IR command from the quick-remote buffer.
 */
/*============================================================================*/
static void qr_transmit(uint8_t btn_idx)
{
    int8_t cmd_idx = s_btn_to_cmd[btn_idx];
    if (cmd_idx < 0 || cmd_idx >= (int8_t)s_qr_cmd_count)
        return;

    const ir_universal_cmd_t *cmd = &s_qr_commands[cmd_idx];
    if (!cmd->valid)
        return;

    if (cmd->is_raw)
    {
        qr_transmit_raw(cmd);
        return;
    }

    /* Parsed signal via IRSND */
    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);

    s_qr_tx_data.protocol = cmd->protocol;
    s_qr_tx_data.address  = cmd->address;
    s_qr_tx_data.command  = cmd->command;
    s_qr_tx_data.flags    = cmd->flags;

    infrared_encode_sys_init();
    irsnd_generate_tx_data(s_qr_tx_data);
    infrared_transmit(1);
    infrared_transmit(0);

    m1_buzzer_notification();
}

/*============================================================================*/
/*
 * Transmit a raw IR command (re-reads file for timing data).
 */
/*============================================================================*/
static void qr_transmit_raw(const ir_universal_cmd_t *cmd)
{
    flipper_file_t ff;
    bool found = false;

    if (!flipper_ir_open(&ff, s_qr_device_path))
        return;

    /* Find the signal by name */
    while (flipper_ir_read_signal(&ff, &s_qr_raw_sig))
    {
        if (strcmp(s_qr_raw_sig.name, cmd->name) == 0 &&
            s_qr_raw_sig.type == FLIPPER_IR_SIGNAL_RAW)
        {
            found = true;
            break;
        }
    }
    ff_close(&ff);

    if (!found || s_qr_raw_sig.raw.sample_count == 0)
        return;

    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);

    /* Convert Flipper raw to OTA format */
    uint16_t ota_len = s_qr_raw_sig.raw.sample_count;
    if (ota_len > FLIPPER_IR_RAW_MAX_SAMPLES)
        ota_len = FLIPPER_IR_RAW_MAX_SAMPLES;

    for (uint16_t i = 0; i < ota_len; i++)
    {
        uint32_t dur = (uint32_t)abs(s_qr_raw_sig.raw.samples[i]);
        if (dur > 65534) dur = 65534;
        if (dur < 2) dur = 2;

        if (i % 2 == 0)
            s_qr_raw_ota[i] = (uint16_t)dur | IR_OTA_PULSE_BIT_MASK;
        else
            s_qr_raw_ota[i] = (uint16_t)dur & IR_OTA_SPACE_BIT_MASK;
    }

    infrared_encode_sys_init();
    irsnd_set_carrier_freq(s_qr_raw_sig.raw.frequency);

    ir_ota_data_tx_active = TRUE;
    ir_ota_data_tx_counter = 0;
    ir_ota_data_tx_len = ota_len;
    pir_ota_data_tx_buffer = s_qr_raw_ota;

    __HAL_TIM_URS_ENABLE(&Timerhdl_IrTx);
    Timerhdl_IrTx.Instance->ARR = s_qr_raw_ota[0];
    HAL_TIM_GenerateEvent(&Timerhdl_IrTx, TIM_EVENTSOURCE_UPDATE);
    __HAL_TIM_URS_DISABLE(&Timerhdl_IrTx);

    if (HAL_IS_BIT_SET(Timerhdl_IrTx.Instance->SR, TIM_FLAG_UPDATE))
        CLEAR_BIT(Timerhdl_IrTx.Instance->SR, TIM_FLAG_UPDATE);

    if (s_qr_raw_ota[0] & 0x0001)
        irsnd_on();

    __HAL_TIM_ENABLE(&Timerhdl_IrTx);

    if (ota_len > 1)
        Timerhdl_IrTx.Instance->ARR = s_qr_raw_ota[++ir_ota_data_tx_counter];

    m1_buzzer_notification();
}

/*============================================================================*/
/*
 * Browse IRDB for a device file (simplified directory browser).
 * Returns true if user selected a file, with path in out_path.
 */
/*============================================================================*/
static bool browse_for_device(ir_category_t cat, char *out_path, uint16_t path_len)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    char browse_path[QR_PATH_MAX];
    char child_path[QR_PATH_MAX];
    char names[16][64];
    uint16_t count;
    uint16_t sel = 0;
    FILINFO fno;
    DIR dir;
    bool need_rescan = true;

    /* Start in the category subdirectory */
    snprintf(browse_path, sizeof(browse_path), "%s/%s",
             IR_UNIVERSAL_IRDB_ROOT, s_layouts[cat].irdb_subdir);

    while (1)
    {
        if (need_rescan)
        {
            /* Scan directory */
            count = 0;
            if (f_opendir(&dir, browse_path) == FR_OK)
            {
                while (count < 16)
                {
                    if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == '\0')
                        break;
                    if (fno.fname[0] == '.')
                        continue;
                    strncpy(names[count], fno.fname, 63);
                    names[count][63] = '\0';
                    count++;
                }
                f_closedir(&dir);
            }

            if (count == 0)
            {
                /* Category directory is empty — fall back to IRDB root
                 * so the user can browse any .ir files on the SD card. */
                if (strcmp(browse_path, IR_UNIVERSAL_IRDB_ROOT) != 0)
                {
                    strncpy(browse_path, IR_UNIVERSAL_IRDB_ROOT,
                            sizeof(browse_path) - 1);
                    browse_path[sizeof(browse_path) - 1] = '\0';
                    /* Show a brief hint before re-scanning */
                    m1_u8g2_firstpage();
                    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
                    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
                    u8g2_DrawStr(&m1_u8g2, 4, 20, "No category files.");
                    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
                    u8g2_DrawStr(&m1_u8g2, 4, 34, "Browsing all IR");
                    u8g2_DrawStr(&m1_u8g2, 4, 46, "files on SD card.");
                    m1_u8g2_nextpage();
                    vTaskDelay(pdMS_TO_TICKS(1200));
                    /* Discard key events entered during the hint so they
                     * cannot be applied immediately by the next browser view. */
                    xQueueReset(main_q_hdl);
                    xQueueReset(button_events_q_hdl);
                    continue; /* re-scan the root */
                }
                /* Still nothing at root — no IR files at all */
                m1_u8g2_firstpage();
                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
                u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
                u8g2_DrawStr(&m1_u8g2, 4, 20, "No IR files found.");
                u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
                u8g2_DrawStr(&m1_u8g2, 4, 34, "Copy .ir files to");
                u8g2_DrawStr(&m1_u8g2, 4, 46, "0:/IR/ on SD card.");
                m1_u8g2_nextpage();
                vTaskDelay(pdMS_TO_TICKS(2000));
                return false;
            }

            sel = 0;
            need_rescan = false;
        }

        /* Draw list */
        {
            const char *title = browse_path;
            const char *p = browse_path;
            while (*p) { if (*p == '/') title = p + 1; p++; }
            if (*title == '\0') title = "Select";

            uint8_t scroll = 0;
            uint8_t vis = M1_MENU_VIS((uint8_t)count);
            if (sel >= vis)
                scroll = (uint8_t)(sel - vis + 1);

            const char *labels[16];
            for (uint16_t i = 0; i < count; i++)
                labels[i] = names[i];

            m1_scene_draw_menu(title, labels, (uint8_t)count, (uint8_t)sel,
                               scroll, vis);
        }

        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE || q_item.q_evt_type != Q_EVENT_KEYPAD)
            continue;

        xQueueReceive(button_events_q_hdl, &btn, 0);

        if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            xQueueReset(main_q_hdl);
            return false;
        }
        else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = (sel > 0) ? sel - 1 : count - 1;
        }
        else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = (sel + 1) % count;
        }
        else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            snprintf(child_path, sizeof(child_path), "%s/%s", browse_path, names[sel]);

            if (f_stat(child_path, &fno) == FR_OK)
            {
                if (fno.fattrib & AM_DIR)
                {
                    /* Navigate into subdirectory */
                    strncpy(browse_path, child_path, sizeof(browse_path) - 1);
                    browse_path[sizeof(browse_path) - 1] = '\0';
                    need_rescan = true;
                }
                else
                {
                    size_t len = strlen(names[sel]);
                    if (len >= 3 && strcmp(&names[sel][len - 3], ".ir") == 0)
                    {
                        strncpy(out_path, child_path, path_len - 1);
                        out_path[path_len - 1] = '\0';
                        xQueueReset(main_q_hdl);
                        return true;
                    }
                }
            }
        }
    }
}

/*============================================================================*/
/*
 * Extract just the filename (without path and extension) from a path.
 */
/*============================================================================*/
static void extract_device_name(const char *path, char *name, uint16_t name_len)
{
    const char *fname = path;
    const char *p = path;

    while (*p)
    {
        if (*p == '/')
            fname = p + 1;
        p++;
    }

    strncpy(name, fname, name_len - 1);
    name[name_len - 1] = '\0';

    /* Strip .ir extension */
    size_t len = strlen(name);
    if (len >= 3 && strcmp(&name[len - 3], ".ir") == 0)
        name[len - 3] = '\0';
}

/*============================================================================*/
/*
 * Main quick-remote entry point.
 * Blocking function — returns when user presses BACK.
 */
/*============================================================================*/
void ir_quick_remote(ir_category_t category)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    uint8_t sel = 0;
    bool device_loaded = false;
    char device_name[32];
    bool transmitting = false;
    const ir_category_layout_t *layout;

    if ((uint8_t)category >= IR_CAT_COUNT)
        return;

    layout = &s_layouts[category];
    device_name[0] = '\0';

    /* Try loading last-used device */
    if (load_last_used(category, s_qr_device_path, QR_PATH_MAX))
    {
        /* Verify the file still exists */
        FILINFO fno;
        if (f_stat(s_qr_device_path, &fno) == FR_OK)
        {
            if (load_device(s_qr_device_path))
            {
                map_buttons_to_commands(layout);
                extract_device_name(s_qr_device_path, device_name, sizeof(device_name));
                device_loaded = true;
            }
        }
    }

    /* If no last-used device, prompt to browse */
    if (!device_loaded)
    {
        char selected_path[QR_PATH_MAX];
        if (browse_for_device(category, selected_path, QR_PATH_MAX))
        {
            strncpy(s_qr_device_path, selected_path, QR_PATH_MAX - 1);
            s_qr_device_path[QR_PATH_MAX - 1] = '\0';
            if (load_device(s_qr_device_path))
            {
                map_buttons_to_commands(layout);
                extract_device_name(s_qr_device_path, device_name, sizeof(device_name));
                save_last_used(category, s_qr_device_path);
                device_loaded = true;
            }
        }
    }

    if (!device_loaded)
        return;  /* User cancelled browse */

    /* Draw initial grid */
    draw_grid(layout, sel, device_name, false);

    /* Main event loop */
    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE)
            continue;

        if (q_item.q_evt_type == Q_EVENT_KEYPAD)
        {
            xQueueReceive(button_events_q_hdl, &btn, 0);

            if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                xQueueReset(main_q_hdl);
                return;
            }
            else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
            {
                if (sel >= layout->cols)
                    sel -= layout->cols;
                else
                {
                    /* Wrap to last row */
                    uint8_t last_row_start = (layout->rows - 1) * layout->cols;
                    uint8_t target = last_row_start + (sel % layout->cols);
                    if (target >= layout->btn_count)
                        target = layout->btn_count - 1;
                    sel = target;
                }
            }
            else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
            {
                if (sel + layout->cols < layout->btn_count)
                    sel += layout->cols;
                else
                {
                    /* Wrap to first row */
                    sel = sel % layout->cols;
                }
            }
            else if (btn.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
            {
                if (layout->universal_file)
                {
                    /* Launch brute-force scan */
                    ir_brute_force_scan(category);
                }
            }
            else if (btn.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
            {
                /* Browse for a different device file */
                char selected_path[QR_PATH_MAX];
                if (browse_for_device(category, selected_path, QR_PATH_MAX))
                {
                    strncpy(s_qr_device_path, selected_path, QR_PATH_MAX - 1);
                    s_qr_device_path[QR_PATH_MAX - 1] = '\0';
                    if (load_device(s_qr_device_path))
                    {
                        map_buttons_to_commands(layout);
                        extract_device_name(s_qr_device_path, device_name, sizeof(device_name));
                        save_last_used(category, s_qr_device_path);
                        device_loaded = true;
                    }
                }
            }
            else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                if (s_btn_to_cmd[sel] >= 0)
                {
                    transmitting = true;
                    draw_grid(layout, sel, device_name, true);
                    qr_transmit(sel);
                    transmitting = false;
                }
            }

            draw_grid(layout, sel, device_name, transmitting);
        }
        else if (q_item.q_evt_type == Q_EVENT_IRRED_TX)
        {
            /* IR TX completed */
            infrared_encode_sys_deinit();
            m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);
            transmitting = false;
            draw_grid(layout, sel, device_name, false);
        }
    }
}

/*============================================================================*/
/*
 * Brute-force power scan: iterate through Universal_Power.ir,
 * transmitting each code.  User presses OK when device responds,
 * or BACK to cancel.
 */
/*============================================================================*/
void ir_brute_force_scan(ir_category_t category)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    const ir_category_layout_t *layout;
    uint16_t i;
    char info[40];

    if ((uint8_t)category >= IR_CAT_COUNT)
        return;

    layout = &s_layouts[category];
    if (layout->universal_file == NULL)
        return;

    /* Load the universal file */
    if (!load_device(layout->universal_file))
    {
        m1_u8g2_firstpage();
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 10, 30, "No universal file");
        m1_u8g2_nextpage();
        vTaskDelay(pdMS_TO_TICKS(1500));
        return;
    }

    /* Iterate through all commands */
    for (i = 0; i < s_qr_cmd_count; i++)
    {
        if (!s_qr_commands[i].valid)
            continue;

        /* Skip raw commands — they cannot be transmitted via the brute-force path */
        if (s_qr_commands[i].is_raw)
            continue;

        /* Draw scan progress screen */
        m1_u8g2_firstpage();
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

        u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
        u8g2_DrawStr(&m1_u8g2, 2, 9, "Power Scan");
        u8g2_DrawHLine(&m1_u8g2, 0, 11, 128);

        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);

        snprintf(info, sizeof(info), "Trying: %s", s_qr_commands[i].name);
        u8g2_DrawStr(&m1_u8g2, 4, 24, info);

        snprintf(info, sizeof(info), "%u / %u", (unsigned)(i + 1), (unsigned)s_qr_cmd_count);
        u8g2_DrawStr(&m1_u8g2, 4, 36, info);

        /* Progress bar */
        {
            uint8_t bar_w = (uint8_t)((uint16_t)120 * (i + 1) / s_qr_cmd_count);
            u8g2_DrawFrame(&m1_u8g2, 4, 40, 120, 8);
            u8g2_DrawBox(&m1_u8g2, 4, 40, bar_w, 8);
        }

        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 4, 58, "OK:Found  Back:Cancel");

        m1_u8g2_nextpage();

        /* Transmit — raw commands are already skipped above */
        {
            const ir_universal_cmd_t *cmd = &s_qr_commands[i];
            s_qr_tx_data.protocol = cmd->protocol;
            s_qr_tx_data.address  = cmd->address;
            s_qr_tx_data.command  = cmd->command;
            s_qr_tx_data.flags    = cmd->flags;

            infrared_encode_sys_init();
            irsnd_generate_tx_data(s_qr_tx_data);
            infrared_transmit(1);
            infrared_transmit(0);
        }

        /* Wait for TX complete or user input */
        {
            uint32_t deadline = HAL_GetTick() + 2000;
            bool tx_done = false;
            bool stopped = false;

            while (!tx_done && !stopped)
            {
                uint32_t now = HAL_GetTick();
                uint32_t remaining = (now < deadline) ? (deadline - now) : 0;
                if (remaining == 0) break;

                ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(remaining));
                if (ret != pdTRUE) break;

                if (q_item.q_evt_type == Q_EVENT_IRRED_TX)
                {
                    tx_done = true;
                }
                else if (q_item.q_evt_type == Q_EVENT_KEYPAD)
                {
                    xQueueReceive(button_events_q_hdl, &btn, 0);
                    if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
                    {
                        /* User found the working code! */
                        infrared_encode_sys_deinit();
                        m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);

                        /* Show success screen */
                        m1_u8g2_firstpage();
                        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
                        u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
                        u8g2_DrawStr(&m1_u8g2, 10, 20, "Found!");
                        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
                        snprintf(info, sizeof(info), "Code: %s", s_qr_commands[i].name);
                        u8g2_DrawStr(&m1_u8g2, 10, 35, info);
                        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
                        u8g2_DrawStr(&m1_u8g2, 10, 50, "Press any key");
                        m1_u8g2_nextpage();

                        /* Wait for dismiss */
                        while (1)
                        {
                            ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
                            if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
                            {
                                xQueueReceive(button_events_q_hdl, &btn, 0);
                                break;
                            }
                        }
                        return;
                    }
                    else if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
                    {
                        stopped = true;
                    }
                }
            }

            infrared_encode_sys_deinit();

            if (stopped)
            {
                m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);
                xQueueReset(main_q_hdl);
                return;
            }

            /* Brief delay between codes */
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    /* Scan complete without user stopping */
    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 10, 20, "Scan Complete");
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    snprintf(info, sizeof(info), "Tried %u codes", (unsigned)s_qr_cmd_count);
    u8g2_DrawStr(&m1_u8g2, 10, 35, info);
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 10, 50, "Press any key");
    m1_u8g2_nextpage();

    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
        {
            xQueueReceive(button_events_q_hdl, &btn, 0);
            break;
        }
    }
    xQueueReset(main_q_hdl);
}
