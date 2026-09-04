/* See COPYING.txt for license details. */

/*
*
*  m1_infrared.h
*
*  M1 Infrared functions
*
* M1 Project
*
*/

#ifndef M1_INFRARED_H_
#define M1_INFRARED_H_

#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "app_freertos.h"
#include "m1_compile_cfg.h"
#include "queue.h"
#include "irmp.h"
#include "flipper_ir.h"

#define IR_DECODE_TIMER                 TIM2        /*!< Timer used for IR decoding */
/* TIM prescaler is computed to have 1 μs as time base. TIM frequency (in MHz) / (prescaler+1) */
#define IR_RX_TIM_PRESCALER          	74           /*!< TIM prescaler */
#define IR_DECODE_TIMER_CLK             __HAL_RCC_TIM2_CLK_ENABLE      /*!< Clock of the used timer */
#define IR_DECODE_TIMER_CLK_DIS         __HAL_RCC_TIM2_CLK_DISABLE
#define IR_DECODE_TIMER_IRQn            TIM2_IRQn             /*!< IR TIM IRQ */
#define IR_DECODE_TIMER_RX_CHANNEL   	TIM_CHANNEL_4            /*!< IR TIM Channel */
#define IR_DECODE_TIMER_DEC_CH_ACTIV  	HAL_TIM_ACTIVE_CHANNEL_4

#define IR_ENCODE_CARRIER_TIMER         TIM1        /*!< Timer used for IR encoding */
#define IR_ENCODE_BASEBAND_TIMER        TIM16
#define IR_TX_TIM_PRESCALER          	0           /*!< TIM prescaler */
#define IR_ENCODE_CARRIER_TIMER_CLK     	__HAL_RCC_TIM1_CLK_ENABLE      /*!< Clock of the used timer */
#define IR_ENCODE_CARRIER_TIMER_CLK_DIS 	__HAL_RCC_TIM1_CLK_DISABLE
#define IR_ENCODE_BASEBAND_TIMER_CLK     	__HAL_RCC_TIM16_CLK_ENABLE
#define IR_ENCODE_BASEBAND_TIMER_CLK_DIS 	__HAL_RCC_TIM16_CLK_DISABLE
#define IR_ENCODE_TIMER_TX_CHANNEL   	TIM_CHANNEL_4            /*!< IR TIM Channel */
#define IR_ENCODE_TIMER_ENC_CH_ACTIV  	HAL_TIM_ACTIVE_CHANNEL_4
#define IR_ENCODE_TIMER_IRQn            TIM16_IRQn             /*!< IR TIM IRQ */

#define IR_ENCODE_CARRIER_FREQ_36KHZ_PERIOD		2083	// clock tick period = 1/75MHz, carrier period = 1/36KHz = 75MHz/36KHz = 2083 clock tick periods
#define IR_ENCODE_CARRIER_FREQ_30_KHZ			(uint32_t)30000
#define IR_ENCODE_CARRIER_FREQ_32_KHZ           (uint32_t)32000
#define IR_ENCODE_CARRIER_FREQ_36_KHZ           (uint32_t)36000
#define IR_ENCODE_CARRIER_FREQ_38_KHZ           (uint32_t)38000
#define IR_ENCODE_CARRIER_FREQ_40_KHZ           (uint32_t)40000
#define IR_ENCODE_CARRIER_FREQ_56_KHZ           (uint32_t)56000
#define IR_ENCODE_CARRIER_FREQ_455_KHZ          (uint32_t)455000

#define IR_ENCODE_CARRIER_PRESCALE_FACTOR		10

#define IR_ENC_HPERIOD_RC5      ((uint32_t)1333)        /*!< RC5 Encoder modulation frequency base period */
#define IR_ENC_LPERIOD_RC5      ((uint32_t)46630)       /*!< RC5 Encoder pulse base period */
#define IR_ENC_HPERIOD_SIRC     ((uint32_t)1200)        /*!< SIRC Encoder modulation frequency base period */
#define IR_ENC_LPERIOD_SIRC     ((uint32_t)28799)       /*!< SIRC Encoder pulse base period */

// IR_RX	PORTC.4	<--> TIM2_CH4
// IR_DRV 	PORTC.5	<--> TIM1_CH4N
#define IR_GPIO_PORT           GPIOC                    /*!< Port which IR input is connected */
#define IR_GPIO_PORT_CLK       __HAL_RCC_GPIOC_CLK_ENABLE      /*!< IR pin GPIO Clock Port */
#define IR_RX_GPIO_PIN         	GPIO_PIN_4               /*!< Pin which IR is connected */
#define IR_TX_GPIO_PIN         	GPIO_PIN_5
#define IR_GPIO_AF_TR          	GPIO_AF1_TIM1
#define IR_GPIO_AF_RX         	GPIO_AF1_TIM2

/* ---- External IR transmitter on the expansion header (m1_ir_ext_on == 1) ----
 * HX-53 transmitter DAT on PA9 = TIM1_CH2 (shares the onboard carrier timer TIM1,
 * AF1). PA9/CH2 is a REGULAR (non-complementary) output, so irsnd drives CCxE
 * here instead of the onboard CH4N's CCxNE (see irsnd_set_output_mode()). The
 * HX-53 is a 5 V module, powered from the +5_EXT rail (ext_power_5V_set).
 * Receive/learn always stays on the onboard receiver (PC4). */
#define IR_EXT_TX_GPIO_PORT     GPIOA
#define IR_EXT_TX_GPIO_PIN      GPIO_PIN_9
#define IR_EXT_TX_GPIO_AF       GPIO_AF1_TIM1
#define IR_EXT_TX_TIM_CHANNEL   TIM_CHANNEL_2

/* IR transmit routing path (pure decision; testable on host). */
typedef enum { IR_PATH_ONBOARD = 0, IR_PATH_EXTERNAL = 1 } S_M1_IR_Path;
static inline S_M1_IR_Path ir_active_path(uint8_t ext_on)
{
	return ext_on ? IR_PATH_EXTERNAL : IR_PATH_ONBOARD;
}

#define TIM_FORCED_ACTIVE      ((uint16_t)0x0050)
#define TIM_FORCED_INACTIVE    ((uint16_t)0x0040)

#define IR_OTA_PULSE_BIT_MASK	0x0001 // LSB bit = 1 for Mark, using OR operator
#define IR_OTA_SPACE_BIT_MASK	0xFFFE // LSB bit = 0 for Space, using AND operator

typedef enum {
	EDGE_DET_FALLING = 0,
	EDGE_DET_RISING,
	EDGE_DET_IDLE,
	EDGE_DET_ACTIVE
} S_M1_IR_Det;

typedef enum
{
	IR_TX_INIT = 0,
	IR_TX_ACTIVE,
	IR_TX_POST_PROCESS,
	IR_TX_DELAY,
	IR_TX_COMPLETED
} S_M1_IR_Tx_States;

void menu_infrared_init(void);

void infrared_universal_remotes(void);
void esl_scene_entry(void);
void infrared_learn_new_remote(void);
void infrared_saved_remotes(void);
bool infrared_capture_one_signal(IRMP_DATA *out_data);
void infrared_decode_sys_init(void);
void infrared_decode_sys_deinit(void);
void infrared_encode_sys_init(void);
void infrared_encode_sys_deinit(void);
void infrared_send_raw_signal(const flipper_ir_signal_t *sig);
uint16_t *infrared_raw_ota_buffer(void);
/* Diagnostic: drive a solid 38 kHz carrier on the external TX pin (PA9) and show
   the TIM1/GPIO register state. Triggered from Settings -> External IR (OK). */
void infrared_ext_tx_selftest(void);
/* Enable/disable the external IR transmitter rail (+5_EXT). Kept ON the whole
   time External IR is enabled so the HX-53 stays powered (not just during a TX). */
void infrared_ext_power(uint8_t on);
S_M1_IR_Tx_States infrared_transmit(uint8_t init);

extern uint32_t TIM_GetCounterCLKValue(uint16_t prescaler);
extern void HAL_TIM_PeriodElapsedCallback_IR(TIM_HandleTypeDef *htim);

extern volatile S_M1_IR_Det IrRx_Edge_Det;

extern TIM_HandleTypeDef    Timerhdl_IrCarrier;
extern TIM_HandleTypeDef    Timerhdl_IrTx;
extern TIM_HandleTypeDef    Timerhdl_IrRx;

extern volatile uint8_t ir_ota_data_tx_active;
extern uint16_t ir_ota_data_tx_len;
extern volatile uint16_t ir_ota_data_tx_counter;
extern uint16_t *pir_ota_data_tx_buffer;

#endif /* M1_INFRARED_H_ */
