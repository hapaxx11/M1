/* See COPYING.txt for license details. */

/**
 * @file   m1_gpio_uart.c
 * @brief  USB-UART Bridge — Momentum-style.
 *
 * Blocking delegate for the GPIO scene that turns the M1 into a
 * USB-to-serial adapter.  Leverages the existing VCP task infrastructure
 * (vSer2UsbTask / vUsb2SerTask) for the actual data forwarding and adds
 * an on-screen scrolling terminal for monitoring.
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
#include "m1_gpio_uart.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_tasks.h"
#include "m1_system.h"
#include "m1_log_debug.h"
#include "m1_usb_cdc_msc.h"

/*************************** D E F I N E S ************************************/

#define TERM_COLS           21      /* chars per line at FUNC_MENU_FONT_N   */
#define TERM_ROWS           5       /* visible terminal lines               */
#define BRIDGE_POLL_MS      50      /* 20 Hz UI refresh rate                */

/************************** C O N S T A N T S ********************************/

typedef struct {
    uint32_t    rate;
    const char *label;
} uart_baud_entry_t;

static const uart_baud_entry_t baud_table[] = {
    {   9600, "9600"   },
    {  19200, "19200"  },
    {  38400, "38400"  },
    {  57600, "57600"  },
    { 115200, "115200" },
    { 230400, "230400" },
    { 460800, "460800" },
    { 921600, "921600" },
};

#define BAUD_COUNT  ((uint8_t)(sizeof(baud_table) / sizeof(baud_table[0])))

/***************************** V A R I A B L E S ******************************/

/* Terminal display buffer */
static char     term_buf[TERM_ROWS][TERM_COLS + 1];
static uint8_t  term_row;
static uint8_t  term_col;

/* Byte counter */
static uint32_t rx_count;

/* Persistent configuration (survives across bridge invocations) */
static uint8_t  baud_idx = 4;          /* Default: 115200 */
static bool     hex_mode = false;

/* Our private read pointer into the DMA circular RX buffer */
static uint16_t display_tail;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static void term_clear(void);
static void term_newline(void);
static void term_putchar(uint8_t ch);
static bool bridge_process_rx(void);
static void bridge_draw(void);
static void bridge_apply_baud(uint8_t new_idx);
static uint16_t dma_rx_head(void);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/*============================================================================*/
/*  DMA helper                                                                */
/*============================================================================*/

/**
 * @brief  Return the current DMA write position in the circular RX buffer.
 *
 * The GPDMA counter counts DOWN from M1_LOGDB_RX_BUFFER_SIZE to 0.
 * This helper converts it to a head index in [0, M1_LOGDB_RX_BUFFER_SIZE).
 *
 * @pre  USART1 DMA RX must be active (guaranteed during VCP mode).
 */
static uint16_t dma_rx_head(void)
{
    return (M1_LOGDB_RX_BUFFER_SIZE
            - __HAL_DMA_GET_COUNTER(huart_logdb.hdmarx))
            % M1_LOGDB_RX_BUFFER_SIZE;
}

/*============================================================================*/
/*  Terminal buffer helpers                                                   */
/*============================================================================*/

static void term_clear(void)
{
    memset(term_buf, 0, sizeof(term_buf));
    term_row = 0;
    term_col = 0;
    rx_count = 0;
}

static void term_newline(void)
{
    term_col = 0;
    term_row++;
    if (term_row >= TERM_ROWS)
    {
        /* Scroll up by one line */
        memmove(term_buf[0], term_buf[1],
                (TERM_ROWS - 1) * (TERM_COLS + 1));
        memset(term_buf[TERM_ROWS - 1], 0, TERM_COLS + 1);
        term_row = TERM_ROWS - 1;
    }
}

static void term_putchar(uint8_t ch)
{
    if (hex_mode)
    {
        /* Print each byte as "XX " */
        char hex[4];
        snprintf(hex, sizeof(hex), "%02X ", ch);
        for (uint8_t i = 0; hex[i]; i++)
        {
            if (term_col >= TERM_COLS)
                term_newline();
            term_buf[term_row][term_col++] = hex[i];
        }
    }
    else
    {
        /* ASCII mode */
        if (ch == '\n')
        {
            term_newline();
            return;
        }
        if (ch == '\r')
            return;                     /* Silently skip CR */
        if (ch < 0x20 || ch > 0x7E)
            ch = '.';                   /* Replace non-printable */

        if (term_col >= TERM_COLS)
            term_newline();
        term_buf[term_row][term_col++] = (char)ch;
    }
}

/*============================================================================*/
/*  RX processing — taps the DMA circular buffer for display only             */
/*============================================================================*/

/**
 * @brief  Read newly arrived UART data from the DMA circular buffer.
 *
 * This function reads the same physical buffer that the USART1 IDLE ISR
 * uses to feed h_uart_rx_streambuf.  We maintain our own tail pointer so
 * we do not interfere with the VCP task data path — the DMA buffer is
 * circular and data is never "consumed", only overwritten on wrap.
 *
 * @return true if new data was processed.
 */
static bool bridge_process_rx(void)
{
    uint16_t new_head = dma_rx_head();

    if (new_head == display_tail)
        return false;

    /* Calculate available bytes (circular arithmetic) */
    uint16_t avail;
    if (new_head >= display_tail)
        avail = new_head - display_tail;
    else
        avail = M1_LOGDB_RX_BUFFER_SIZE - display_tail + new_head;

    /* If more than 75% of the buffer is unread, the DMA has likely
     * wrapped past our read pointer and overwritten data we haven't
     * consumed.  Skip ahead to the current head to avoid displaying
     * corrupted bytes.  This can happen at high baud rates when the
     * UI poll rate cannot keep up with incoming data. */
    if (avail > (M1_LOGDB_RX_BUFFER_SIZE * 3u / 4u))
    {
        rx_count += avail;
        display_tail = new_head;
        return true;
    }

    /* Feed bytes into the terminal buffer */
    for (uint16_t i = 0; i < avail; i++)
    {
        term_putchar(logdb_rx_buffer[display_tail]);
        rx_count++;
        display_tail = (display_tail + 1) % M1_LOGDB_RX_BUFFER_SIZE;
    }
    return true;
}

/*============================================================================*/
/*  Display                                                                   */
/*============================================================================*/

static void bridge_draw(void)
{
    char line[28];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* ── Title bar ── */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    snprintf(line, sizeof(line), "UART %s %s",
             baud_table[baud_idx].label,
             hex_mode ? "HEX" : "ASCII");
    u8g2_DrawStr(&m1_u8g2, 0, 8, line);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, 128);

    /* ── Terminal rows ── */
    for (uint8_t i = 0; i < TERM_ROWS; i++)
    {
        if (term_buf[i][0] != '\0')
            u8g2_DrawStr(&m1_u8g2, 0, 20 + i * 8, term_buf[i]);
    }

    /* ── Bottom bar ── */
    m1_draw_bottom_bar(&m1_u8g2, arrowleft_8x8, "Baud",
                       "OK:Mode", arrowright_8x8);

    m1_u8g2_nextpage();
}

/*============================================================================*/
/*  Baud rate change                                                          */
/*============================================================================*/

static void bridge_apply_baud(uint8_t new_idx)
{
    if (new_idx >= BAUD_COUNT)
        return;

    baud_idx = new_idx;
    linecoding.bitrate = baud_table[baud_idx].rate;
    m1_usb_cdc_comconfig();         /* deinit + reinit USART1 */

    /* Re-sync our read pointer to the new DMA position */
    display_tail = dma_rx_head();
}

/*============================================================================*/
/*  Public entry point — blocking delegate                                    */
/*============================================================================*/

void gpio_uart_bridge_run(void)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t       q_item;
    BaseType_t           ret;
    bool                 need_redraw = true;
    const TickType_t     poll_ticks  = pdMS_TO_TICKS(BRIDGE_POLL_MS);

    /* ── Save original state ── */
    enCdcMode saved_mode = m1_usbcdc_mode;
    uint32_t  saved_baud = linecoding.bitrate;

    /* ── Suppress debug log output to avoid UART data corruption ── */
    m1_logdb_set_suppress(true);

    /* ── Switch to VCP mode at selected baud rate ── */
    m1_usbcdc_mode     = CDC_MODE_VCP;
    linecoding.bitrate  = baud_table[baud_idx].rate;
    m1_usb_cdc_comconfig();         /* deinit + reinit USART1 with DMA */

    /* ── Initialise terminal display ── */
    term_clear();

    /* Sync our read pointer to the current DMA write position */
    display_tail = dma_rx_head();

    /* ── Main bridge loop ── */
    while (1)
    {
        /* Check for new UART data */
        if (bridge_process_rx())
            need_redraw = true;

        if (need_redraw)
        {
            bridge_draw();
            need_redraw = false;
        }

        /* Wait for button event with short timeout (keeps UI responsive) */
        ret = xQueueReceive(main_q_hdl, &q_item, poll_ticks);
        if (ret != pdTRUE || q_item.q_evt_type != Q_EVENT_KEYPAD)
            continue;

        xQueueReceive(button_events_q_hdl, &btn, 0);

        if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            break;                  /* Exit bridge */
        }
        else if (btn.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
        {
            /* Decrease baud rate (wrap around) */
            uint8_t new_idx = (baud_idx > 0) ? (baud_idx - 1)
                                              : (BAUD_COUNT - 1);
            bridge_apply_baud(new_idx);
            term_clear();
            need_redraw = true;
        }
        else if (btn.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
        {
            /* Increase baud rate (wrap around) */
            uint8_t new_idx = (baud_idx < BAUD_COUNT - 1) ? (baud_idx + 1)
                                                           : 0;
            bridge_apply_baud(new_idx);
            term_clear();
            need_redraw = true;
        }
        else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            /* Toggle ASCII ↔ HEX display mode */
            hex_mode = !hex_mode;
            term_clear();
            need_redraw = true;
        }
        else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            /* Clear terminal */
            term_clear();
            need_redraw = true;
        }
    }

    /* ── Restore original UART state ── */
    m1_usbcdc_mode     = saved_mode;
    linecoding.bitrate  = saved_baud;
    m1_usb_cdc_comconfig();

    /* Resume debug logging */
    m1_logdb_set_suppress(false);

    xQueueReset(main_q_hdl);
}
