/* Minimal stm32h5xx_hal.h stub for host-side unit tests.
 * Provides only the types and macros referenced by m1_io_defs.h,
 * m1_sub_ghz.h, and m1_sub_ghz_decenc.h. */
#ifndef STM32H5XX_HAL_H_STUB
#define STM32H5XX_HAL_H_STUB

#include <stdint.h>

/* GPIO type */
typedef struct { uint32_t dummy; } GPIO_TypeDef;

/* Timer, DMA, EXTI handle types */
typedef struct { uint32_t dummy; } TIM_TypeDef;
typedef struct { uint32_t dummy; } TIM_HandleTypeDef;
typedef struct { uint32_t dummy; } DMA_HandleTypeDef;
typedef struct { uint32_t dummy; } EXTI_HandleTypeDef;

/* GPIO pin macros */
#define GPIO_PIN_0   ((uint16_t)0x0001)
#define GPIO_PIN_1   ((uint16_t)0x0002)
#define GPIO_PIN_2   ((uint16_t)0x0004)
#define GPIO_PIN_3   ((uint16_t)0x0008)
#define GPIO_PIN_4   ((uint16_t)0x0010)
#define GPIO_PIN_5   ((uint16_t)0x0020)
#define GPIO_PIN_6   ((uint16_t)0x0040)
#define GPIO_PIN_7   ((uint16_t)0x0080)
#define GPIO_PIN_8   ((uint16_t)0x0100)
#define GPIO_PIN_9   ((uint16_t)0x0200)
#define GPIO_PIN_10  ((uint16_t)0x0400)
#define GPIO_PIN_11  ((uint16_t)0x0800)
#define GPIO_PIN_12  ((uint16_t)0x1000)
#define GPIO_PIN_13  ((uint16_t)0x2000)
#define GPIO_PIN_14  ((uint16_t)0x4000)
#define GPIO_PIN_15  ((uint16_t)0x8000)

/* GPIO port instances */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
static GPIO_TypeDef _stub_gpioa, _stub_gpiob, _stub_gpioc, _stub_gpiod, _stub_gpioe;
static TIM_TypeDef  _stub_tim1;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#define GPIOA (&_stub_gpioa)
#define GPIOB (&_stub_gpiob)
#define GPIOC (&_stub_gpioc)
#define GPIOD (&_stub_gpiod)
#define GPIOE (&_stub_gpioe)

/* Clock enable macros (no-ops) */
#define __HAL_RCC_TIM1_CLK_ENABLE()   ((void)0)
#define __HAL_RCC_TIM1_CLK_DISABLE()  ((void)0)
#define __HAL_RCC_GPIOA_CLK_ENABLE()  ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE()  ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE()  ((void)0)
#define __HAL_RCC_GPIOD_CLK_ENABLE()  ((void)0)
#define __HAL_RCC_GPIOE_CLK_ENABLE()  ((void)0)

/* Timer/IRQ defines */
#define TIM1        (&_stub_tim1)
#define TIM1_CC_IRQn    0
#define TIM1_UP_IRQn    0
#define TIM_CHANNEL_1   0x0000
#define TIM_CHANNEL_4   0x000C
#define HAL_TIM_ACTIVE_CHANNEL_1 0x01
#define HAL_TIM_ACTIVE_CHANNEL_4 0x08
#define GPIO_AF1_TIM1   0x01

/* SPI handle type (referenced by some headers) */
typedef struct { uint32_t dummy; } SPI_HandleTypeDef;

#endif /* STM32H5XX_HAL_H_STUB */
