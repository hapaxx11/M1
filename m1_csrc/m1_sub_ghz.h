/* See COPYING.txt for license details. */

/*
*
*  m1_sub_ghz.h
*
*  M1 sub-ghz functions
*
* M1 Project
*
*/

#ifndef M1_SUB_GHZ_H_
#define M1_SUB_GHZ_H_

#include "m1_io_defs.h"
#include "m1_ring_buffer.h"

#define SUBGHZ_RX_TIMER                 TIM1        /*!< Timer used for Sub-GHz decoding */
/* TIM prescaler is computed to have 1 μs as time base. TIM frequency (in MHz) / (prescaler+1) */
#define SUBGHZ_RX_TIM_PRESCALER         74           /*!< TIM prescaler */
#define SUBGHZ_RX_TIMER_CLK             __HAL_RCC_TIM1_CLK_ENABLE      /*!< Clock of the used timer */
#define SUBGHZ_RX_TIMER_CLK_DIS         __HAL_RCC_TIM1_CLK_DISABLE
#define SUBGHZ_RX_TIMER_IRQn            TIM1_CC_IRQn             /*!< IR TIM IRQ */
#define SUBGHZ_RX_TIMER_RX_CHANNEL   	TIM_CHANNEL_1            /*!< IR TIM Channel */
#define SUBGHZ_RX_TIMER_CH_ACTIV  		HAL_TIM_ACTIVE_CHANNEL_1

#define SUBGHZ_TX_CARRIER_TIMER         TIM1        /*!< Timer used for IR encoding */
#define SUBGHZ_TX_TIM_PRESCALER         149          /*!< TIM prescaler: 75MHz/150 = 500kHz = 2us/tick */
#define SUBGHZ_TX_TIMER_CLK     		__HAL_RCC_TIM1_CLK_ENABLE      /*!< Clock of the used timer */
#define SUBGHZ_TX_TIMER_CLK_DIS 		__HAL_RCC_TIM1_CLK_DISABLE
#define SUBGHZ_TX_TIMER_TX_CHANNEL   	TIM_CHANNEL_4 //CH4N
#define SUBGHZ_TX_TIMER_ENC_CH_ACTIV  	HAL_TIM_ACTIVE_CHANNEL_4
#define SUBGHZ_TX_TIMER_IRQn            TIM1_UP_IRQn

#define SUBGHZ_TX_CARRIER_FREQ_36KHZ_PERIOD		2083	// clock tick period = 1/75MHz, carrier period = 1/36KHz = 75MHz/36KHz = 2083 clock tick periods
#define SUBGHZ_TX_CARRIER_FREQ_30_KHZ			(uint32_t)30000
#define SUBGHZ_TX_CARRIER_FREQ_32_KHZ           (uint32_t)32000
#define SUBGHZ_TX_CARRIER_FREQ_36_KHZ           (uint32_t)36000
#define SUBGHZ_TX_CARRIER_FREQ_38_KHZ           (uint32_t)38000
#define SUBGHZ_TX_CARRIER_FREQ_40_KHZ           (uint32_t)40000
#define SUBGHZ_TX_CARRIER_FREQ_56_KHZ           (uint32_t)56000
#define SUBGHZ_TX_CARRIER_FREQ_455_KHZ          (uint32_t)455000

#define SUBGHZ_TX_CARRIER_PRESCALE_FACTOR		10

#define SUBGHZ_RAW_DATA_SAMPLES_TO_RW			512
#define SUBGHZ_FORTMATTED_DATA_SAMPLES_TO_RW    3840//5120//2560//3072//3840

#define SUBGHZ_MODULATION_LIST					3

// Defines for ISM bands regions
#define SUBGHZ_ISM_BAND_REGIONS_LIST			4

#define SUBGHZ_ISM_BAND_REGION_NORTH_AMERICA	0
#define SUBGHZ_ISM_BAND_REGION_EUROPE_REGION_1	1
#define SUBGHZ_ISM_BAND_REGION_ASIA				2

#define SUBGHZ_ISM_BAND_REGION					SUBGHZ_ISM_BAND_REGION_NORTH_AMERICA
// End - Defines for ISM bands regions

/* SI4463 operating range — single source of truth for all frequency validators */
#define SUBGHZ_MIN_FREQ_HZ    300000000UL   /* 300.000 MHz — PLL lower bound with outdiv=12 */
#define SUBGHZ_MAX_FREQ_HZ    928000000UL   /* 928.000 MHz — ISM upper edge */


// SUBGHZ_GPIO_0(RX)	PORTE.9	<--> TIM1_CH1
// SUBGHZ_GPIO_2(TX) 	PORTD.5	<--> TIM1_CH4N
#define SUBGHZ_RX_GPIO_PORT         SI4463_GPIO0_GPIO_Port
#define SUBGHZ_RX_GPIO_PIN         	SI4463_GPIO0_Pin
#define SUBGHZ_RX_GPIO_PORT_CLK		__HAL_RCC_GPIOE_CLK_ENABLE

#define SUBGHZ_TX_GPIO_PORT         SI4463_GPIO2_GPIO_Port
#define SUBGHZ_TX_GPIO_PIN         	SI4463_GPIO2_Pin
#define SUBGHZ_TX_GPIO_PORT_CLK		__HAL_RCC_GPIOD_CLK_ENABLE

#define SUBGHZ_GPIO_AF_TX          	GPIO_AF1_TIM1
#define SUBGHZ_GPIO_AF_RX         	GPIO_AF1_TIM1

#define TIM_FORCED_ACTIVE      		((uint16_t)0x0050)
#define TIM_FORCED_INACTIVE    		((uint16_t)0x0040)

#define SUBGHZ_OTA_PULSE_BIT_MASK	0x0001 // LSB bit = 1 for Mark, using OR operator
#define SUBGHZ_OTA_SPACE_BIT_MASK	0xFFFE // LSB bit = 0 for Space, using AND operator

/* RX timer period: use full 16-bit range (65535) so the unsigned
 * subtraction  (current_CCR − previous_CCR) & 0xFFFF  yields the
 * correct edge-to-edge duration for gaps up to 65.5 ms.  The old
 * value of 20000 caused aliasing for inter-packet gaps near multiples
 * of 20 ms.  TX timeout is kept separate. */
#define SUBGHZ_RX_TIMEOUT_TIME      0xFFFF
#define SUBGHZ_TX_TIMEOUT_TIME      20000

// RF_Input_Level_dBm = (RSSI_value / 2) – MODEM_RSSI_COMP – 70
#define MODEM_RSSI_COMP 	0x40 // = 64d is appropriate for most applications.

typedef enum
{
	SUB_GHZ_BAND_300 = 0,
	SUB_GHZ_BAND_310,
	SUB_GHZ_BAND_315,
	SUB_GHZ_BAND_345,
	SUB_GHZ_BAND_372,
	SUB_GHZ_BAND_390,
	SUB_GHZ_BAND_433,
	SUB_GHZ_BAND_433_92,
	SUB_GHZ_BAND_915,
	SUB_GHZ_BAND_EOL,
	/* Arbitrary frequency via SI446x_Set_Frequency() */
	SUB_GHZ_BAND_CUSTOM = 0x80
} S_M1_SubGHz_Band;

typedef enum
{
	SUB_GHZ_OPMODE_ISOLATED = 0,
	SUB_GHZ_OPMODE_RX,
	SUB_GHZ_OPMODE_TX,
	SUB_GHZ_OPMODE_EOL
} S_M1_SubGHz_OpMode;

typedef enum {
	PULSE_DET_NORMAL = 0,
	PULSE_DET_IDLE,
	PULSE_DET_ACTIVE, // Ready and waiting for a valid frame gap to continue
	PULSE_DET_EOP,	// End-of-packet condition met
	PULSE_DET_RISING,
	PULSE_DET_FALLING,
	PULSE_DET_UNKNOWN
} S_M1_SubGHz_Pulse_Det;

typedef enum {
	MODULATION_OOK = 0,
	MODULATION_ASK,
	MODULATION_FSK,
	MODULATION_UNKNOWN
} S_M1_SubGHz_Modulation;

typedef struct
{
	uint8_t channel;
	S_M1_SubGHz_Band band;
	S_M1_SubGHz_Modulation modulation;
} S_M1_SubGHz_Scan_Config;

void menu_sub_ghz_init(void);
void menu_sub_ghz_exit(void);

void sub_ghz_init(void);
void sub_ghz_frequency_reader(void);
void sub_ghz_spectrum_analyzer(void);
void sub_ghz_weather_station(void);
void sub_ghz_brute_force(void);
void sub_ghz_rssi_meter(void);
void sub_ghz_freq_scanner(void);
void sub_ghz_add_manually(void);
uint8_t sub_ghz_replay_flipper_file(const char *sub_path);

/**
 * @brief  Replay an M1 native .sgh NOISE file directly without conversion.
 *
 * Bypasses the temp-file conversion path used by sub_ghz_replay_flipper_file()
 * and feeds the original .sgh file straight into the streaming engine.
 * Use this whenever flipper_subghz_signal_t::is_m1_native is true and the
 * type is FLIPPER_SUBGHZ_TYPE_RAW.
 *
 * @param  sgh_path   Path to the .sgh NOISE file (e.g. "/SUBGHZ/foo.sgh")
 * @param  frequency  Carrier frequency in Hz (from the loaded signal metadata)
 * @param  modulation MODULATION_OOK / MODULATION_FSK / MODULATION_ASK
 * @retval 0 = success, non-zero = error code
 */
uint8_t sub_ghz_replay_datafile(const char *sgh_path, uint32_t frequency, uint8_t modulation);

/* Scene-based UI entry point (new Flipper-inspired architecture) */
void sub_ghz_scene_entry(void);

extern EXTI_HandleTypeDef 	si4463_exti_hdl;
extern TIM_HandleTypeDef   	timerhdl_subghz_tx;
extern TIM_HandleTypeDef   	timerhdl_subghz_rx;
extern DMA_HandleTypeDef	hdma_subghz_tx;
extern uint8_t subghz_tx_tc_flag;
extern S_M1_RingBuffer subghz_rx_rawdata_rb;
extern uint8_t subghz_record_mode_flag;
#endif /* M1_SUB_GHZ_H_ */
