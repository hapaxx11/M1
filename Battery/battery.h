/* See COPYING.txt for license details. */

/*
 * battery.h
 *
 *  Created on: Oct 31, 2025
 *      Author: thomas
 */

#ifndef BATTERY_H_
#define BATTERY_H_


typedef enum
{
	POWER_CTRL_OPT_SHUTDOWN = 0,
	POWER_CTRL_OPT_REBOOT
} S_M1_Power_Ctrl_t;

// BQ25896 CHRG_STAT values (power_status.stat)
#define CHRG_STAT_NOT_CHARGING  0
#define CHRG_STAT_PRE_CHARGE    1
#define CHRG_STAT_FAST_CHARGE   2
#define CHRG_STAT_COMPLETE      3

typedef struct
{
	uint8_t 	stat;					// Charge / Dis-Charge
	uint8_t     fault;					// 0 Normal, 1: Input, 2: Thermal Shutdown, 3: Safety Timer Expiration
	float 		charge_voltage;			// bus voltage
	uint16_t	charge_current;			// 0 ~ xxxx mA

	uint8_t		battery_level;			// 0 ~ 100%
	uint8_t		battery_temp;			// 0 ~ 80 deg
	float 		battery_voltage;		// battery voltage
	uint8_t		battery_health;			// 0 ~ 100 %
	int			consumption_current;	// 0 ~ xxx mA

	uint8_t     soh_state;
	uint16_t    flags;
	uint16_t    status;
} S_M1_Power_Status_t;

//extern S_M1_Power_Status_t power_status;
uint32_t battery_power_status_get(S_M1_Power_Status_t *pSystemPowerStatus);
void battery_status_update(void);
void battery_service_init(void);

#endif /* BATTERY_H_ */
