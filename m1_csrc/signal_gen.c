/* See COPYING.txt for license details. */

/*
 * signal_gen.c
 *
 * Configurable square-wave signal generator for M1.
 * Uses the buzzer timer (TIM8/TIM3 → SPK_CTRL pin) to output a continuous
 * 50 % duty-cycle square wave at a selectable frequency.
 *
 * Inspired by the signal_generator FAP from:
 *   https://github.com/flipperdevices/flipperzero-good-faps
 *
 * M1 Project
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "stm32h5xx_hal.h"
#include "main.h"
#include "cmsis_os.h"

#include "signal_gen.h"
#include "m1_buzzer.h"
#include "m1_lcd.h"
#include "m1_display.h"
#include "m1_tasks.h"
#include "m1_lib.h"

/*************************** D E F I N E S ************************************/

/* Frequency preset table (Hz).  Lower bound ~130 Hz is enforced by the
 * 16-bit TIM prescaler (PCLK2=75 MHz, factor=10: min = 75e6/(65535*10) ≈ 114 Hz). */
static const uint16_t sg_freqs[] = {
    200,  250,  300,  400,  500,  600,  700,  800, 1000, 1200,
   1500, 2000, 2500, 3000, 4000, 5000, 6000, 8000
};
#define SG_FREQ_COUNT   ((uint8_t)(sizeof(sg_freqs) / sizeof(sg_freqs[0])))

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/*============================================================================*/
/**
 * @brief  Signal Generator entry point.
 *
 * Controls:
 *   UP / DOWN  — change frequency preset (wraps around)
 *   OK         — toggle output ON / OFF
 *   BACK       — stop output and return to menu
 */
/*============================================================================*/
void signal_gen_run(void)
{
    S_M1_Buttons_Status bs;
    S_M1_Main_Q_t       q_item;
    BaseType_t          ret;
    bool                running    = true;
    bool                output_on  = false;
    uint8_t             freq_idx   = 8;         /* default: 1000 Hz */
    bool                need_redraw = true;
    char                freq_str[20];
    char                info_str[32];

    while (running)
    {
        if (need_redraw)
        {
            need_redraw = false;

            /* Format frequency string */
            uint16_t f = sg_freqs[freq_idx];
            if (f >= 1000)
                snprintf(freq_str, sizeof(freq_str), "%u.%03u kHz",
                         f / 1000u, f % 1000u);
            else
                snprintf(freq_str, sizeof(freq_str), "%u Hz", f);

            snprintf(info_str, sizeof(info_str), "Output: %s",
                     output_on ? "ON" : "OFF");

            u8g2_FirstPage(&m1_u8g2);
            do {
                u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
                u8g2_DrawStr(&m1_u8g2, 2, 10, "Signal Generator");

                /* Frequency (large) */
                u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
                u8g2_DrawStr(&m1_u8g2, 2, 28, freq_str);

                /* Output state indicator box */
                if (output_on)
                {
                    u8g2_DrawBox(&m1_u8g2, 2, 32, 124, 12);
                    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
                    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
                    u8g2_DrawStr(&m1_u8g2, 4, 42, info_str);
                    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
                }
                else
                {
                    u8g2_DrawFrame(&m1_u8g2, 2, 32, 124, 12);
                    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
                    u8g2_DrawStr(&m1_u8g2, 4, 42, info_str);
                }

                /* Controls help */
                u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
                u8g2_DrawStr(&m1_u8g2, 2, 54, "UP/DN:Freq  OK:Toggle");
                u8g2_DrawStr(&m1_u8g2, 2, 64, "BACK to exit");
            } while (u8g2_NextPage(&m1_u8g2));
        }

        /* Wait for button (100 ms timeout so display stays fresh) */
        ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(100));
        if (ret != pdTRUE)
            continue;
        if (q_item.q_evt_type != Q_EVENT_KEYPAD)
            continue;

        ret = xQueueReceive(button_events_q_hdl, &bs, 0);
        if (ret != pdTRUE)
            continue;

        if (bs.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK ||
            bs.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_LCLICK)
        {
            if (freq_idx < SG_FREQ_COUNT - 1)
                freq_idx++;
            else
                freq_idx = 0;

            if (output_on)
                m1_buzzer_tone_start(sg_freqs[freq_idx]);
            need_redraw = true;
        }
        else if (bs.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK ||
                 bs.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_LCLICK)
        {
            if (freq_idx > 0)
                freq_idx--;
            else
                freq_idx = SG_FREQ_COUNT - 1;

            if (output_on)
                m1_buzzer_tone_start(sg_freqs[freq_idx]);
            need_redraw = true;
        }
        else if (bs.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            output_on = !output_on;
            if (output_on)
                m1_buzzer_tone_start(sg_freqs[freq_idx]);
            else
                m1_buzzer_tone_stop();
            need_redraw = true;
        }
        else if (bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            running = false;
        }
    }

    /* Ensure output is stopped on exit */
    m1_buzzer_tone_stop();

    xQueueReset(main_q_hdl);
    m1_app_send_q_message(main_q_hdl, Q_EVENT_MENU_EXIT);
}
