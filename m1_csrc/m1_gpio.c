/* See COPYING.txt for license details. */

/*
*
*  m1_gpio.c
*
*  M1 GPIO functions
*
* M1 Project
*
*/

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_gpio.h"
#include "m1_gpio_uart.h"

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG	"GPIO"

#define THIS_LCD_MENU_TEXT_FIRST_ROW_Y			11
#define THIS_LCD_MENU_TEXT_FRAME_FIRST_ROW_Y	1
#define THIS_LCD_MENU_TEXT_ROW_SPACE			10

//************************** C O N S T A N T **********************************/

const char *m1_ext_gpio_label[M1_EXT_GPIO_LIST_N] = {	"Power 3.3V",
														"Power 5.0V",
														"",
														"Pin PE2",
														"Pin PE4",
														"Pin PE5",
														"Pin PE6",
														"Pin PD12",
														"Pin PD13",
														"Pin PA14",
														"Pin PA13",
														/*"Pin PA9",*/
														/*"Pin PA10",*/
														"Pin PC2",
														"Pin PC3",
														"Pin PD0",
														"Pin PD1"
													};

//************************** S T R U C T U R E S *******************************

/***************************** V A R I A B L E S ******************************/

S_GPIO_IO_t m1_ext_gpio[M1_EXT_GPIO_LIST_N] = {	{.gpio_port = EN_EXT_3V3_GPIO_Port, .gpio_pin = EN_EXT_3V3_Pin},
												{.gpio_port = EN_EXT_5V_GPIO_Port, .gpio_pin = EN_EXT_5V_Pin},
												{.gpio_port = EN_EXT2_5V_GPIO_Port, .gpio_pin = EN_EXT2_5V_Pin},
												{.gpio_port = PE2_GPIO_Port, .gpio_pin = PE2_Pin},
												{.gpio_port = PE2_GPIO_Port, .gpio_pin = PE4_Pin},
												{.gpio_port = PE2_GPIO_Port, .gpio_pin = PE5_Pin},
												{.gpio_port = PE2_GPIO_Port, .gpio_pin = PE6_Pin},
												{.gpio_port = PD12_GPIO_Port, .gpio_pin = PD12_Pin},
												{.gpio_port = PD13_GPIO_Port, .gpio_pin = PD13_Pin},
												{.gpio_port = SWCLK_GPIO_Port, .gpio_pin = SWCLK_Pin},
												{.gpio_port = SWDIO_GPIO_Port, .gpio_pin = SWDIO_Pin},
												/*{.gpio_port = UART_1_TX_GPIO_Port, .gpio_pin = UART_1_TX_Pin},*/
												/*{.gpio_port = UART_1_RX_GPIO_Port, .gpio_pin = UART_1_RX_Pin},*/
												{.gpio_port = PC2_GPIO_Port, .gpio_pin = PC2_Pin},
												{.gpio_port = PC3_GPIO_Port, .gpio_pin = PC3_Pin},
												{.gpio_port = PD0_GPIO_Port, .gpio_pin = PD0_Pin},
												{.gpio_port = PD1_GPIO_Port, .gpio_pin = PD1_Pin}
											};

static uint8_t m1_ext_gpio_stat[M1_EXT_GPIO_LIST_N] = {0};
static uint8_t m1_ext_gpio_id = M1_EXT_GPIO_FIRST_ID; // Default to the first ext. GPIO [PE2_GPIO_Port, PE2_Pin]

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

void menu_gpio_init(void);
void menu_gpio_exit(void);

void gpio_manual_control(void);
void gpio_5v_on_gpio(void);
void gpio_3_3v_on_gpio(void);
void gpio_usb_uart_bridge(void);
void ext_power_5V_set(uint8_t set_mode);
void ext_power_3V_set(uint8_t set_mode);
void gpio_gui_update(const S_M1_Menu_t *phmenu, uint8_t sel_item);
void gpio_xkey_handler(S_M1_Key_Event event, uint8_t button_id, uint8_t sel_item);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/******************************************************************************/
/**
  * @brief Initializes display for this sub-menu item.
  * @param
  * @retval
  */
/******************************************************************************/
void menu_gpio_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint8_t i;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    for(i=0; i<M1_EXT_GPIO_LIST_N; i++)
    {
    	if ( i >= M1_EXT_GPIO_FIRST_ID ) // Do not reinitialize power control pins
    	{
    		GPIO_InitStruct.Pin = m1_ext_gpio[i].gpio_pin;
    		HAL_GPIO_Init(m1_ext_gpio[i].gpio_port, &GPIO_InitStruct);
    	}
    	HAL_GPIO_WritePin(m1_ext_gpio[i].gpio_port, m1_ext_gpio[i].gpio_pin, GPIO_PIN_RESET);
    	m1_ext_gpio_stat[i] = 0;
    }

    m1_ext_gpio_id = M1_EXT_GPIO_FIRST_ID; // Default to the first ext. GPIO [PE2_GPIO_Port, PE2_Pin]
} // void menu_gpio_init(void)


/******************************************************************************/
/**
  * @brief Exits this sub-menu and return to the upper level menu.
  * @param
  * @retval
  */
/******************************************************************************/
void menu_gpio_exit(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint8_t i;

    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    for(i=M1_EXT_GPIO_FIRST_ID; i<M1_EXT_GPIO_LIST_N; i++)
    {
    	GPIO_InitStruct.Pin = m1_ext_gpio[i].gpio_pin;
    	HAL_GPIO_Init(m1_ext_gpio[i].gpio_port, &GPIO_InitStruct);
    }

    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF0_SWJ;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN; // Pulldown for SWCLK
    GPIO_InitStruct.Pin = SWCLK_Pin;
    HAL_GPIO_Init(SWCLK_GPIO_Port, &GPIO_InitStruct); // SWCLK
    GPIO_InitStruct.Pull = GPIO_PULLUP; // Pullup for SWDIO
    GPIO_InitStruct.Pin = SWDIO_Pin;
    HAL_GPIO_Init(SWDIO_GPIO_Port, &GPIO_InitStruct); // SWDIO

    for(i=0; i<M1_EXT_GPIO_FIRST_ID; i++) // Reset power control pins
    {
    	HAL_GPIO_WritePin(m1_ext_gpio[i].gpio_port, m1_ext_gpio[i].gpio_pin, GPIO_PIN_RESET);
    }
} // void menu_gpio_exit(void)



/******************************************************************************/
/**
  * @brief
  * @param
  * @retval
  */
/******************************************************************************/
void gpio_manual_control(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	uint8_t prn_name[GUI_DISP_LINE_LEN_MAX + 1] = {0};

    m1_ext_gpio_stat[m1_ext_gpio_id] ^= 1; // Toggle

    HAL_GPIO_WritePin(m1_ext_gpio[m1_ext_gpio_id].gpio_port, m1_ext_gpio[m1_ext_gpio_id].gpio_pin, m1_ext_gpio_stat[m1_ext_gpio_id]);

	sprintf(prn_name, "%s: %s", m1_ext_gpio_label[m1_ext_gpio_id], (m1_ext_gpio_stat[m1_ext_gpio_id]==1)?"ON":"OFF");
	m1_info_box_display_draw(INFO_BOX_ROW_1, prn_name);
	m1_u8g2_nextpage(); // Update display RAM

	xQueueReset(main_q_hdl); // Reset main q before return
} // void gpio_manual_control(void)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void gpio_3_3v_on_gpio(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	uint8_t prn_name[GUI_DISP_LINE_LEN_MAX + 1] = {0};

    m1_ext_gpio_stat[0] ^= 1; // Toggle
    if ( m1_ext_gpio_stat[0] )
    {
    	m1_ext_gpio_stat[1] = 0; // 3.3V and 5.0V must not be turned ON at the same time
        HAL_GPIO_WritePin(m1_ext_gpio[1].gpio_port, m1_ext_gpio[1].gpio_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(m1_ext_gpio[2].gpio_port, m1_ext_gpio[2].gpio_pin, GPIO_PIN_RESET);
    }

    HAL_GPIO_WritePin(m1_ext_gpio[0].gpio_port, m1_ext_gpio[0].gpio_pin, m1_ext_gpio_stat[0]);

	sprintf(prn_name, "%s: %s", m1_ext_gpio_label[0], (m1_ext_gpio_stat[0]==1)?"ON":"OFF");
	m1_info_box_display_draw(INFO_BOX_ROW_1, prn_name);
	m1_u8g2_nextpage(); // Update display RAM

	xQueueReset(main_q_hdl); // Reset main q before return
} // void gpio_3_3v_on_gpio(void)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void gpio_5v_on_gpio(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	uint8_t prn_name[GUI_DISP_LINE_LEN_MAX + 1] = {0};

    m1_ext_gpio_stat[1] ^= 1; // Toggle
    if ( m1_ext_gpio_stat[1] )
    {
    	m1_ext_gpio_stat[0] = 0; // 3.3V and 5.0V must not be turned ON at the same time
    	HAL_GPIO_WritePin(m1_ext_gpio[0].gpio_port, m1_ext_gpio[0].gpio_pin, GPIO_PIN_RESET);
    }

    HAL_GPIO_WritePin(m1_ext_gpio[1].gpio_port, m1_ext_gpio[1].gpio_pin, m1_ext_gpio_stat[1]);
    HAL_GPIO_WritePin(m1_ext_gpio[2].gpio_port, m1_ext_gpio[2].gpio_pin, m1_ext_gpio_stat[1]);

	sprintf(prn_name, "%s: %s", m1_ext_gpio_label[1], (m1_ext_gpio_stat[1]==1)?"ON":"OFF");
	m1_info_box_display_draw(INFO_BOX_ROW_1, prn_name);
	m1_u8g2_nextpage(); // Update display RAM

	xQueueReset(main_q_hdl); // Reset main q before return
} // void gpio_5v_on_gpio(void)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void gpio_usb_uart_bridge(void)
{
	gpio_uart_bridge_run();
} // void gpio_usb_uart_bridge(void)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void ext_power_5V_set(uint8_t set_mode)
{
	HAL_GPIO_WritePin(EN_EXT_5V_GPIO_Port, EN_EXT_5V_Pin, set_mode);
	HAL_GPIO_WritePin(EN_EXT2_5V_GPIO_Port, EN_EXT2_5V_Pin, set_mode);
} // void ext_power_5V_set(uint8_t set_mode)


/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void ext_power_3V_set(uint8_t set_mode)
{
	  HAL_GPIO_WritePin(EN_EXT_3V3_GPIO_Port, EN_EXT_3V3_Pin, set_mode);
} // void ext_power_5V_set(uint8_t set_mode)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void gpio_gui_update(const S_M1_Menu_t *phmenu, uint8_t sel_item)
{
	uint8_t i, n_items;
	uint8_t menu_text_y;
	uint8_t prn_name[GUI_DISP_LINE_LEN_MAX + 1] = {0};
	uint16_t msg_len, msg_id;

	n_items = phmenu->num_submenu_items;
	menu_text_y = THIS_LCD_MENU_TEXT_FIRST_ROW_Y;

	/* Graphic work starts here */
	m1_u8g2_firstpage(); // This call required for page drawing in mode 1
    do
    {
    	for (i=0; i<n_items; i++)
    	{
    		if ( i==sel_item )
    		{
    			// Draw box for selected menu item with text color
    			u8g2_DrawBox(&m1_u8g2, 0, menu_text_y - THIS_LCD_MENU_TEXT_ROW_SPACE + 2, M1_LCD_SUB_MENU_TEXT_FRAME_W, THIS_LCD_MENU_TEXT_ROW_SPACE);
    			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG); // set to background color
    			u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
    			u8g2_DrawStr(&m1_u8g2, 4, menu_text_y, phmenu->submenu[i]->title);
    			if ( i==0 ) // Index of GPIO Control
    			{
    		    	// Draw arrows left and right
    		    	u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - 40, menu_text_y - THIS_LCD_MENU_TEXT_ROW_SPACE + 2, 10, 10, arrowleft_10x10);
    		    	u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - 20, menu_text_y - THIS_LCD_MENU_TEXT_ROW_SPACE + 2, 10, 10, arrowright_10x10);
    			}
    			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT); // return to text color
    			u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N); // return to default font
    		}
    		else
    		{
    			u8g2_DrawStr(&m1_u8g2, 4, menu_text_y, phmenu->submenu[i]->title);
    		}
    		menu_text_y += THIS_LCD_MENU_TEXT_ROW_SPACE;
    	} // for (i=0; i<n_items; i++)

    	// Draw info box at the bottom
    	m1_info_box_display_init(false);

    	switch ( sel_item )
    	{
    		case 0: // GPIO
    			sprintf(prn_name, "%s: %s", m1_ext_gpio_label[m1_ext_gpio_id], (m1_ext_gpio_stat[m1_ext_gpio_id]==1)?"ON":"OFF");
    			m1_info_box_display_draw(INFO_BOX_ROW_1, prn_name);
    			break;

    		case 1: // Power 3.3V
    			sprintf(prn_name, "%s: %s", m1_ext_gpio_label[0], (m1_ext_gpio_stat[0]==1)?"ON":"OFF");
    	    	m1_info_box_display_draw(INFO_BOX_ROW_1, prn_name);
    			break;

    		case 2: // Power 5.0V
    			sprintf(prn_name, "%s: %s", m1_ext_gpio_label[1], (m1_ext_gpio_stat[1]==1)?"ON":"OFF");
    	    	m1_info_box_display_draw(INFO_BOX_ROW_1, prn_name);
    			break;

    		case 3:
    	    	m1_info_box_display_draw(INFO_BOX_ROW_1, "USB-UART Bridge");
    			break;

    		default: // Unknown selection
    			break;
    	} // switch ( sel_item )
    } while (m1_u8g2_nextpage());

} // void gpio_gui_update(const S_M1_Menu_t *phmenu, uint8_t sel_item)



/******************************************************************************/
/**
  * @brief
  * @param None
  * @retval None
  */
/******************************************************************************/
void gpio_xkey_handler(S_M1_Key_Event event, uint8_t button_id, uint8_t sel_item)
{
	uint8_t prn_name[GUI_DISP_LINE_LEN_MAX + 1] = {0};

	if ( sel_item != 0) // Not the index of GPIO Control
		return;

	if ( event==BUTTON_EVENT_CLICK )
	{
		if ( button_id==BUTTON_LEFT_KP_ID ) // Left arrow key
		{
			m1_ext_gpio_id--;
			if ( m1_ext_gpio_id < M1_EXT_GPIO_FIRST_ID )
				m1_ext_gpio_id = M1_EXT_GPIO_LIST_N - 1;
		} // if ( button_id==BUTTON_LEFT_KP_ID )
		else if ( button_id==BUTTON_RIGHT_KP_ID ) // Right arrow key
		{
			m1_ext_gpio_id++;
			if ( m1_ext_gpio_id >= M1_EXT_GPIO_LIST_N )
				m1_ext_gpio_id = M1_EXT_GPIO_FIRST_ID;
		}

		sprintf(prn_name, "%s: %s", m1_ext_gpio_label[m1_ext_gpio_id], (m1_ext_gpio_stat[m1_ext_gpio_id]==1)?"ON":"OFF");
    	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG); // set to background color
    	// Clear old content
    	m1_info_box_display_clear();
    	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT); // set to text color
		m1_info_box_display_draw(INFO_BOX_ROW_1, prn_name);

		m1_u8g2_nextpage(); // Update LCD display RAM
	} // if ( event==BUTTON_EVENT_CLICK )
} // void gpio_xkey_handler(S_M1_Key_Event event, uint8_t button_id, uint8_t)
