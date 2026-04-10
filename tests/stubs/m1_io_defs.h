/* Minimal m1_io_defs.h stub for host-side unit tests.
 * Provides the GPIO/HAL type stubs so m1_sub_ghz.h compiles. */

#ifndef M1_IO_DEFS_H_STUB
#define M1_IO_DEFS_H_STUB

#include <stdint.h>

/* Stub types normally provided by stm32h5xx_hal.h */
typedef struct { uint32_t dummy; } GPIO_TypeDef;
typedef struct { uint32_t dummy; } TIM_TypeDef;
typedef struct { uint32_t dummy; } EXTI_HandleTypeDef;
typedef struct { uint32_t dummy; } TIM_HandleTypeDef;
typedef struct { uint32_t dummy; } DMA_HandleTypeDef;

/* Minimal GPIO pin/port stubs referenced by m1_sub_ghz.h */
#define SI4463_GPIO0_GPIO_Port  ((GPIO_TypeDef*)0)
#define SI4463_GPIO0_Pin        0
#define SI4463_GPIO2_GPIO_Port  ((GPIO_TypeDef*)0)
#define SI4463_GPIO2_Pin        0
#define TIM1                    ((TIM_TypeDef*)0)

/* HAL macros referenced by m1_sub_ghz.h */
#define __HAL_RCC_TIM1_CLK_ENABLE()   ((void)0)
#define __HAL_RCC_TIM1_CLK_DISABLE()  ((void)0)
#define __HAL_RCC_GPIOE_CLK_ENABLE()  ((void)0)
#define __HAL_RCC_GPIOD_CLK_ENABLE()  ((void)0)

#define TIM1_CC_IRQn    0
#define TIM1_UP_IRQn    0
#define TIM_CHANNEL_1   0
#define TIM_CHANNEL_4   0
#define HAL_TIM_ACTIVE_CHANNEL_1 0
#define HAL_TIM_ACTIVE_CHANNEL_4 0
#define GPIO_AF1_TIM1   0

#endif /* M1_IO_DEFS_H_STUB */
