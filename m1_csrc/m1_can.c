/* See COPYING.txt for license details. */

/*
*
*  m1_can.c
*
*  M1 CAN bus (FDCAN1) — CAN Commander
*
*  Exposes FDCAN1 on J7 (X10) via PD0 (RX) / PD1 (TX).
*  Requires external CAN transceiver board (e.g. Waveshare SN65HVD230 3.3 V).
*
* M1 Project
*
*/

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_can.h"

#ifdef M1_APP_CAN_ENABLE

#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_tasks.h"
#include "m1_system.h"
#include "m1_buzzer.h"
#include "m1_log_debug.h"

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG        "CAN"

/* Bit-timing table for a 25 MHz FDCAN clock source.
 * Each row: { prescaler, time_seg1, time_seg2, sjw } → Nominal bit rate
 *   Bit time TQ count = 1 + seg1 + seg2
 *   Baud = 25 MHz / (prescaler × TQ count)                                  */
typedef struct {
    uint16_t    prescaler;
    uint8_t     seg1;
    uint8_t     seg2;
    uint8_t     sjw;
    uint32_t    baud_bps;
    const char *label;
} can_baud_entry_t;

static const can_baud_entry_t can_baud_table[CAN_BAUD_NUM] = {
    /* 125 kbps : 25 MHz / (10 × 20) = 125000 */
    { .prescaler = 10, .seg1 = 16, .seg2 = 3, .sjw = 3, .baud_bps = 125000, .label = "125 kbps" },
    /* 250 kbps : 25 MHz / (5 × 20) = 250000 */
    { .prescaler = 5,  .seg1 = 16, .seg2 = 3, .sjw = 3, .baud_bps = 250000, .label = "250 kbps" },
    /* 500 kbps : 25 MHz / (5 × 10) = 500000 */
    { .prescaler = 5,  .seg1 = 7,  .seg2 = 2, .sjw = 2, .baud_bps = 500000, .label = "500 kbps" },
    /* 1 Mbps   : 25 MHz / (1 × 25) = 1000000 */
    { .prescaler = 1,  .seg1 = 21, .seg2 = 3, .sjw = 3, .baud_bps = 1000000, .label = "1 Mbps"  },
};

/************************** V A R I A B L E S ********************************/

static m1_can_msg_t     msg_buf[CAN_MSG_BUF_SIZE];
static volatile uint16_t msg_buf_head;          /* Next write position        */
static volatile uint16_t msg_buf_count;         /* Number of items in buffer  */

static m1_can_baud_t    current_baud  = CAN_BAUD_500K;
static bool             can_running   = false;

extern FDCAN_HandleTypeDef hfdcan1;
extern QueueHandle_t       main_q_hdl;
extern QueueHandle_t       button_events_q_hdl;

/*************** F U N C T I O N   P R O T O T Y P E S ***********************/

static void can_apply_baud(m1_can_baud_t baud);
static bool can_poll_rx(m1_can_msg_t *out);
static void can_draw_sniffer_frame(const m1_can_msg_t *msgs, uint16_t count,
                                   uint16_t scroll_pos, m1_can_baud_t baud);
static void can_draw_send_screen(uint32_t id, const uint8_t *data, uint8_t dlc,
                                 uint8_t cursor_pos);

/*************** F U N C T I O N   I M P L E M E N T A T I O N **************/

/*============================================================================*/
/*  Low-level: start / stop / transmit / receive                              */
/*============================================================================*/

/**
  * @brief  Reconfigure FDCAN1 bit timing for the chosen baud rate and start.
  */
bool m1_can_start(m1_can_baud_t baud)
{
    if (baud >= CAN_BAUD_NUM)
        return false;

    /* Stop first if already running */
    if (can_running)
        m1_can_stop();

    can_apply_baud(baud);

    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
    {
        M1_LOG_E(M1_LOGDB_TAG, "FDCAN start failed");
        return false;
    }

    can_running  = true;
    current_baud = baud;

    msg_buf_head  = 0;
    msg_buf_count = 0;

    M1_LOG_I(M1_LOGDB_TAG, "CAN started @ %s", can_baud_table[baud].label);
    return true;
}

void m1_can_stop(void)
{
    if (can_running)
    {
        HAL_FDCAN_Stop(&hfdcan1);
        can_running = false;
        M1_LOG_I(M1_LOGDB_TAG, "CAN stopped");
    }
}

bool m1_can_transmit(uint32_t id, bool extended, const uint8_t *data, uint8_t dlc)
{
    FDCAN_TxHeaderTypeDef tx_hdr = {0};

    if (!can_running || dlc > CAN_DLC_MAX)
        return false;

    tx_hdr.Identifier          = id;
    tx_hdr.IdType              = extended ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
    tx_hdr.TxFrameType         = FDCAN_DATA_FRAME;
    tx_hdr.DataLength          = (uint32_t)dlc << 16;   /* HAL encodes DLC in [19:16] */
    tx_hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_hdr.BitRateSwitch       = FDCAN_BRS_OFF;
    tx_hdr.FDFormat            = FDCAN_CLASSIC_CAN;
    tx_hdr.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    tx_hdr.MessageMarker       = 0;

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_hdr, (uint8_t *)data) != HAL_OK)
    {
        M1_LOG_E(M1_LOGDB_TAG, "TX failed id=0x%lX", (unsigned long)id);
        return false;
    }
    return true;
}

/*============================================================================*/
/*  Internals                                                                 */
/*============================================================================*/

static void can_apply_baud(m1_can_baud_t baud)
{
    const can_baud_entry_t *e = &can_baud_table[baud];

    /* Must be in init mode to change timing */
    HAL_FDCAN_Stop(&hfdcan1);
    HAL_FDCAN_DeInit(&hfdcan1);

    hfdcan1.Init.NominalPrescaler    = e->prescaler;
    hfdcan1.Init.NominalSyncJumpWidth = e->sjw;
    hfdcan1.Init.NominalTimeSeg1     = e->seg1;
    hfdcan1.Init.NominalTimeSeg2     = e->seg2;
    /* Keep data timing identical for classic CAN (unused but required) */
    hfdcan1.Init.DataPrescaler       = e->prescaler;
    hfdcan1.Init.DataSyncJumpWidth   = e->sjw;
    hfdcan1.Init.DataTimeSeg1        = e->seg1;
    hfdcan1.Init.DataTimeSeg2        = e->seg2;

    HAL_FDCAN_Init(&hfdcan1);

    /* Re-apply global filter */
    HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
        FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT,
        FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
}

/** Non-blocking poll: returns true if a frame was copied into *out. */
static bool can_poll_rx(m1_can_msg_t *out)
{
    FDCAN_RxHeaderTypeDef rx_hdr;
    uint8_t               rx_data[CAN_DLC_MAX];

    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rx_hdr, rx_data) != HAL_OK)
        return false;

    out->id          = rx_hdr.Identifier;
    out->is_extended = (rx_hdr.IdType == FDCAN_EXTENDED_ID);
    out->is_rtr      = (rx_hdr.RxFrameType == FDCAN_REMOTE_FRAME);
    out->dlc         = (uint8_t)(rx_hdr.DataLength >> 16);
    out->timestamp   = HAL_GetTick();
    memcpy(out->data, rx_data, out->dlc);

    return true;
}

/*============================================================================*/
/*  Display helpers                                                           */
/*============================================================================*/

static void can_draw_sniffer_frame(const m1_can_msg_t *msgs, uint16_t count,
                                   uint16_t scroll_pos, m1_can_baud_t baud)
{
    char line[40];
    uint8_t y;
    uint16_t idx;
    const uint8_t max_rows = 5;         /* Lines visible on 128×64 display    */

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title bar */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    snprintf(line, sizeof(line), "CAN Sniffer  %s", can_baud_table[baud].label);
    u8g2_DrawStr(&m1_u8g2, 0, 8, line);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, 128);

    /* Message rows */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    for (uint8_t row = 0; row < max_rows && (scroll_pos + row) < count; row++)
    {
        idx = scroll_pos + row;
        const m1_can_msg_t *m = &msgs[idx % CAN_MSG_BUF_SIZE];
        y = 20 + row * 9;

        if (m->dlc <= 3)
        {
            /* Short frame: show all bytes on one line */
            snprintf(line, sizeof(line), "%03lX [%d]", (unsigned long)m->id, m->dlc);
            uint8_t pos = (uint8_t)strlen(line);
            for (uint8_t i = 0; i < m->dlc && pos < sizeof(line) - 4; i++)
                pos += snprintf(line + pos, sizeof(line) - pos, " %02X", m->data[i]);
        }
        else
        {
            /* Longer frame: show first 4 bytes then ".." */
            snprintf(line, sizeof(line), "%03lX [%d] %02X %02X %02X %02X..",
                     (unsigned long)m->id, m->dlc,
                     m->data[0], m->data[1], m->data[2], m->data[3]);
        }
        u8g2_DrawStr(&m1_u8g2, 0, y, line);
    }

    /* Bottom bar */
    snprintf(line, sizeof(line), "%u msgs", count);
    m1_draw_bottom_bar(&m1_u8g2, arrowleft_8x8, "Back", line, arrowright_8x8);

    m1_u8g2_nextpage();
}

static void can_draw_send_screen(uint32_t id, const uint8_t *data, uint8_t dlc,
                                 uint8_t cursor_pos)
{
    char line[40];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 0, 8, "CAN Send");
    u8g2_DrawHLine(&m1_u8g2, 0, 10, 128);

    /* ID row */
    snprintf(line, sizeof(line), "ID:  0x%03lX", (unsigned long)id);
    u8g2_DrawStr(&m1_u8g2, 0, 22, line);
    if (cursor_pos == 0)
        u8g2_DrawFrame(&m1_u8g2, 28, 13, 42, 11);

    /* DLC row */
    snprintf(line, sizeof(line), "DLC: %d", dlc);
    u8g2_DrawStr(&m1_u8g2, 0, 33, line);
    if (cursor_pos == 1)
        u8g2_DrawFrame(&m1_u8g2, 28, 24, 18, 11);

    /* Data row(s) */
    uint8_t pos = 0;
    line[0] = '\0';
    for (uint8_t i = 0; i < dlc && pos < sizeof(line) - 4; i++)
        pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", data[i]);
    u8g2_DrawStr(&m1_u8g2, 0, 44, line);
    if (cursor_pos == 2)
        u8g2_DrawFrame(&m1_u8g2, 0, 35, 127, 11);

    /* Bottom bar */
    m1_draw_bottom_bar(&m1_u8g2, arrowleft_8x8, "Back", "Send", arrowright_8x8);

    m1_u8g2_nextpage();
}

/*============================================================================*/
/*  Menu hooks                                                                */
/*============================================================================*/

void menu_can_init(void)
{
    /* Nothing to do — FDCAN1 is initialised in MX_FDCAN1_Init() */
}

void menu_can_deinit(void)
{
    m1_can_stop();
}

/*============================================================================*/
/*  CAN Sniffer — main UI loop                                               */
/*============================================================================*/

void can_sniffer(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    m1_can_msg_t rx_msg;
    uint16_t scroll_pos = 0;
    bool need_redraw = true;
    TickType_t poll_ticks = pdMS_TO_TICKS(20);  /* 50 Hz poll rate */

    msg_buf_head  = 0;
    msg_buf_count = 0;

    if (!m1_can_start(current_baud))
    {
        m1_u8g2_firstpage();
        u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 4, 20, "CAN init failed!");
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 4, 35, "Check transceiver");
        u8g2_DrawStr(&m1_u8g2, 4, 45, "on J7 (X10).");
        m1_draw_bottom_bar(&m1_u8g2, arrowleft_8x8, "Back", NULL, NULL);
        m1_u8g2_nextpage();

        /* Wait for BACK key, then exit */
        while (1)
        {
            ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
            if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
            {
                xQueueReceive(button_events_q_hdl, &this_button_status, 0);
                if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
                    break;
            }
        }
        xQueueReset(main_q_hdl);
        return;
    }

    /* Main sniffer loop */
    while (1)
    {
        /* Non-blocking RX poll */
        while (can_poll_rx(&rx_msg))
        {
            msg_buf[msg_buf_head % CAN_MSG_BUF_SIZE] = rx_msg;
            msg_buf_head++;
            if (msg_buf_count < CAN_MSG_BUF_SIZE)
                msg_buf_count++;
            need_redraw = true;
        }

        if (need_redraw)
        {
            /* Auto-scroll to newest */
            if (msg_buf_count > 5)
                scroll_pos = msg_buf_count - 5;
            else
                scroll_pos = 0;

            can_draw_sniffer_frame(msg_buf, msg_buf_count, scroll_pos, current_baud);
            need_redraw = false;
        }

        /* Check for button events (non-blocking with short timeout) */
        ret = xQueueReceive(main_q_hdl, &q_item, poll_ticks);
        if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
        {
            xQueueReceive(button_events_q_hdl, &this_button_status, 0);

            if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                m1_can_stop();
                xQueueReset(main_q_hdl);
                break;
            }
            else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
            {
                if (scroll_pos > 0) scroll_pos--;
                need_redraw = true;
            }
            else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
            {
                if (scroll_pos + 5 < msg_buf_count) scroll_pos++;
                need_redraw = true;
            }
            else if (this_button_status.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
            {
                /* Cycle baud rate down */
                m1_can_stop();
                current_baud = (current_baud == 0) ? CAN_BAUD_NUM - 1 : current_baud - 1;
                m1_can_start(current_baud);
                msg_buf_head = 0;
                msg_buf_count = 0;
                need_redraw = true;
            }
            else if (this_button_status.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
            {
                /* Cycle baud rate up */
                m1_can_stop();
                current_baud = (current_baud + 1) % CAN_BAUD_NUM;
                m1_can_start(current_baud);
                msg_buf_head = 0;
                msg_buf_count = 0;
                need_redraw = true;
            }
            else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                /* Clear buffer */
                msg_buf_head = 0;
                msg_buf_count = 0;
                scroll_pos = 0;
                need_redraw = true;
            }
        }
    } /* while (1) */
}

/*============================================================================*/
/*  CAN Send — manual frame builder                                          */
/*============================================================================*/

void can_send(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;

    uint32_t tx_id   = 0x123;
    uint8_t  tx_dlc  = 8;
    uint8_t  tx_data[CAN_DLC_MAX] = {0};
    uint8_t  cursor  = 0;           /* 0=ID, 1=DLC, 2=Data */
    bool     need_redraw = true;

    if (!can_running)
    {
        if (!m1_can_start(current_baud))
        {
            m1_u8g2_firstpage();
            u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 4, 20, "CAN init failed!");
            u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 4, 35, "Check transceiver");
            m1_draw_bottom_bar(&m1_u8g2, arrowleft_8x8, "Back", NULL, NULL);
            m1_u8g2_nextpage();

            while (1)
            {
                ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
                if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
                {
                    xQueueReceive(button_events_q_hdl, &this_button_status, 0);
                    if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
                        break;
                }
            }
            xQueueReset(main_q_hdl);
            return;
        }
    }

    while (1)
    {
        if (need_redraw)
        {
            can_draw_send_screen(tx_id, tx_data, tx_dlc, cursor);
            need_redraw = false;
        }

        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE || q_item.q_evt_type != Q_EVENT_KEYPAD)
            continue;

        xQueueReceive(button_events_q_hdl, &this_button_status, 0);

        if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            xQueueReset(main_q_hdl);
            break;
        }
        else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            /* Increment selected field */
            if (cursor == 0) tx_id = (tx_id + 1) & 0x7FF;
            else if (cursor == 1) { if (tx_dlc < CAN_DLC_MAX) tx_dlc++; }
            else { /* Increment first data byte */ tx_data[0] = (tx_data[0] + 1) & 0xFF; }
            need_redraw = true;
        }
        else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (cursor == 0) tx_id = (tx_id - 1) & 0x7FF;
            else if (cursor == 1) { if (tx_dlc > 0) tx_dlc--; }
            else { tx_data[0] = (tx_data[0] - 1) & 0xFF; }
            need_redraw = true;
        }
        else if (this_button_status.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (cursor > 0) cursor--;
            need_redraw = true;
        }
        else if (this_button_status.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (cursor < 2) cursor++;
            need_redraw = true;
        }
        else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            /* Send the frame */
            if (m1_can_transmit(tx_id, false, tx_data, tx_dlc))
            {
                m1_buzzer_notification();
                M1_LOG_I(M1_LOGDB_TAG, "TX id=0x%03lX dlc=%d", (unsigned long)tx_id, tx_dlc);
            }
            need_redraw = true;
        }
    } /* while (1) */
}

/*============================================================================*/
/*  CAN Saved — placeholder for future .can file support                     */
/*============================================================================*/

void can_saved(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;

    m1_u8g2_firstpage();
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 10, 25, "Coming soon!");
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 4, 40, "Save/load CAN logs");
    u8g2_DrawStr(&m1_u8g2, 4, 50, "to SD card.");
    m1_draw_bottom_bar(&m1_u8g2, arrowleft_8x8, "Back", NULL, NULL);
    m1_u8g2_nextpage();

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
        }
    }
}

#endif /* M1_APP_CAN_ENABLE */
