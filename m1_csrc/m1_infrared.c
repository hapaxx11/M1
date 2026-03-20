/* See COPYING.txt for license details. */

/*
*
*  m1_infrared.c
*
*  M1 Infrared functions
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
#include "m1_infrared.h"
#include "irmp.h"
#include "irsnd.h"
#include "m1_storage.h"
#include "m1_file_browser.h"
#include "m1_file_util.h"
#include "ff.h"

/*************************** D E F I N E S ************************************/


//************************** S T R U C T U R E S *******************************

/***************************** V A R I A B L E S ******************************/

TIM_HandleTypeDef   Timerhdl_IrCarrier;
TIM_HandleTypeDef   Timerhdl_IrTx;
TIM_HandleTypeDef   Timerhdl_IrRx;
IRMP_DATA 			irmp_data;

volatile S_M1_IR_Det IrRx_Edge_Det; // Flag for first falling edge detected

volatile uint8_t ir_ota_data_tx_active;
uint8_t ir_ota_data_tx_len;
volatile uint8_t ir_ota_data_tx_counter;
uint16_t *pir_ota_data_tx_buffer;
static TimerHandle_t ir_tx_timer_hdl = NULL;

static IRMP_DATA 			irmp_loopback_data;
static uint8_t				new_remote_learned;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

void menu_infrared_init(void);
void menu_infrared_exit(void);

void infrared_universal_remotes(void);
void infrared_learn_new_remote(void);
void infrared_saved_remotes(void);
S_M1_IR_Tx_States infrared_transmit(uint8_t init);

static void infrared_decode_sys_init(void);
static void infrared_decode_sys_deinit(void);
void infrared_encode_sys_init(void);
void infrared_encode_sys_deinit(void);
static void infrared_encode_timer_cb(TimerHandle_t xTimer);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/*============================================================================*/
/*
 * "This function initializes display for this sub-menu item.
 */
/*============================================================================*/
void menu_infrared_init(void)
{
	;
	new_remote_learned = 0;
} // void menu_infrared_init(void)



/*============================================================================*/
/*
 * This function will exit this sub-menu and return to the upper level menu.
 */
/*============================================================================*/
void menu_infrared_exit(void)
{
	;
} // void menu_infrared_exit(void)



/*============================================================================*/
/*
 * This function will transmit infrared frame(s) based on pre-configured data
 * Return: state machine's status of the transmitter
 */
/*============================================================================*/
S_M1_IR_Tx_States infrared_transmit(uint8_t init)
{
	static uint32_t ir_delay_time;
	static S_M1_IR_Tx_States ir_tx_state = IR_TX_INIT;
	uint16_t wtemp;
	BaseType_t ret;

	if (init)
	{
		ir_tx_state = IR_TX_INIT;
		return 0;
	} // if (init)

	while (1) // Conditional loop
	{
		switch ( ir_tx_state )
		{
			case IR_TX_INIT:
				wtemp = m1_make_ir_ota_multiframes();
				if ( wtemp )
				{
					if ( wtemp==TRUE ) // Return value is not a delay time?
					{
						if ( m1_check_ir_ota_frame_status() )
						{
							ir_ota_data_tx_active = TRUE;
							ir_ota_data_tx_counter = 0;
							ir_ota_data_tx_len = m1_get_ir_ota_frame_len();
							pir_ota_data_tx_buffer = m1_get_ir_ota_buffer_ptr();
							__HAL_TIM_URS_ENABLE(&Timerhdl_IrTx); // Enable URS to temporarily disable the UIF when the UG bit is set
							Timerhdl_IrTx.Instance->ARR = pir_ota_data_tx_buffer[0]; // Update Auto Reload Register ARR value
							HAL_TIM_GenerateEvent(&Timerhdl_IrTx, TIM_EVENTSOURCE_UPDATE); // Generate Update Event (set UG bit) to reload the valid ARR and reset the counter
							__HAL_TIM_URS_DISABLE(&Timerhdl_IrTx); // Disable URS to enable the UIF again
							  /* Check if the update flag is set after the Update Generation, if so clear the UIF flag */
							if (HAL_IS_BIT_SET(Timerhdl_IrTx.Instance->SR, TIM_FLAG_UPDATE))
							{
								/* Clear the update flag */
								CLEAR_BIT(Timerhdl_IrTx.Instance->SR, TIM_FLAG_UPDATE);
							}
							if ( pir_ota_data_tx_buffer[0] & 0x0001 ) // First OTA bit is a Mark?
								irsnd_on(); // Start the PWM of the carrier at the output
							__HAL_TIM_ENABLE(&Timerhdl_IrTx); // Start the timer of the baseband to control the carrier
							// Update reload value for the next bit (next period)
							Timerhdl_IrTx.Instance->ARR = pir_ota_data_tx_buffer[++ir_ota_data_tx_counter]; // Save the ARR for the next bit
							//__HAL_TIM_SET_COUNTER(&Timerhdl_IrTx, 0);
							ir_tx_state = IR_TX_ACTIVE; // update state machine
						} // if ( m1_check_ir_ota_frame_status() )
						else // OTA frame buffer is not ready for some reason. Let finish.
						{
							; // Send warning message to display
							; // exit
							ir_tx_state = IR_TX_COMPLETED;
						}
					}// if ( wtemp==TRUE )
					else // Let do a delay with the delay time given in the return value
					{
						ir_delay_time = wtemp;
						ir_delay_time /= 1000; // Convert delay time from us to ms
						ret = pdFAIL;
						if ( ir_delay_time ) // Valid delay time?
						{
							// Create the one shot timer for the delay
							ir_tx_timer_hdl = xTimerCreate( "Ir_Tx_Delay_Once", pdMS_TO_TICKS(ir_delay_time), pdFALSE, 0, infrared_encode_timer_cb);
							if( ir_tx_timer_hdl != NULL )
							{
								ret = xTimerStart( ir_tx_timer_hdl, 0 );
							} // if( ( ir_tx_timer_hdl != NULL )
						} // if ( ir_delay_time )
						if (ret!=pdPASS) // Not a valid delay time or timer fails, just finish
						{
							ir_tx_state = IR_TX_COMPLETED;
						} // if (ret!=pdPASS)
					} // else
				} // if ( wtemp )
				else // Task is complete. Let finish.
				{
					ir_tx_state = IR_TX_COMPLETED;
				}
				break;

			case IR_TX_ACTIVE:
				if ( !ir_ota_data_tx_active ) // Tx completed?
				{
					__HAL_TIM_DISABLE(&Timerhdl_IrTx); // Stop the timer of the baseband
					m1_ir_ota_frame_post_process(irmp_data.protocol);
					ir_tx_state = IR_TX_INIT; // reset to repeat the process
					continue; // Repeat the process
				} // if ( !ir_ota_data_tx_active )
				break;

			case IR_TX_COMPLETED:
				// Wait for user to exit
				break;

			default:
				break;
		} // switch ( ir_tx_state )
		break;
	} //while (1)

	return ir_tx_state;
} // S_M1_IR_Tx_States infrared_transmit(uint8_t init)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
/*============================================================================*/
/*
 * Universal Remote — built-in common IR codes
 *
 * Device categories: TV, Soundbar/Audio, AC, Projector, STB/Streaming
 * Each category has common commands (power, vol+/-, ch+/-, mute, etc.)
 * that work across major brands using standard protocol codes.
 *
 * Navigation:
 *   UP/DOWN   — scroll commands
 *   LEFT/RIGHT — switch device/brand
 *   OK        — transmit selected command
 *   BACK      — exit
 *
 * @note SiN360 — Universal Remote Phase A (built-in codes)
 */
/*============================================================================*/

/* ---- IR command entry ---- */
typedef struct {
    const char *label;      /* Display name */
    uint8_t    protocol;    /* IRMP protocol ID */
    uint16_t   address;     /* Device address */
    uint16_t   command;     /* Command code */
} ir_cmd_entry_t;

/* ---- Device database ---- */
typedef struct {
    const char *name;               /* Brand/device name */
    const ir_cmd_entry_t *cmds;     /* Command list */
    uint8_t cmd_count;              /* Number of commands */
} ir_device_t;

/* ======== TV databases ======== */
/*
 * IRMP address/command format reference:
 *   Samsung32:  addr=0x0707, cmd = byte from raw Samsung 32-bit code
 *   NEC:        addr=16-bit (device+subdevice), cmd=8-bit function
 *   SIRCS:      addr=5-bit device, cmd=7-bit function
 *   RC5:        addr=5-bit, cmd=6-bit + toggle
 *   Kaseikyo:   addr=16-bit, cmd=varies by OEM
 *
 * Sources: Flipper-IRDB, IRMP examples, Arduino IRremote community,
 *          Tasmota IR code database, manufacturer documentation
 */

/* Samsung TV (Samsung32 protocol, addr 0x0707) */
static const ir_cmd_entry_t ir_cmds_samsung_tv[] = {
    {"Power",       IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x0002},
    {"Vol +",       IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x0007},
    {"Vol -",       IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x000B},
    {"Ch +",        IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x0012},
    {"Ch -",        IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x0010},
    {"Mute",        IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x000F},
    {"Source",      IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x0001},
    {"Menu",        IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x001A},
    {"Up",          IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x0060},
    {"Down",        IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x0061},
    {"Left",        IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x0065},
    {"Right",       IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x0062},
    {"Enter",       IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x0068},
    {"HDMI",        IRMP_SAMSUNG32_PROTOCOL, 0x0707, 0x008B},
};

/* LG TV (NEC protocol, addr 0x04FB) */
static const ir_cmd_entry_t ir_cmds_lg_tv[] = {
    {"Power",       IRMP_NEC_PROTOCOL, 0x04FB, 0x0008},
    {"Vol +",       IRMP_NEC_PROTOCOL, 0x04FB, 0x0040},
    {"Vol -",       IRMP_NEC_PROTOCOL, 0x04FB, 0x00C0},
    {"Ch +",        IRMP_NEC_PROTOCOL, 0x04FB, 0x0000},
    {"Ch -",        IRMP_NEC_PROTOCOL, 0x04FB, 0x0001},
    {"Mute",        IRMP_NEC_PROTOCOL, 0x04FB, 0x0009},
    {"Input",       IRMP_NEC_PROTOCOL, 0x04FB, 0x000B},
    {"OK/Enter",    IRMP_NEC_PROTOCOL, 0x04FB, 0x0044},
    {"Up",          IRMP_NEC_PROTOCOL, 0x04FB, 0x0040},
    {"Down",        IRMP_NEC_PROTOCOL, 0x04FB, 0x0041},
    {"Left",        IRMP_NEC_PROTOCOL, 0x04FB, 0x0007},
    {"Right",       IRMP_NEC_PROTOCOL, 0x04FB, 0x0006},
    {"Home",        IRMP_NEC_PROTOCOL, 0x04FB, 0x0043},
    {"Back",        IRMP_NEC_PROTOCOL, 0x04FB, 0x0028},
};

/* Sony TV (SIRCS 12-bit, addr 0x01) */
static const ir_cmd_entry_t ir_cmds_sony_tv[] = {
    {"Power",       IRMP_SIRCS_PROTOCOL, 0x0001, 0x0015},
    {"Vol +",       IRMP_SIRCS_PROTOCOL, 0x0001, 0x0012},
    {"Vol -",       IRMP_SIRCS_PROTOCOL, 0x0001, 0x0013},
    {"Ch +",        IRMP_SIRCS_PROTOCOL, 0x0001, 0x0010},
    {"Ch -",        IRMP_SIRCS_PROTOCOL, 0x0001, 0x0011},
    {"Mute",        IRMP_SIRCS_PROTOCOL, 0x0001, 0x0014},
    {"Input",       IRMP_SIRCS_PROTOCOL, 0x0001, 0x0025},
    {"Menu",        IRMP_SIRCS_PROTOCOL, 0x0001, 0x0060},
    {"Up",          IRMP_SIRCS_PROTOCOL, 0x0001, 0x0074},
    {"Down",        IRMP_SIRCS_PROTOCOL, 0x0001, 0x0075},
    {"Left",        IRMP_SIRCS_PROTOCOL, 0x0001, 0x0034},
    {"Right",       IRMP_SIRCS_PROTOCOL, 0x0001, 0x0033},
    {"OK/Enter",    IRMP_SIRCS_PROTOCOL, 0x0001, 0x0065},
    {"Home",        IRMP_SIRCS_PROTOCOL, 0x0001, 0x0060},
};

/* Philips RC5 TV (RC5 protocol, addr 0x00) */
static const ir_cmd_entry_t ir_cmds_philips_tv[] = {
    {"Power",       IRMP_RC5_PROTOCOL, 0x0000, 0x000C},
    {"Vol +",       IRMP_RC5_PROTOCOL, 0x0000, 0x0010},
    {"Vol -",       IRMP_RC5_PROTOCOL, 0x0000, 0x0011},
    {"Ch +",        IRMP_RC5_PROTOCOL, 0x0000, 0x0020},
    {"Ch -",        IRMP_RC5_PROTOCOL, 0x0000, 0x0021},
    {"Mute",        IRMP_RC5_PROTOCOL, 0x0000, 0x000D},
    {"Source",      IRMP_RC5_PROTOCOL, 0x0000, 0x0038},
    {"Menu",        IRMP_RC5_PROTOCOL, 0x0000, 0x0012},
    {"Up",          IRMP_RC5_PROTOCOL, 0x0000, 0x0010},
    {"Down",        IRMP_RC5_PROTOCOL, 0x0000, 0x0011},
    {"Left",        IRMP_RC5_PROTOCOL, 0x0000, 0x0015},
    {"Right",       IRMP_RC5_PROTOCOL, 0x0000, 0x0016},
    {"OK/Enter",    IRMP_RC5_PROTOCOL, 0x0000, 0x0017},
    {"Standby",     IRMP_RC5_PROTOCOL, 0x0000, 0x000C},
};

/* Panasonic TV (Kaseikyo protocol, addr 0x2002 / 0x0220) */
static const ir_cmd_entry_t ir_cmds_panasonic_tv[] = {
    {"Power",       IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x003D},
    {"Vol +",       IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x0020},
    {"Vol -",       IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x0021},
    {"Ch +",        IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x0034},
    {"Ch -",        IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x0035},
    {"Mute",        IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x0032},
    {"Input",       IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x00A0},
    {"Menu",        IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x0006},
    {"Up",          IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x0052},
    {"Down",        IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x0053},
    {"Left",        IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x0054},
    {"Right",       IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x0055},
    {"OK/Enter",    IRMP_KASEIKYO_PROTOCOL, 0x2002, 0x0049},
};

/* Vizio TV (NEC protocol, addr 0x0004 / 0x20DF) */
static const ir_cmd_entry_t ir_cmds_vizio_tv[] = {
    {"Power",       IRMP_NEC_PROTOCOL, 0x0004, 0x0008},
    {"Vol +",       IRMP_NEC_PROTOCOL, 0x0004, 0x0015},
    {"Vol -",       IRMP_NEC_PROTOCOL, 0x0004, 0x0016},
    {"Ch +",        IRMP_NEC_PROTOCOL, 0x0004, 0x0010},
    {"Ch -",        IRMP_NEC_PROTOCOL, 0x0004, 0x0011},
    {"Mute",        IRMP_NEC_PROTOCOL, 0x0004, 0x0009},
    {"Input",       IRMP_NEC_PROTOCOL, 0x0004, 0x002F},
    {"Menu",        IRMP_NEC_PROTOCOL, 0x0004, 0x0043},
    {"Up",          IRMP_NEC_PROTOCOL, 0x0004, 0x0041},
    {"Down",        IRMP_NEC_PROTOCOL, 0x0004, 0x0042},
    {"OK/Enter",    IRMP_NEC_PROTOCOL, 0x0004, 0x0044},
};

/* TCL / Roku TV (NEC protocol) */
static const ir_cmd_entry_t ir_cmds_tcl_tv[] = {
    {"Power",       IRMP_NEC_PROTOCOL, 0xFE01, 0x0018},
    {"Vol +",       IRMP_NEC_PROTOCOL, 0xFE01, 0x0001},
    {"Vol -",       IRMP_NEC_PROTOCOL, 0xFE01, 0x0009},
    {"Ch +",        IRMP_NEC_PROTOCOL, 0xFE01, 0x0003},
    {"Ch -",        IRMP_NEC_PROTOCOL, 0xFE01, 0x000B},
    {"Mute",        IRMP_NEC_PROTOCOL, 0xFE01, 0x0010},
    {"Input",       IRMP_NEC_PROTOCOL, 0xFE01, 0x0000},
    {"OK/Enter",    IRMP_NEC_PROTOCOL, 0xFE01, 0x0044},
    {"Up",          IRMP_NEC_PROTOCOL, 0xFE01, 0x0040},
    {"Down",        IRMP_NEC_PROTOCOL, 0xFE01, 0x0041},
    {"Home",        IRMP_NEC_PROTOCOL, 0xFE01, 0x0015},
};

/* Hisense TV (NEC protocol) */
static const ir_cmd_entry_t ir_cmds_hisense_tv[] = {
    {"Power",       IRMP_NEC_PROTOCOL, 0x0100, 0x0018},
    {"Vol +",       IRMP_NEC_PROTOCOL, 0x0100, 0x0001},
    {"Vol -",       IRMP_NEC_PROTOCOL, 0x0100, 0x0009},
    {"Ch +",        IRMP_NEC_PROTOCOL, 0x0100, 0x0003},
    {"Ch -",        IRMP_NEC_PROTOCOL, 0x0100, 0x000B},
    {"Mute",        IRMP_NEC_PROTOCOL, 0x0100, 0x0010},
    {"Input",       IRMP_NEC_PROTOCOL, 0x0100, 0x0000},
    {"OK/Enter",    IRMP_NEC_PROTOCOL, 0x0100, 0x0044},
    {"Menu",        IRMP_NEC_PROTOCOL, 0x0100, 0x0006},
    {"Up",          IRMP_NEC_PROTOCOL, 0x0100, 0x0040},
    {"Down",        IRMP_NEC_PROTOCOL, 0x0100, 0x0041},
    {"Home",        IRMP_NEC_PROTOCOL, 0x0100, 0x001A},
};

/* Toshiba TV (NEC protocol, addr 0x40BF) */
static const ir_cmd_entry_t ir_cmds_toshiba_tv[] = {
    {"Power",       IRMP_NEC_PROTOCOL, 0x40BF, 0x0012},
    {"Vol +",       IRMP_NEC_PROTOCOL, 0x40BF, 0x001A},
    {"Vol -",       IRMP_NEC_PROTOCOL, 0x40BF, 0x001E},
    {"Ch +",        IRMP_NEC_PROTOCOL, 0x40BF, 0x001B},
    {"Ch -",        IRMP_NEC_PROTOCOL, 0x40BF, 0x001F},
    {"Mute",        IRMP_NEC_PROTOCOL, 0x40BF, 0x0010},
    {"Input",       IRMP_NEC_PROTOCOL, 0x40BF, 0x000F},
    {"Menu",        IRMP_NEC_PROTOCOL, 0x40BF, 0x0044},
    {"Up",          IRMP_NEC_PROTOCOL, 0x40BF, 0x0041},
    {"Down",        IRMP_NEC_PROTOCOL, 0x40BF, 0x0042},
    {"OK/Enter",    IRMP_NEC_PROTOCOL, 0x40BF, 0x0043},
};

/* Sharp TV (NEC protocol) */
static const ir_cmd_entry_t ir_cmds_sharp_tv[] = {
    {"Power",       IRMP_NEC_PROTOCOL, 0xA0A0, 0x001E},
    {"Vol +",       IRMP_NEC_PROTOCOL, 0xA0A0, 0x0014},
    {"Vol -",       IRMP_NEC_PROTOCOL, 0xA0A0, 0x0015},
    {"Ch +",        IRMP_NEC_PROTOCOL, 0xA0A0, 0x0016},
    {"Ch -",        IRMP_NEC_PROTOCOL, 0xA0A0, 0x0017},
    {"Mute",        IRMP_NEC_PROTOCOL, 0xA0A0, 0x001C},
    {"Input",       IRMP_NEC_PROTOCOL, 0xA0A0, 0x001A},
    {"Menu",        IRMP_NEC_PROTOCOL, 0xA0A0, 0x004E},
    {"Up",          IRMP_NEC_PROTOCOL, 0xA0A0, 0x0041},
    {"Down",        IRMP_NEC_PROTOCOL, 0xA0A0, 0x0042},
    {"OK/Enter",    IRMP_NEC_PROTOCOL, 0xA0A0, 0x0044},
};

/* Insignia TV (NEC protocol) */
static const ir_cmd_entry_t ir_cmds_insignia_tv[] = {
    {"Power",       IRMP_NEC_PROTOCOL, 0xFE01, 0x0018},
    {"Vol +",       IRMP_NEC_PROTOCOL, 0xFE01, 0x0001},
    {"Vol -",       IRMP_NEC_PROTOCOL, 0xFE01, 0x0009},
    {"Ch +",        IRMP_NEC_PROTOCOL, 0xFE01, 0x0003},
    {"Ch -",        IRMP_NEC_PROTOCOL, 0xFE01, 0x000B},
    {"Mute",        IRMP_NEC_PROTOCOL, 0xFE01, 0x0010},
    {"Input",       IRMP_NEC_PROTOCOL, 0xFE01, 0x0000},
    {"Menu",        IRMP_NEC_PROTOCOL, 0xFE01, 0x0006},
    {"OK/Enter",    IRMP_NEC_PROTOCOL, 0xFE01, 0x0044},
};

/* Sanyo TV (NEC protocol) */
static const ir_cmd_entry_t ir_cmds_sanyo_tv[] = {
    {"Power",       IRMP_NEC_PROTOCOL, 0x1C3C, 0x0018},
    {"Vol +",       IRMP_NEC_PROTOCOL, 0x1C3C, 0x0001},
    {"Vol -",       IRMP_NEC_PROTOCOL, 0x1C3C, 0x0009},
    {"Ch +",        IRMP_NEC_PROTOCOL, 0x1C3C, 0x0003},
    {"Ch -",        IRMP_NEC_PROTOCOL, 0x1C3C, 0x000B},
    {"Mute",        IRMP_NEC_PROTOCOL, 0x1C3C, 0x0010},
    {"Input",       IRMP_NEC_PROTOCOL, 0x1C3C, 0x000F},
    {"OK/Enter",    IRMP_NEC_PROTOCOL, 0x1C3C, 0x0044},
};

/* Magnavox / Funai TV (NEC protocol) */
static const ir_cmd_entry_t ir_cmds_magnavox_tv[] = {
    {"Power",       IRMP_NEC_PROTOCOL, 0x0000, 0x000A},
    {"Vol +",       IRMP_NEC_PROTOCOL, 0x0000, 0x0010},
    {"Vol -",       IRMP_NEC_PROTOCOL, 0x0000, 0x0011},
    {"Ch +",        IRMP_NEC_PROTOCOL, 0x0000, 0x0020},
    {"Ch -",        IRMP_NEC_PROTOCOL, 0x0000, 0x0021},
    {"Mute",        IRMP_NEC_PROTOCOL, 0x0000, 0x000D},
    {"Input",       IRMP_NEC_PROTOCOL, 0x0000, 0x0038},
    {"OK/Enter",    IRMP_NEC_PROTOCOL, 0x0000, 0x0017},
};

/* Generic NEC (0x00FF — many Chinese/budget TVs, set-top boxes) */
static const ir_cmd_entry_t ir_cmds_generic_nec[] = {
    {"Power",       IRMP_NEC_PROTOCOL, 0x00FF, 0x0045},
    {"Vol +",       IRMP_NEC_PROTOCOL, 0x00FF, 0x0046},
    {"Vol -",       IRMP_NEC_PROTOCOL, 0x00FF, 0x0015},
    {"Ch +",        IRMP_NEC_PROTOCOL, 0x00FF, 0x0040},
    {"Ch -",        IRMP_NEC_PROTOCOL, 0x00FF, 0x0019},
    {"Mute",        IRMP_NEC_PROTOCOL, 0x00FF, 0x0047},
    {"OK/Enter",    IRMP_NEC_PROTOCOL, 0x00FF, 0x001C},
    {"Menu",        IRMP_NEC_PROTOCOL, 0x00FF, 0x0044},
    {"Up",          IRMP_NEC_PROTOCOL, 0x00FF, 0x0018},
    {"Down",        IRMP_NEC_PROTOCOL, 0x00FF, 0x0052},
    {"Left",        IRMP_NEC_PROTOCOL, 0x00FF, 0x0008},
    {"Right",       IRMP_NEC_PROTOCOL, 0x00FF, 0x005A},
};

/* Generic NEC Alt (0xBF00 — another common Chinese address) */
static const ir_cmd_entry_t ir_cmds_generic_nec2[] = {
    {"Power",       IRMP_NEC_PROTOCOL, 0xBF00, 0x0012},
    {"Vol +",       IRMP_NEC_PROTOCOL, 0xBF00, 0x0001},
    {"Vol -",       IRMP_NEC_PROTOCOL, 0xBF00, 0x0009},
    {"Ch +",        IRMP_NEC_PROTOCOL, 0xBF00, 0x0003},
    {"Ch -",        IRMP_NEC_PROTOCOL, 0xBF00, 0x000B},
    {"Mute",        IRMP_NEC_PROTOCOL, 0xBF00, 0x0010},
    {"OK/Enter",    IRMP_NEC_PROTOCOL, 0xBF00, 0x0044},
    {"Menu",        IRMP_NEC_PROTOCOL, 0xBF00, 0x0006},
};

/* ---- Master device list ---- */
#define IR_DEVICE_COUNT 15

static const ir_device_t ir_devices[IR_DEVICE_COUNT] = {
    {"Samsung",     ir_cmds_samsung_tv,     sizeof(ir_cmds_samsung_tv) / sizeof(ir_cmd_entry_t)},
    {"LG",          ir_cmds_lg_tv,          sizeof(ir_cmds_lg_tv) / sizeof(ir_cmd_entry_t)},
    {"Sony",        ir_cmds_sony_tv,        sizeof(ir_cmds_sony_tv) / sizeof(ir_cmd_entry_t)},
    {"Philips",     ir_cmds_philips_tv,     sizeof(ir_cmds_philips_tv) / sizeof(ir_cmd_entry_t)},
    {"Panasonic",   ir_cmds_panasonic_tv,   sizeof(ir_cmds_panasonic_tv) / sizeof(ir_cmd_entry_t)},
    {"Vizio",       ir_cmds_vizio_tv,       sizeof(ir_cmds_vizio_tv) / sizeof(ir_cmd_entry_t)},
    {"TCL/Roku",    ir_cmds_tcl_tv,         sizeof(ir_cmds_tcl_tv) / sizeof(ir_cmd_entry_t)},
    {"Hisense",     ir_cmds_hisense_tv,     sizeof(ir_cmds_hisense_tv) / sizeof(ir_cmd_entry_t)},
    {"Toshiba",     ir_cmds_toshiba_tv,     sizeof(ir_cmds_toshiba_tv) / sizeof(ir_cmd_entry_t)},
    {"Sharp",       ir_cmds_sharp_tv,       sizeof(ir_cmds_sharp_tv) / sizeof(ir_cmd_entry_t)},
    {"Insignia",    ir_cmds_insignia_tv,    sizeof(ir_cmds_insignia_tv) / sizeof(ir_cmd_entry_t)},
    {"Sanyo",       ir_cmds_sanyo_tv,       sizeof(ir_cmds_sanyo_tv) / sizeof(ir_cmd_entry_t)},
    {"Magnavox",    ir_cmds_magnavox_tv,    sizeof(ir_cmds_magnavox_tv) / sizeof(ir_cmd_entry_t)},
    {"Generic NEC", ir_cmds_generic_nec,    sizeof(ir_cmds_generic_nec) / sizeof(ir_cmd_entry_t)},
    {"Generic Alt", ir_cmds_generic_nec2,   sizeof(ir_cmds_generic_nec2) / sizeof(ir_cmd_entry_t)},
};

/* ---- Helper: draw the universal remote screen ---- */
static void uremote_draw(uint8_t dev_idx, uint8_t cmd_idx, uint8_t sending)
{
    const ir_device_t *dev = &ir_devices[dev_idx];
    char line[32];

    u8g2_FirstPage(&m1_u8g2);
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Header: device name with L/R arrows and index */
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    {
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "%d/%d %s", dev_idx + 1, IR_DEVICE_COUNT, dev->name);
        u8g2_DrawStr(&m1_u8g2, 8, 10, hdr);
    }
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 1, 10, "<");
    u8g2_DrawStr(&m1_u8g2, 122, 10, ">");

    /* Command list — show 3 visible items centered on selection */
    uint8_t visible_start = 0;
    if (cmd_idx > 1 && dev->cmd_count > 3)
        visible_start = (cmd_idx - 1 > dev->cmd_count - 3) ? dev->cmd_count - 3 : cmd_idx - 1;

    for (uint8_t i = 0; i < 3 && (visible_start + i) < dev->cmd_count; i++)
    {
        uint8_t ci = visible_start + i;
        uint8_t y = 24 + i * 12;

        if (ci == cmd_idx)
        {
            u8g2_DrawBox(&m1_u8g2, 0, y - 9, 128, 11);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
            u8g2_DrawStr(&m1_u8g2, 4, y, dev->cmds[ci].label);
            if (sending && ci == cmd_idx)
                u8g2_DrawStr(&m1_u8g2, 80, y, ">> TX");
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        }
        else
        {
            u8g2_DrawStr(&m1_u8g2, 4, y, dev->cmds[ci].label);
        }
    }

    /* Scroll indicator */
    if (dev->cmd_count > 3)
    {
        snprintf(line, sizeof(line), "%d/%d", cmd_idx + 1, dev->cmd_count);
        u8g2_DrawStr(&m1_u8g2, 100, 48, line);
    }

    /* Bottom bar */
    u8g2_DrawBox(&m1_u8g2, 0, 52, 128, 12);
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 61, "OK=Send  L/R=Device");
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    m1_u8g2_nextpage();
}
/* ==== Flipper .ir file support ==== */
#define IR_FILE_MAX_CMDS    32
#define IR_FILE_LABEL_LEN   24
#define IR_FILE_PATH        "/infrared"
#define IR_FILE_LINE_MAX    128

typedef struct {
    char     label[IR_FILE_LABEL_LEN];
    uint8_t  protocol;
    uint16_t address;
    uint16_t command;
} ir_file_cmd_t;

static ir_file_cmd_t ir_file_cmds[IR_FILE_MAX_CMDS];
static uint8_t ir_file_cmd_count = 0;
static char ir_file_name[64];

/* Map Flipper protocol name to IRMP protocol number */
static uint8_t ir_map_protocol(const char *name)
{
    if (strstr(name, "Samsung48"))  return IRMP_SAMSUNG48_PROTOCOL;
    if (strstr(name, "Samsung32"))  return IRMP_SAMSUNG32_PROTOCOL;
    if (strstr(name, "NEC42"))      return IRMP_NEC42_PROTOCOL;
    if (strstr(name, "NECext"))     return IRMP_NEC_PROTOCOL;
    if (strstr(name, "NEC"))        return IRMP_NEC_PROTOCOL;
    if (strstr(name, "RC6"))        return IRMP_RC6_PROTOCOL;
    if (strstr(name, "RC5"))        return IRMP_RC5_PROTOCOL;
    if (strstr(name, "SIRC"))       return IRMP_SIRCS_PROTOCOL;
    if (strstr(name, "Sony"))       return IRMP_SIRCS_PROTOCOL;
    if (strstr(name, "Kaseikyo"))   return IRMP_KASEIKYO_PROTOCOL;
    if (strstr(name, "Panasonic"))  return IRMP_KASEIKYO_PROTOCOL;
    if (strstr(name, "Denon"))      return IRMP_DENON_PROTOCOL;
    if (strstr(name, "Sharp"))      return IRMP_DENON_PROTOCOL;
    if (strstr(name, "JVC"))        return IRMP_JVC_PROTOCOL;
    if (strstr(name, "BOSE"))       return IRMP_BOSE_PROTOCOL;
    if (strstr(name, "LG"))         return IRMP_LGAIR_PROTOCOL;
    return 0xFF; /* unknown */
}

/* Parse Flipper 4-byte hex "07 00 00 00" to uint16_t (LSB first) */
static uint16_t ir_parse_flipper_hex(const char *hex_str)
{
    unsigned int b0 = 0, b1 = 0;
    const char *p = hex_str;
    /* Skip leading whitespace */
    while (*p == ' ') p++;
    /* Parse first byte */
    b0 = 0;
    while ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
    {
        b0 <<= 4;
        if (*p >= '0' && *p <= '9') b0 |= (*p - '0');
        else if (*p >= 'A' && *p <= 'F') b0 |= (*p - 'A' + 10);
        else if (*p >= 'a' && *p <= 'f') b0 |= (*p - 'a' + 10);
        p++;
    }
    /* Skip space */
    while (*p == ' ') p++;
    /* Parse second byte */
    b1 = 0;
    while ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
    {
        b1 <<= 4;
        if (*p >= '0' && *p <= '9') b1 |= (*p - '0');
        else if (*p >= 'A' && *p <= 'F') b1 |= (*p - 'A' + 10);
        else if (*p >= 'a' && *p <= 'f') b1 |= (*p - 'a' + 10);
        p++;
    }
    return (uint16_t)(b0 | (b1 << 8));
}

/* Parse a Flipper .ir file and fill ir_file_cmds[] */
static uint8_t ir_parse_file(const S_M1_file_info *f_info)
{
    FIL fil;
    char line[IR_FILE_LINE_MAX];
    ir_file_cmd_count = 0;

    char fullpath[128];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", f_info->dir_name, f_info->file_name);

    FRESULT fres = f_open(&fil, fullpath, FA_READ);
    if (fres != FR_OK)
    {
        /* Try without extra slash */
        snprintf(fullpath, sizeof(fullpath), "%s%s", f_info->dir_name, f_info->file_name);
        fres = f_open(&fil, fullpath, FA_READ);
        if (fres != FR_OK)
            return 0;
    }

    char cur_name[IR_FILE_LABEL_LEN] = "";
    char cur_protocol[32] = "";
    char cur_address[32] = "";
    char cur_command[32] = "";
    uint8_t cur_type_parsed = 0;
    uint8_t have_command = 0;

    while (f_gets(line, sizeof(line), &fil) != NULL)
    {
        /* Trim trailing whitespace/CR/LF */
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' || line[len-1] == ' ' || line[len-1] == '\t'))
            line[--len] = '\0';

        /* Skip empty lines and comments */
        if (len == 0) continue;
        if (line[0] == '#') continue;
        if (strncmp(line, "Filetype:", 9) == 0) continue;
        if (strncmp(line, "Version:", 8) == 0) continue;

        if (strncmp(line, "name: ", 6) == 0)
        {
            /* Save previous command if complete */
            if (have_command && cur_type_parsed && cur_protocol[0])
            {
                uint8_t proto = ir_map_protocol(cur_protocol);
                if (proto != 0xFF && ir_file_cmd_count < IR_FILE_MAX_CMDS)
                {
                    ir_file_cmd_t *cmd = &ir_file_cmds[ir_file_cmd_count];
                    strncpy(cmd->label, cur_name, IR_FILE_LABEL_LEN - 1);
                    cmd->label[IR_FILE_LABEL_LEN - 1] = '\0';
                    cmd->protocol = proto;
                    cmd->address = ir_parse_flipper_hex(cur_address);
                    cmd->command = ir_parse_flipper_hex(cur_command);
                    ir_file_cmd_count++;
                }
            }
            /* Start new command */
            strncpy(cur_name, &line[6], IR_FILE_LABEL_LEN - 1);
            cur_name[IR_FILE_LABEL_LEN - 1] = '\0';
            cur_protocol[0] = '\0';
            cur_address[0] = '\0';
            cur_command[0] = '\0';
            cur_type_parsed = 0;
            have_command = 1;
        }
        else if (strncmp(line, "type: parsed", 12) == 0)
        {
            cur_type_parsed = 1;
        }
        else if (strncmp(line, "type: raw", 9) == 0)
        {
            cur_type_parsed = 0;
        }
        else if (strncmp(line, "protocol: ", 10) == 0)
        {
            strncpy(cur_protocol, &line[10], sizeof(cur_protocol) - 1);
            cur_protocol[sizeof(cur_protocol) - 1] = '\0';
        }
        else if (strncmp(line, "address: ", 9) == 0)
        {
            strncpy(cur_address, &line[9], sizeof(cur_address) - 1);
            cur_address[sizeof(cur_address) - 1] = '\0';
        }
        else if (strncmp(line, "command: ", 9) == 0)
        {
            strncpy(cur_command, &line[9], sizeof(cur_command) - 1);
            cur_command[sizeof(cur_command) - 1] = '\0';
        }
    }

    /* Flush last command */
    if (have_command && cur_type_parsed && cur_protocol[0])
    {
        uint8_t proto = ir_map_protocol(cur_protocol);
        if (proto != 0xFF && ir_file_cmd_count < IR_FILE_MAX_CMDS)
        {
            ir_file_cmd_t *cmd = &ir_file_cmds[ir_file_cmd_count];
            strncpy(cmd->label, cur_name, IR_FILE_LABEL_LEN - 1);
            cmd->label[IR_FILE_LABEL_LEN - 1] = '\0';
            cmd->protocol = proto;
            cmd->address = ir_parse_flipper_hex(cur_address);
            cmd->command = ir_parse_flipper_hex(cur_command);
            ir_file_cmd_count++;
        }
    }

    f_close(&fil);
    return ir_file_cmd_count;
}

/* Draw the .ir file command browser */
static void ir_file_draw(uint8_t cmd_idx, uint8_t sending)
{
    char line[32];

    u8g2_FirstPage(&m1_u8g2);
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Header: filename */
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 1, 10, ir_file_name);

    /* Command list — 3 visible */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    uint8_t visible_start = 0;
    if (cmd_idx > 1 && ir_file_cmd_count > 3)
        visible_start = (cmd_idx - 1 > ir_file_cmd_count - 3) ? ir_file_cmd_count - 3 : cmd_idx - 1;

    for (uint8_t i = 0; i < 3 && (visible_start + i) < ir_file_cmd_count; i++)
    {
        uint8_t ci = visible_start + i;
        uint8_t y = 24 + i * 12;

        if (ci == cmd_idx)
        {
            u8g2_DrawBox(&m1_u8g2, 0, y - 9, 128, 11);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
            u8g2_DrawStr(&m1_u8g2, 4, y, ir_file_cmds[ci].label);
            if (sending)
                u8g2_DrawStr(&m1_u8g2, 88, y, ">> TX");
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        }
        else
        {
            u8g2_DrawStr(&m1_u8g2, 4, y, ir_file_cmds[ci].label);
        }
    }

    if (ir_file_cmd_count > 3)
    {
        snprintf(line, sizeof(line), "%d/%d", cmd_idx + 1, ir_file_cmd_count);
        u8g2_DrawStr(&m1_u8g2, 100, 48, line);
    }

    u8g2_DrawBox(&m1_u8g2, 0, 52, 128, 12);
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 61, "OK=Send  Back=Exit");
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    m1_u8g2_nextpage();
}
/* Run the .ir file remote */
static void ir_file_remote_run(const S_M1_file_info *f_info)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    uint8_t cmd_idx = 0;
    uint8_t ir_tx_active = 0;

    /* Parse the file */
    uint8_t count = ir_parse_file(f_info);
    if (count == 0)
    {
        u8g2_FirstPage(&m1_u8g2);
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
        u8g2_DrawStr(&m1_u8g2, 4, 15, "No commands found");
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 4, 30, f_info->file_name);
        m1_u8g2_nextpage();
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    /* Extract filename for display */
    strncpy(ir_file_name, f_info->file_name, sizeof(ir_file_name) - 1);
    ir_file_name[sizeof(ir_file_name) - 1] = '\0';
    /* Remove extension */
    char *dot = strrchr(ir_file_name, '.');
    if (dot) *dot = '\0';

    ir_file_draw(cmd_idx, 0);

    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE) continue;

        if (q_item.q_evt_type == Q_EVENT_KEYPAD)
        {
            ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
            if (ret != pdTRUE) continue;

            if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                if (ir_tx_active)
                {
                    if (ir_ota_data_tx_active)
                        m1_ir_ota_frame_post_process(0xFF);
                    infrared_encode_sys_deinit();
                    ir_tx_active = 0;
                }
                xQueueReset(main_q_hdl);
               /* Restore orientation */

                return;
            }

            if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
            {
                cmd_idx = (cmd_idx == 0) ? (ir_file_cmd_count - 1) : (cmd_idx - 1);
                ir_file_draw(cmd_idx, 0);
            }

            if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
            {
                cmd_idx = (cmd_idx + 1) % ir_file_cmd_count;
                ir_file_draw(cmd_idx, 0);
            }

            if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                ir_file_cmd_t *cmd = &ir_file_cmds[cmd_idx];

                ir_file_draw(cmd_idx, 1);

                irmp_data.protocol = cmd->protocol;
                irmp_data.address  = cmd->address;
                irmp_data.command  = cmd->command;
                irmp_data.flags    = 1;

                if (!ir_tx_active)
                {
                    infrared_encode_sys_init();
                    ir_tx_active = 1;
                }
                else
                {
                    irsnd_init(&Timerhdl_IrCarrier, IR_ENCODE_TIMER_TX_CHANNEL);
                    vTaskDelay(20);
                }

                irsnd_generate_tx_data(irmp_data);
                infrared_transmit(1);

                m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);
                m1_buzzer_notification();

                while (1)
                {
                    S_M1_IR_Tx_States tx_state = infrared_transmit(0);
                    if (tx_state == IR_TX_COMPLETED)
                    {
                        m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);
                        break;
                    }
                    if (tx_state == IR_TX_ACTIVE)
                    {
                        ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(2000));
                        if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_IRRED_TX)
                            continue;
                        if (ret != pdTRUE)
                        {
                            m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);
                            break;
                        }
                    }
                    else
                    {
                        ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
                        if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_IRRED_TX)
                            continue;
                        break;
                    }
                }

                ir_file_draw(cmd_idx, 0);
            }
        }
        else if (q_item.q_evt_type == Q_EVENT_IRRED_TX)
        {
            /* Stale TX completion */
        }
    }
}
static void ir_builtin_remote_run(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    uint8_t dev_idx = 0;
    uint8_t cmd_idx = 0;
    uint8_t needs_redraw = 1;
    uint8_t ir_tx_active = 0;

    while (1)
    {
        if (needs_redraw)
        {
            needs_redraw = 0;
            uremote_draw(dev_idx, cmd_idx, 0);
        }

        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE) continue;

        if (q_item.q_evt_type == Q_EVENT_KEYPAD)
        {
            ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
            if (ret != pdTRUE) continue;

            if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                if (ir_tx_active)
                {
                    if (ir_ota_data_tx_active)
                        m1_ir_ota_frame_post_process(0xFF);
                    infrared_encode_sys_deinit();
                    ir_tx_active = 0;
                }
                xQueueReset(main_q_hdl);
                break;
            }

            if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
            {
                const ir_device_t *dev = &ir_devices[dev_idx];
                cmd_idx = (cmd_idx == 0) ? (dev->cmd_count - 1) : (cmd_idx - 1);
                needs_redraw = 1;
            }

            if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
            {
                const ir_device_t *dev = &ir_devices[dev_idx];
                cmd_idx = (cmd_idx + 1) % dev->cmd_count;
                needs_redraw = 1;
            }

            if (this_button_status.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
            {
                dev_idx = (dev_idx == 0) ? (IR_DEVICE_COUNT - 1) : (dev_idx - 1);
                cmd_idx = 0;
                needs_redraw = 1;
            }

            if (this_button_status.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
            {
                dev_idx = (dev_idx + 1) % IR_DEVICE_COUNT;
                cmd_idx = 0;
                needs_redraw = 1;
            }

            if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                const ir_device_t *dev = &ir_devices[dev_idx];
                const ir_cmd_entry_t *cmd = &dev->cmds[cmd_idx];

                /* Show TX indicator */
                uremote_draw(dev_idx, cmd_idx, 1);

                /* Set up IRMP data */
                irmp_data.protocol = cmd->protocol;
                irmp_data.address  = cmd->address;
                irmp_data.command  = cmd->command;
                irmp_data.flags    = 1; /* don't repeat */

                /* Init encoder and transmit */
                if (!ir_tx_active)
                {
                    infrared_encode_sys_init();
                    ir_tx_active = 1;
                }
                else
                {
                    /* Re-init carrier for subsequent transmits */
                    irsnd_init(&Timerhdl_IrCarrier, IR_ENCODE_TIMER_TX_CHANNEL);
                    vTaskDelay(20);
                }

                irsnd_generate_tx_data(irmp_data);
                infrared_transmit(1); /* initialize tx */

                m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);
                m1_buzzer_notification();

                /* Drive transmit to completion (non-blocking loop) */
                while (1)
                {
                    S_M1_IR_Tx_States tx_state = infrared_transmit(0);
                    if (tx_state == IR_TX_COMPLETED)
                    {
                        m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);
                        break;
                    }
                    if (tx_state == IR_TX_ACTIVE)
                    {
                        /* Wait for TX event or button */
                        ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(2000));
                        if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_IRRED_TX)
                        {
                            continue; /* Process next frame */
                        }
                        if (ret != pdTRUE)
                        {
                            /* Timeout — abort */
                            m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);
                            break;
                        }
                    }
                    else
                    {
                        /* IR_TX_INIT etc — just loop */
                        ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
                        if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_IRRED_TX)
                            continue;
                        break;
                    }
                }

                needs_redraw = 1;
            }
        }
        else if (q_item.q_evt_type == Q_EVENT_IRRED_TX)
        {
            /* Stale TX completion — ignore */
        }
    }

} // static void ir_builtin_remote_run(void)

void infrared_universal_remotes(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    uint8_t sel = 0;
    uint8_t needs_redraw = 1;

    static const char *ur_menu[3];
    ur_menu[0] = "Built-in Remotes";
    ur_menu[1] = "Load .ir File";
    ur_menu[2] = (m1_screen_orientation == M1_ORIENT_REMOTE) ? "Normal Mode" : "Remote Mode";

    while (1)
    {
        if (needs_redraw)
        {
            needs_redraw = 0;
            ur_menu[2] = (m1_screen_orientation == M1_ORIENT_REMOTE) ? "Normal Mode" : "Remote Mode";
            u8g2_FirstPage(&m1_u8g2);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 1, 10, "Universal Remote");
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            for (uint8_t i = 0; i < 3; i++)
            {
                uint8_t y = 26 + i * 14;
                if (i == sel) {
                    u8g2_DrawBox(&m1_u8g2, 0, y - 10, 128, 12);
                    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
                    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
                    u8g2_DrawStr(&m1_u8g2, 4, y, ur_menu[i]);
                    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
                    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
                } else {
                    u8g2_DrawStr(&m1_u8g2, 4, y, ur_menu[i]);
                }
            }
            m1_u8g2_nextpage();
        }

        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE) continue;
        if (q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

        ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
        if (ret != pdTRUE) continue;

        if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            xQueueReset(main_q_hdl);
            break;
        }
        if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = (sel == 0) ? 2 : (sel - 1);
            needs_redraw = 1;
        }
        if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = (sel + 1) % 3;
            needs_redraw = 1;
        }
        if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (sel == 0)
            {
                /* Built-in remotes */
                ir_builtin_remote_run();
                needs_redraw = 1;
            }
            else if (sel == 1)
            {
                /* Temporarily switch to normal for file browser */
                uint8_t browse_orient = m1_screen_orientation;
                if (browse_orient != M1_ORIENT_NORMAL)
                {
                    m1_screen_orientation = M1_ORIENT_NORMAL;
                    u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R2);
                }
                /* Load .ir file from SD */
                S_M1_file_info *f_info = storage_browse();
                if (f_info && f_info->file_is_selected)
                {
                    char filepath[128];
                    int dlen = strlen(f_info->dir_name);
                    if (dlen > 0 && f_info->dir_name[dlen-1] == '/')
                        snprintf(filepath, sizeof(filepath), "%s%s", f_info->dir_name, f_info->file_name);
                    else
                        snprintf(filepath, sizeof(filepath), "%s/%s", f_info->dir_name, f_info->file_name);
                    /* Restore orientation before running file remote */
                    m1_screen_orientation = browse_orient;
                    if (browse_orient == M1_ORIENT_REMOTE) u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R1);
                    else if (browse_orient == M1_ORIENT_SOUTHPAW) u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R0);
                    ir_file_remote_run(f_info);
                }
                needs_redraw = 1;
            }
            else if (sel == 2)
            {
                /* Toggle Remote Mode orientation */
                if (m1_screen_orientation == M1_ORIENT_REMOTE)
                {
                    m1_screen_orientation = M1_ORIENT_NORMAL;
                    u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R2);
                }
                else
                {
                    m1_screen_orientation = M1_ORIENT_REMOTE;
                    u8g2_SetDisplayRotation(&m1_u8g2, U8G2_R1);
                }
                needs_redraw = 1;
            }
        }
    }
} // void infrared_universal_remotes(void)

/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void infrared_learn_new_remote(void)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint8_t ir_data[20];

	infrared_decode_sys_init();
	irmp_init();

	m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);

	/* Graphic work starts here */
	u8g2_FirstPage(&m1_u8g2);
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawXBMP(&m1_u8g2, 2, 2, 48, 25, remote_48x25);
	u8g2_DrawStr(&m1_u8g2, 60, 20, "Reading...");
	m1_u8g2_nextpage(); // Update display RAM

	while (1) // Main loop of this task
	{
		;
		; // Do other parts of this task here
		;

		// Wait for the notification from button_event_handler_task to subfunc_handler_task.
		// This task is the sub-task of subfunc_handler_task.
		// The notification is given in the form of an item in the main queue.
		// So let read the main queue.
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret==pdTRUE)
		{
			if ( q_item.q_evt_type==Q_EVENT_IRRED_RX )
			{
				irmp_data_sampler(q_item.q_data.ir_rx_data.ir_edge_te, q_item.q_data.ir_rx_data.ir_edge_dir);
				/* Decode the Rx frame */
				if (irmp_get_data(&irmp_data))
				{
					m1_buzzer_notification();
					u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
					u8g2_DrawBox(&m1_u8g2, 0, 30, 128, 34); // Clear old content
					u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
					u8g2_DrawStr(&m1_u8g2, 15, 40, irmp_protocol_names[irmp_data.protocol]);
					sprintf(ir_data, "Address: 0x%04X", irmp_data.address);
					u8g2_DrawStr(&m1_u8g2, 15, 50, ir_data);
					sprintf(ir_data, "Command: 0x%04X", irmp_data.command);
					u8g2_DrawStr(&m1_u8g2, 15, 60, ir_data);
					u8g2_NextPage(&m1_u8g2); // Update display RAM

					memcpy(&irmp_loopback_data, &irmp_data, sizeof(IRMP_DATA));
					new_remote_learned = 1;

				} // if (irmp_get_data (&irmp_data))
			} // if ( q_item.q_evt_type==Q_EVENT_IRRED_RX )
			else if ( q_item.q_evt_type==Q_EVENT_KEYPAD )
			{
				// Notification is only sent to this task when there's any button activity,
				// so it doesn't need to wait when reading the event from the queue
				ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
				if ( this_button_status.event[BUTTON_BACK_KP_ID]==BUTTON_EVENT_CLICK ) // user wants to exit?
				{
					m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF); // Turn off

					; // Do extra tasks here if needed
					infrared_decode_sys_deinit();

					xQueueReset(main_q_hdl); // Reset main q before return
					break; // Exit and return to the calling task (subfunc_handler_task)
				} // if ( m1_buttons_status[BUTTON_BACK_KP_ID]==BUTTON_EVENT_CLICK )
				else
				{
					; // Do other things for this task, if needed
				}
			} // else if ( q_item.q_evt_type==Q_EVENT_KEYPAD )
			else
			{
				; // Do other things for this task
			}
		} // if (ret==pdTRUE)
	} // while (1 ) // Main loop of this task
} // void infrared_learn_new_remote(void)



/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void infrared_saved_remotes(void)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint8_t ir_tx_complete, ir_data[20];

	// m1_gui_let_update_fw();

	/* Graphic work starts here */
	u8g2_FirstPage(&m1_u8g2);
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	u8g2_DrawXBMP(&m1_u8g2, 2, 2, 48, 25, remote_48x25);
	u8g2_DrawStr(&m1_u8g2, 60, 20, "Sending...");
	m1_u8g2_nextpage(); // Update display RAM

	if (new_remote_learned)
	{
    	memcpy(&irmp_data, &irmp_loopback_data, sizeof(IRMP_DATA));
    	irmp_data.flags    = 1;
    }
	else
	{
		return;
		irmp_data.protocol = IRMP_NEC_PROTOCOL;//IRMP_RC5_PROTOCOL;//IRMP_NEC_PROTOCOL; // use NEC protocol
		irmp_data.address  = 0xFD02; // set address to 0x00FF
		irmp_data.command  = 0x005C; // set command to 0x0001
		irmp_data.flags    = 1; // don't repeat frame
    }

	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
	u8g2_DrawBox(&m1_u8g2, 0, 30, 128, 34); // Clear old content
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	u8g2_DrawStr(&m1_u8g2, 15, 40, irmp_protocol_names[irmp_data.protocol]);
	sprintf(ir_data, "Address: 0x%04X", irmp_data.address);
	u8g2_DrawStr(&m1_u8g2, 15, 50, ir_data);
	sprintf(ir_data, "Command: 0x%04X", irmp_data.command);
	u8g2_DrawStr(&m1_u8g2, 15, 60, ir_data);
	u8g2_NextPage(&m1_u8g2); // Update display RAM

	m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);
    infrared_encode_sys_init();
	irsnd_generate_tx_data(irmp_data); // make ota data
	infrared_transmit(1); // initialize the tx
	ir_tx_complete = 0;

	while (1 ) // Main loop of this task
	{
		;
		; // Do other parts of this task here
		;
		infrared_transmit(0);

		// Wait for the notification from button_event_handler_task to subfunc_handler_task.
		// This task is the sub-task of subfunc_handler_task.
		// The notification is given in the form of an item in the main queue.
		// So let read the main queue.
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret==pdTRUE)
		{
			if ( q_item.q_evt_type==Q_EVENT_KEYPAD )
			{
				// Notification is only sent to this task when there's any button activity,
				// so it doesn't need to wait when reading the event from the queue
				ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
				if ( this_button_status.event[BUTTON_BACK_KP_ID]==BUTTON_EVENT_CLICK ) // user wants to exit?
				{
					m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF); // Turn off

					; // Do extra tasks here if needed
					if ( ir_ota_data_tx_active ) // Tx not completed?
					{
						m1_ir_ota_frame_post_process(0xFF); // Reset
					} // if ( !ir_ota_data_tx_active )

					infrared_encode_sys_deinit();

					xQueueReset(main_q_hdl); // Reset main q before return
					break; // Exit and return to the calling task (subfunc_handler_task)
				} // if ( m1_buttons_status[BUTTON_BACK_KP_ID]==BUTTON_EVENT_CLICK )
				else if (this_button_status.event[BUTTON_OK_KP_ID]==BUTTON_EVENT_CLICK)
				{
					; // Re-Send
					if ( !ir_tx_complete )
						continue;

					m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);
					irsnd_init(&Timerhdl_IrCarrier, IR_ENCODE_TIMER_TX_CHANNEL);
					vTaskDelay(20); // A delay here is necessary for the function to work properly!
					irsnd_generate_tx_data(irmp_data); // make ota data
					infrared_transmit(1); // initialize the tx
					ir_tx_complete = 0;
				}
				else
				{
					; // Do other things for this task, if needed
				}
			} // if ( q_item.q_evt_type==Q_EVENT_KEYPAD )
			else if ( q_item.q_evt_type==Q_EVENT_IRRED_TX ) // Transmit completed?
			{
				m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF); // Turn off
				ir_tx_complete = 1;
			}
		} // if (ret==pdTRUE)
	} // while (1 ) // Main loop of this task

} // void infrared_saved_remotes(void)



/*============================================================================*/
/*
  * @brief  Initialize the decoder module
  * Input capture mode for both rising and falling edges, given in TIMx_CCR1 for each edge capture
  * @param  None
  * @retval None
 */
/*============================================================================*/
static void infrared_decode_sys_init(void)
{
	GPIO_InitTypeDef gpio_init_struct;
	TIM_IC_InitTypeDef tim_ic_init = {0};
	TIM_MasterConfigTypeDef tim_master_conf = {0};
	uint32_t tim_prescaler_val;

	/* Pin configuration: input floating */
	gpio_init_struct.Pin = IR_RX_GPIO_PIN;
	gpio_init_struct.Mode = GPIO_MODE_AF_OD;
	gpio_init_struct.Pull = GPIO_NOPULL;
	gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
	gpio_init_struct.Alternate = IR_GPIO_AF_RX;
	HAL_GPIO_Init(IR_GPIO_PORT, &gpio_init_struct);

	/*  Clock Configuration for TIMER */
	IR_DECODE_TIMER_CLK();

	/* Timer Clock */
	tim_prescaler_val = (uint32_t) (HAL_RCC_GetPCLK2Freq() / 1000000) - 1; // 1MHz

	Timerhdl_IrRx.Instance = IR_DECODE_TIMER;

	Timerhdl_IrRx.Init.ClockDivision = 0;
	Timerhdl_IrRx.Init.CounterMode = TIM_COUNTERMODE_UP;
	Timerhdl_IrRx.Init.Period = IRMP_TIMEOUT_TIME;
	Timerhdl_IrRx.Init.Prescaler = tim_prescaler_val;
	Timerhdl_IrRx.Init.RepetitionCounter = 0;
	Timerhdl_IrRx.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_IC_Init(&Timerhdl_IrRx) != HAL_OK)
	{
		//Error_Handler(__FILE__, __LINE__);
		Error_Handler();
	}
/*
  Input capture mode
  In input capture mode, the capture/compare registers (TIMx_CCRx) are used to latch the value of the counter after a transition detected by the corresponding ICx signal. When a
  capture occurs, the corresponding CCXIF flag (TIMx_SR register) is set and an interrupt or a DMA request can be sent if they are enabled. If a capture occurs while the CCxIF flag was
  already high, then the overcapture flag CCxOF (TIMx_SR register) is set. CCxIF can be
  cleared by software by writing it to 0 or by reading the captured data stored in the
  TIMx_CCRx register. CCxOF is cleared when it is written with 0.
  The following example shows how to capture the counter value in TIMx_CCR1 when tim_ti1
  input rises. To do this, use the following procedure:
  1. Select the proper tim_tix_in[15:0] source (internal or external) with the TI1SEL[3:0] bits in the TIMx_TISEL register.
  2. Select the active input: TIMx_CCR1 must be linked to the tim_ti1 input, so write the
  CC1S bits to 01 in the TIMx_CCMR1 register. As soon as CC1S becomes different
  from 00, the channel is configured in input and the TIMx_CCR1 register becomes read-
  only.
  3. Program the needed input filter duration in relation with the signal connected to the
  timer (when the input is one of the tim_tix (ICxF bits in the TIMx_CCMRx register). Let’s
  imagine that, when toggling, the input signal is not stable during at most five internal clock cycles. We must program a filter duration longer than these five clock cycles. We can validate a transition on tim_ti1 when eight consecutive samples with the new level
  have been detected (sampled at fDTS frequency). Then write IC1F bits to 0011 in the
  TIMx_CCMR1 register.
  4. Select the edge of the active transition on the tim_ti1 channel by writing the CC1P and CC1NP bits to 000 in the TIMx_CCER register (rising edge in this case).
  5. Program the input prescaler. In this example, the capture is to be performed at each
  valid transition, so the prescaler is disabled (write IC1PS bits to 00 in the TIMx_CCMR1 register).
  6. Enable capture from the counter into the capture register by setting the CC1E bit in the TIMx_CCER register.
  7. If needed, enable the related interrupt request by setting the CC1IE bit in the
  TIMx_DIER register, and/or the DMA request by setting the CC1DE bit in the TIMx_DIER register.
  When an input capture occurs:
  • The TIMx_CCR1 register gets the value of the counter on the active transition.
  • CC1IF flag is set (interrupt flag). CC1OF is also set if at least two consecutive captures occurred whereas the flag was not cleared.
  • An interrupt is generated depending on the CC1IE bit.
  • A DMA request is generated depending on the CC1DE bit.
  In order to handle the overcapture, it is recommended to read the data before the overcapture flag. This is to avoid missing an overcapture which may happen after reading
  the flag and before reading the data.
  Note: IC interrupt and/or DMA requests can be generated by software by setting the
  corresponding CCxG bit in the TIMx_EGR register.
*/
  /* Enable the Master/Slave Mode */
  /* SMS = 1000:Combined reset + trigger mode - Rising edge of the selected trigger input (tim_trgi)
   * reinitializes the counter, generates an update of the registers and starts the counter.
   * SMS = 0100: Reset mode - Rising edge of the selected trigger input (tim_trgi)
   * reinitializes the counter and generates an update of the registers. -Recommended by Reference Manual
   */
	tim_master_conf.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;//TIM_SMCR_MSM;
	tim_master_conf.MasterOutputTrigger = TIM_TRGO_RESET;
	if (HAL_TIMEx_MasterConfigSynchronization( &Timerhdl_IrRx, &tim_master_conf) != HAL_OK)
	{
		//_Error_Handler(__FILE__, __LINE__);
		Error_Handler();
	}

	tim_ic_init.ICPolarity = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;
	tim_ic_init.ICSelection = TIM_ICSELECTION_DIRECTTI;
	tim_ic_init.ICPrescaler = TIM_ICPSC_DIV1;
	tim_ic_init.ICFilter = 0;
	if (HAL_TIM_IC_ConfigChannel(&Timerhdl_IrRx, &tim_ic_init, IR_DECODE_TIMER_RX_CHANNEL) != HAL_OK)
	{
		//_Error_Handler(__FILE__, __LINE__);
		Error_Handler();
	}

	/* Enable the TIMx global Interrupt */
	HAL_NVIC_SetPriority(IR_DECODE_TIMER_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0);
	HAL_NVIC_EnableIRQ(IR_DECODE_TIMER_IRQn);

	/* Configures the TIM Update Request Interrupt source: counter overflow */
	__HAL_TIM_URS_ENABLE(&Timerhdl_IrRx);

	/* Clear update flag */
	__HAL_TIM_CLEAR_FLAG( &Timerhdl_IrRx, TIM_FLAG_UPDATE);

	/* Enable TIM Update Event Interrupt Request */
	/* Enable the CCx/CCy Interrupt Request */
	__HAL_TIM_ENABLE_IT( &Timerhdl_IrRx, TIM_FLAG_UPDATE);

	/* Enable the timer */
	//__HAL_TIM_ENABLE(&Timerhdl_IrRx);

	if (HAL_TIM_IC_Start_IT(&Timerhdl_IrRx, IR_DECODE_TIMER_RX_CHANNEL) != HAL_OK)
	{
		//_Error_Handler(__FILE__, __LINE__);
		Error_Handler();
	}

	IrRx_Edge_Det = EDGE_DET_IDLE;
} // static void infrared_decode_sys_init(void)


/*============================================================================*/
/**
  * @brief  De-initializes the peripherals (RCC,GPIO, TIM)
  * @param  None
  * @retval None
  */
/*============================================================================*/
static void infrared_decode_sys_deinit(void)
{
	HAL_TIM_IC_DeInit(&Timerhdl_IrRx);

	IR_DECODE_TIMER_CLK_DIS();
	HAL_NVIC_DisableIRQ(IR_DECODE_TIMER_IRQn);

	HAL_GPIO_DeInit(IR_GPIO_PORT, IR_RX_GPIO_PIN);

	if ( main_q_hdl != NULL)
		xQueueReset(main_q_hdl);
} // static void infrared_decode_sys_deinit(void)



/*============================================================================*/
/*
  * @brief  Initialize the encoder module
  * Output modulated (PWM carrier + base band) data on Ir Tx GPIO pin
  * @param  None
  * @retval None
 */
/*============================================================================*/
void infrared_encode_sys_init(void)
{
	//TIM_OC_InitTypeDef ch_config;
	TIM_MasterConfigTypeDef sMasterConfig;
	GPIO_InitTypeDef gpio_init_struct;
	uint32_t tim_prescaler_val;

	IR_ENCODE_CARRIER_TIMER_CLK();
	IR_ENCODE_BASEBAND_TIMER_CLK();

	Timerhdl_IrTx.Instance = IR_ENCODE_BASEBAND_TIMER;
	Timerhdl_IrCarrier.Instance = IR_ENCODE_CARRIER_TIMER;

	/*Configure GPIO pin */
	gpio_init_struct.Pin = IR_TX_GPIO_PIN;
	gpio_init_struct.Mode = GPIO_MODE_AF_PP;
	gpio_init_struct.Pull = GPIO_NOPULL;
	gpio_init_struct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	gpio_init_struct.Alternate = IR_GPIO_AF_TR;
	HAL_GPIO_Init(IR_GPIO_PORT, &gpio_init_struct);

	irsnd_init(&Timerhdl_IrCarrier, IR_ENCODE_TIMER_TX_CHANNEL);
/*
	// HAL_TIM_PWM_DeInit(&Timerhdl_IrCarrier);

	tim_prescaler_val = (uint32_t) (HAL_RCC_GetPCLK2Freq() / (IR_ENCODE_CARRIER_FREQ_36_KHZ*IR_ENCODE_CARRIER_PRESCALE_FACTOR)) - 1;

	// Time base configuration for timer x
	Timerhdl_IrCarrier.Init.Prescaler = tim_prescaler_val;
	Timerhdl_IrCarrier.Init.CounterMode = TIM_COUNTERMODE_UP;
	Timerhdl_IrCarrier.Init.Period = IR_ENCODE_CARRIER_PRESCALE_FACTOR - 1;
	Timerhdl_IrCarrier.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	Timerhdl_IrCarrier.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_PWM_Init(&Timerhdl_IrCarrier) != HAL_OK)
	{
		//_Error_Handler(__FILE__, __LINE__);
		Error_Handler();
	}

	// Output Compare Timing Mode configuration: Channel 4N
	ch_config.OCMode = TIM_OCMODE_PWM1;
	ch_config.Pulse = Timerhdl_IrCarrier.Init.Period / 4; // Duty cycle = 25%
	ch_config.OCPolarity = TIM_OCPOLARITY_HIGH;
	ch_config.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	ch_config.OCFastMode = TIM_OCFAST_DISABLE;
	ch_config.OCIdleState = TIM_OCIDLESTATE_RESET;
	ch_config.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	if (HAL_TIM_PWM_ConfigChannel(&Timerhdl_IrCarrier, &ch_config, IR_ENCODE_TIMER_TX_CHANNEL) != HAL_OK)
	{
		//_Error_Handler(__FILE__, __LINE__);
		Error_Handler();
	}

	//TIM_SET_CAPTUREPOLARITY(&Timerhdl_IrCarrier, IR_ENCODE_TIMER_TX_CHANNEL, TIM_CCxN_ENABLE | TIM_CCx_ENABLE );

	//HAL_TIM_PWM_Start(&Timerhdl_IrCarrier, IR_ENCODE_TIMER_TX_CHANNEL);
*/

	// DeInit TIMx
	//HAL_TIM_Base_DeInit(&Timerhdl_IrTx);

	tim_prescaler_val = (uint32_t) (HAL_RCC_GetPCLK2Freq() / 1000000) - 1; // 1MHz

	// Time Base configuration for timer x
	Timerhdl_IrTx.Init.Prescaler = tim_prescaler_val;
	Timerhdl_IrTx.Init.CounterMode = TIM_COUNTERMODE_UP;
	Timerhdl_IrTx.Init.Period = 0xFFFF; // Temporary value, it will be updated with valid value later
	Timerhdl_IrTx.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	Timerhdl_IrTx.Init.RepetitionCounter = 0;
	Timerhdl_IrTx.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&Timerhdl_IrTx) != HAL_OK)
	{
		//_Error_Handler(__FILE__, __LINE__);
		Error_Handler();
	}

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&Timerhdl_IrTx, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}

	/* Configures the TIM Update Request Interrupt source: counter overflow */
	//__HAL_TIM_URS_ENABLE(&Timerhdl_IrTx);

	/* Clear update flag */
	__HAL_TIM_CLEAR_FLAG( &Timerhdl_IrTx, TIM_FLAG_UPDATE);

	/* Enable TIM Update Event Interrupt Request */
	__HAL_TIM_ENABLE_IT( &Timerhdl_IrTx, TIM_FLAG_UPDATE);

	/* Enable the timer */
	//__HAL_TIM_ENABLE(&Timerhdl_IrTx);
	//HAL_TIM_Base_Stop_IT(&Timerhdl_IrTx);

	//if (HAL_TIM_Base_Start_IT(&Timerhdl_IrTx) != HAL_OK)
	//{
		//_Error_Handler(__FILE__, __LINE__);
	//	Error_Handler();
	//}

	/* Peripheral interrupt init */
	HAL_NVIC_SetPriority(IR_ENCODE_TIMER_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0);
	HAL_NVIC_EnableIRQ(IR_ENCODE_TIMER_IRQn);

	/* TIM Disable */
	//__HAL_TIM_DISABLE(&Timerhdl_IrTx);
} // void infrared_encode_sys_init(void)



/*============================================================================*/
/**
  * @brief  De-initializes the peripherals (RCC,GPIO, TIM)
  * @param  None
  * @retval None
  */
/*============================================================================*/
void infrared_encode_sys_deinit(void)
{
	BaseType_t ret;
	GPIO_InitTypeDef gpio_init_struct;

	gpio_init_struct.Pin = IR_DRV_Pin;
	gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
	gpio_init_struct.Pull = GPIO_NOPULL;
	gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(IR_GPIO_PORT, &gpio_init_struct);
	HAL_GPIO_WritePin(IR_GPIO_PORT, IR_DRV_Pin, GPIO_PIN_RESET);

	HAL_TIM_PWM_DeInit(&Timerhdl_IrCarrier);
	HAL_TIM_Base_DeInit(&Timerhdl_IrTx);

	IR_ENCODE_CARRIER_TIMER_CLK_DIS();
	IR_ENCODE_BASEBAND_TIMER_CLK_DIS();

	HAL_NVIC_DisableIRQ(IR_ENCODE_TIMER_IRQn);

	if( ir_tx_timer_hdl != NULL )
	{
		ret = xTimerDelete(ir_tx_timer_hdl, 0);
		assert_param(ret==pdPASS);
		ir_tx_timer_hdl = NULL;
	}

	if ( main_q_hdl != NULL)
		xQueueReset(main_q_hdl);

	//HAL_GPIO_DeInit(IR_GPIO_PORT, IR_TX_GPIO_PIN);
} // void infrared_encode_sys_deinit(void)



/*============================================================================*/
/**
  * This is a callback function for the software timer
  * It's used as a delay function for the Tx task
  *
  */
/*============================================================================*/
static void infrared_encode_timer_cb(TimerHandle_t xTimer)
{
	S_M1_Main_Q_t q_item;

	q_item.q_data.ir_tx_data = 1; // any value, not used
	q_item.q_evt_type = Q_EVENT_IRRED_TX;
	xQueueSend(main_q_hdl, &q_item, portMAX_DELAY);
} // static void infrared_encode_timer_cb(TimerHandle_t xTimer)
