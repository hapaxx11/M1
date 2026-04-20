/* See COPYING.txt for license details. */

/**
 * @file   m1_clock.c
 * @brief  World Clock utility — standalone clock display with timezone pages.
 *
 * Displays local time + 4 world time zones (UTC, UTC+1, UTC+5, UTC+9).
 * Pages through zones with LEFT/RIGHT buttons.  Uses the game_poll_button()
 * blocking helper so it runs as a blocking delegate inside the Games scene.
 *
 * Ported from dagnazty/M1_T-1000 app_clock.c and adapted:
 *   - Pure logic extracted to m1_clock_util.c/h
 *   - Drawing adapted to Hapax display helpers
 *   - "Back" label removed per UX rules
 */

#include "m1_compile_cfg.h"

#ifdef M1_APP_GAMES_ENABLE

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "m1_clock_util.h"
#include "m1_games.h"
#include "m1_system.h"
#include "m1_display.h"
#include "m1_lcd.h"

/*============================================================================*/
/* Static data                                                                */
/*============================================================================*/

static const char *const clock_weekdays[] = {
    "---",
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};

static const clock_zone_t clock_zones[] = {
    { "UTC",   0 },
    { "UTC+1", 1 },
    { "UTC+5", 5 },
    { "UTC+9", 9 },
};

/*============================================================================*/
/* Constants (derived from data above — do not hard-code)                     */
/*============================================================================*/

/* Number of world-zone pages; add/remove entries in clock_zones[] above. */
#define CLOCK_ZONE_COUNT  (sizeof(clock_zones) / sizeof(clock_zones[0]))
/* Total pages: 1 local + one per zone. */
#define CLOCK_PAGE_COUNT  (1U + CLOCK_ZONE_COUNT)
#define CLOCK_POLL_MS     200U

/*============================================================================*/
/* Drawing                                                                    */
/*============================================================================*/

static void clock_draw_page(uint8_t page)
{
    m1_time_t raw;
    clock_time_t now;
    clock_time_t zone_time;
    char badge[8];
    char date_line[24];
    char time_line[12];
    const char *title = "Local";
    const char *weekday;

    /* Read RTC */
    m1_get_localtime(&raw);

    /* Copy into clock_time_t (layout-compatible) */
    now.year    = raw.year;
    now.month   = raw.month;
    now.day     = raw.day;
    now.hour    = raw.hour;
    now.minute  = raw.minute;
    now.second  = raw.second;
    now.weekday = raw.weekday;

    weekday = (now.weekday <= 7U) ? clock_weekdays[now.weekday]
                                  : clock_weekdays[0];

    snprintf(badge, sizeof(badge), "%u/%u",
             (unsigned)(page + 1U), (unsigned)CLOCK_PAGE_COUNT);

    if (page == 0U)
    {
        /* Local time — show user's configured timezone label */
        zone_time = now;
        snprintf(date_line, sizeof(date_line), "%s %02u/%02u/%04u",
                 weekday, zone_time.month, zone_time.day, zone_time.year);

        /* Build "Local (UTC+N)" title */
        {
            char tz_buf[8];
            static char local_title[16];
            clock_tz_label(m1_clock_tz_offset, tz_buf, sizeof(tz_buf));
            snprintf(local_title, sizeof(local_title), "Local %s", tz_buf);
            title = local_title;
        }
    }
    else
    {
        /* World zone — first convert local time to UTC, then apply zone offset */
        const clock_zone_t *zone = &clock_zones[page - 1U];
        title = zone->label;
        int16_t rel = (int16_t)zone->offset_hours - (int16_t)m1_clock_tz_offset;
        clock_apply_offset(&now, (int8_t)rel, &zone_time);
        snprintf(date_line, sizeof(date_line), "%02u/%02u/%04u",
                 zone_time.month, zone_time.day, zone_time.year);
    }

    snprintf(time_line, sizeof(time_line), "%02u:%02u:%02u",
             zone_time.hour, zone_time.minute, zone_time.second);

    /* --- Render --- */
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title bar: "Clock" left, page badge right */
    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "Clock");
    {
        u8g2_uint_t bw = u8g2_GetStrWidth(&m1_u8g2, badge);
        u8g2_DrawStr(&m1_u8g2, (u8g2_uint_t)(126 - bw), 10, badge);
    }
    u8g2_DrawHLine(&m1_u8g2, 0, 12, 128);

    /* Content frame */
    u8g2_DrawFrame(&m1_u8g2, 2, 14, 124, 37);

    /* Date line */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 8, 24, 114, date_line, TEXT_ALIGN_LEFT);

    /* Large time digits */
    u8g2_SetFont(&m1_u8g2, M1_DISP_LARGE_FONT_2B);
    m1_draw_text(&m1_u8g2, 10, 42, 108, time_line, TEXT_ALIGN_CENTER);

    /* Zone label */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 8, 50, 114, title, TEXT_ALIGN_LEFT);

    /* Bottom bar: ← Prev  |  Next → */
    m1_draw_bottom_bar(&m1_u8g2, arrowleft_8x8, "Prev", "Next", arrowright_8x8);

    m1_u8g2_nextpage();
}

/*============================================================================*/
/* Entry point (blocking delegate)                                            */
/*============================================================================*/

void app_clock_run(void)
{
    uint8_t page = 0U;
    game_button_t btn;

    for (;;)
    {
        clock_draw_page(page);
        btn = game_poll_button(CLOCK_POLL_MS);

        if (btn == GAME_BTN_BACK)
            return;

        if (btn == GAME_BTN_LEFT || btn == GAME_BTN_UP)
            page = (page == 0U) ? (CLOCK_PAGE_COUNT - 1U) : (uint8_t)(page - 1U);
        else if (btn == GAME_BTN_RIGHT || btn == GAME_BTN_DOWN || btn == GAME_BTN_OK)
            page = (uint8_t)((page + 1U) % CLOCK_PAGE_COUNT);
    }
}

#endif /* M1_APP_GAMES_ENABLE */
