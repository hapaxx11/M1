/* See COPYING.txt for license details. */

/*
*
* m1_led_indicator.c
*
* Library for led indicator
*
* M1 Project
*
*/

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_lp5814.h"
#include "m1_system.h"
#include "m1_led_color.h"
#include "battery.h"

/*************************** D E F I N E S ************************************/

#define LED_FUNC_QUEUE_SIZE		3
#define LED_FUNC_PARAM_SIZE		3

//************************** C O N S T A N T **********************************/

//************************** S T R U C T U R E S *******************************

typedef void (*recovery_func_t)(uint8_t *params);

const recovery_func_t recovery_funcs[LED_EOL_FN_ID] = {
													m1_led_indicator_off,
													m1_led_indicator_on,
													m1_led_fast_blink_ex,
													m1_led_fw_update_on,
													m1_led_indicator_off,
													m1_led_batt_charged_on,
													m1_led_batt_full_on
												};

typedef struct
{
	uint8_t active_led_func_id;
	uint8_t led_func_params[LED_FUNC_PARAM_SIZE];
} S_M1_LED_FUNC_t;

static S_M1_LED_FUNC_t recovery_queue[LED_FUNC_QUEUE_SIZE] = {0};
static uint8_t queued_func_ids = 0;

/***************************** V A R I A B L E S ******************************/

static S_M1_LED_FUNC_t active_led_func = {
								.active_led_func_id = LED_INDICATOR_OFF_FN_ID
};


/********************* F U N C T I O N   P R O T O T Y P E S ******************/

void m1_led_indicator_off(uint8_t *params);
void m1_led_indicator_on(uint8_t *params);
void m1_led_fast_blink_ex(uint8_t *params);
void m1_led_fw_update_on(uint8_t *params);
void m1_led_fw_update_off(void);
void m1_led_batt_charged_on(uint8_t *params);
void m1_led_batt_full_on(uint8_t *params);

void m1_led_fast_blink(uint8_t r_g_b, uint8_t pwm_rgb, uint8_t on_off_ms);
void m1_led_set_blink_timer(uint8_t r_g_b, uint16_t on_off_ms, uint8_t mode);
uint8_t m1_led_get_running_id(void);
void m1_led_insert_function_id(uint8_t func_id, uint8_t *params);
static void m1_led_add_function_id(uint8_t func_id, uint8_t *params);
void m1_led_func_recovery(void);
/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/


/*============================================================================*/
/*
 * This function turns off all R/G/B LEDs.
 *
*/
/*============================================================================*/
void m1_led_indicator_off(uint8_t *params)
{
	active_led_func.active_led_func_id = LED_INDICATOR_OFF_FN_ID;
	lp5814_all_off_RGB();
} // void m1_led_indicator_off(uint8_t *params)



/*============================================================================*/
/*
 * This function turns on LED indicator using the user-configured default
 * LED color (m1_led_color_r/g/b).
*/
/*============================================================================*/
void m1_led_indicator_on(uint8_t *params)
{
	active_led_func.active_led_func_id = LED_INDICATOR_ON_FN_ID;
	lp5814_all_off_RGB();
	if (m1_led_color_r) lp5814_led_on_Red(m1_led_color_r);
	if (m1_led_color_g) lp5814_led_on_Green(m1_led_color_g);
	if (m1_led_color_b) lp5814_led_on_Blue(m1_led_color_b);
} // void m1_led_indicator_on(uint8_t *params)



/*============================================================================*/
/*
 * Wrapper function of the function m1_led_fast_blink
 * Input param:
 *
*/
/*============================================================================*/
void m1_led_fast_blink_ex(uint8_t *params)
{
	if ( params!= NULL )
	{
		m1_led_fast_blink(params[0], params[1], params[2]);
	} // if ( params!= NULL )
} // void m1_led_fast_blink_ex(uint8_t *params)



/*============================================================================*/
/*
 * This function turns on LED indicator in fast blink mode.
 * Input param: pwm_rgb brightness of the RGB LED, 0 - 255
 * 				on_off_ms on off time in ms
*/
/*============================================================================*/
void m1_led_fast_blink(uint8_t r_g_b, uint8_t pwm_rgb, uint8_t on_off_ms)
{
	if ( on_off_ms!=LED_FASTBLINK_ONTIME_OFF ) // Turn on?
	{
		lp5814_fastblink_on_R_G_B(r_g_b, pwm_rgb, on_off_ms);
		if (active_led_func.active_led_func_id != LED_INDICATOR_OFF_FN_ID) // Another led function is running?
		{
			m1_led_add_function_id(active_led_func.active_led_func_id, NULL);
		}
		active_led_func.active_led_func_id = LED_FAST_BLINK_FN_ID;
		active_led_func.led_func_params[0] = r_g_b;
		active_led_func.led_func_params[1] = pwm_rgb;
		active_led_func.led_func_params[2] = on_off_ms;
	}
	else if (active_led_func.active_led_func_id==LED_FAST_BLINK_FN_ID) // Turn off
	{
		lp5814_fastblink_on_R_G_B(r_g_b, pwm_rgb, on_off_ms);
		m1_led_func_recovery();
	}
} // void m1_led_fast_blink(uint8_t r_g_b, uint8_t pwm_rgb, uint8_t on_off_ms)




/*============================================================================*/
/*
 * This function turns on RGB led in fast blink mode during device FW update.
 * Input param: none
*/
/*============================================================================*/
void m1_led_fw_update_on(uint8_t *params)
{
	//active_led_func.active_led_func_id = LED_FW_UPDATE_ON_FN_ID;
	m1_led_fast_blink(LED_BLINK_ON_GREEN | LED_BLINK_ON_BLUE, 25, 100);
} // void m1_led_fw_update_on(uint8_t *params)



/*============================================================================*/
/*
 * This function turns off RGB led in fast blink mode after device FW update.
 * Input param: none
*/
/*============================================================================*/
void m1_led_fw_update_off(void)
{
	//active_led_func.active_led_func_id = LED_INDICATOR_OFF_FN_ID; // Reset
	m1_led_fast_blink(LED_BLINK_ON_GREEN | LED_BLINK_ON_BLUE, 0, 0);
} // void m1_led_fw_update_off(void)



/*============================================================================*/
/**
 * @brief Turn on LED indicator for battery charge status.
 *        Color is eased between low-batt and full-charge user colors
 *        based on the current battery level.
 */
/*============================================================================*/
void m1_led_batt_charged_on(uint8_t *params)
{
	active_led_func.active_led_func_id = LED_BATTERY_CHARGED_ON_FN_ID;
	lp5814_all_off_RGB();

	S_M1_Power_Status_t pwr;
	battery_power_status_get(&pwr);
	uint8_t level = pwr.battery_level;
	if (level > 100) level = 100;

	uint8_t r, g, b;
	m1_led_color_ease(level,
	                  m1_led_color_r, m1_led_color_g, m1_led_color_b,
	                  m1_led_lowbatt_r, m1_led_lowbatt_g, m1_led_lowbatt_b,
	                  &r, &g, &b);
	if (r) lp5814_led_on_Red(r);
	if (g) lp5814_led_on_Green(g);
	if (b) lp5814_led_on_Blue(b);
} // void m1_led_batt_charged_on(uint8_t *params)



/*============================================================================*/
/**
 * @brief Turn on LED indicator for battery full status.
 *        Uses the user-configured full-charge LED color.
 */
/*============================================================================*/
void m1_led_batt_full_on(uint8_t *params)
{
	active_led_func.active_led_func_id = LED_BATTERY_FULL_ON_FN_ID;
	lp5814_all_off_RGB();
	if (m1_led_color_r) lp5814_led_on_Red(m1_led_color_r);
	if (m1_led_color_g) lp5814_led_on_Green(m1_led_color_g);
	if (m1_led_color_b) lp5814_led_on_Blue(m1_led_color_b);
} // void m1_led_batt_full_on(uint8_t *params)


/*============================================================================*/
/**
  * @brief  Blink the R/G/B with timer.
  * @param  r_g_b select LED(s) to blink
  * 		on_off_ms blink time in millisecond
  * 		mode blink mode
  * @retval None
  */
/*============================================================================*/
void m1_led_set_blink_timer(uint8_t r_g_b, uint16_t on_off_ms, uint8_t mode)
{
	lp5814_set_blink_timer(r_g_b, on_off_ms, mode, m1_led_func_recovery);

	if ( active_led_func.active_led_func_id != LED_INDICATOR_OFF_FN_ID ) // A led function is running?
		m1_led_add_function_id(active_led_func.active_led_func_id, active_led_func.led_func_params);
} // void m1_led_set_blink_timer(uint8_t r_g_b, uint16_t on_off_ms, uint8_t mode)



/*============================================================================*/
/**
 * @brief Return the running function id
 */
/*============================================================================*/
uint8_t m1_led_get_running_id(void)
{
	return active_led_func.active_led_func_id;
} // uint8_t m1_led_get_running_id(void)



/*============================================================================*/
/**
 * @brief Insert a led function id for the recovery function
 */
/*============================================================================*/
void m1_led_insert_function_id(uint8_t func_id, uint8_t *params)
{
	uint8_t i;

	if ( queued_func_ids )
	{
		if ( (recovery_queue[0].active_led_func_id >= LED_BATTERY_UNCHARGED_FN_ID) && (recovery_queue[0].active_led_func_id <= LED_BATTERY_FULL_ON_FN_ID) )
		{
			recovery_queue[0].active_led_func_id = func_id; // Overwrite existing func id, instead of inserting.
			return;
		}
	} // if ( queued_func_ids )

	if ( queued_func_ids >= LED_FUNC_QUEUE_SIZE ) // Full queue?
		queued_func_ids = LED_FUNC_QUEUE_SIZE - 1; // Overwrite the last one

	for (i=queued_func_ids; i > 0; i--)
	{
		recovery_queue[i] = recovery_queue[i - 1]; // Shift the queue up
	} // for (i=queued_func_ids; i > 0; i--)

	recovery_queue[i].active_led_func_id = func_id;
	if ( params != NULL )
	{
		recovery_queue[i].led_func_params[0] = params[0];
		recovery_queue[i].led_func_params[1] = params[1];
		recovery_queue[i].led_func_params[2] = params[2];
	} // if ( params != NULL )

	queued_func_ids++;
} // void m1_led_insert_function_id(uint8_t func_id, uint8_t *params)



/*============================================================================*/
/**
 * @brief Add a led function id for the recovery function
 */
/*============================================================================*/
void m1_led_add_function_id(uint8_t func_id, uint8_t *params)
{
	if ( queued_func_ids >= LED_FUNC_QUEUE_SIZE ) // Full queue?
		queued_func_ids = LED_FUNC_QUEUE_SIZE - 1; // Overwrite the last one

	recovery_queue[queued_func_ids].active_led_func_id = func_id;
	if ( params != NULL )
	{
		recovery_queue[queued_func_ids].led_func_params[0] = params[0];
		recovery_queue[queued_func_ids].led_func_params[1] = params[1];
		recovery_queue[queued_func_ids].led_func_params[2] = params[2];
	} // if ( params != NULL )

	queued_func_ids++;
} // void m1_led_add_function_id(uint8_t func_id, uint8_t *params)



/*============================================================================*/
/*
 * This function restarts a led function that has been queued
 * Input param: none
*/
/*============================================================================*/
void m1_led_func_recovery(void)
{
	S_M1_LED_FUNC_t *pid;

	if ( queued_func_ids )
	{
		active_led_func.active_led_func_id = LED_INDICATOR_OFF_FN_ID; // Reset
		pid = &recovery_queue[queued_func_ids - 1];
		recovery_funcs[pid->active_led_func_id](pid->led_func_params);
		queued_func_ids--;
	}
	else
	{
		active_led_func.active_led_func_id = LED_INDICATOR_OFF_FN_ID; // Reset
	}
} // void m1_led_func_recovery(void)
