#ifndef TEST_RNG_STM32H5XX_HAL_H
#define TEST_RNG_STM32H5XX_HAL_H

#include <stdint.h>

typedef struct {
    volatile uint32_t CCIPR5;
    volatile uint32_t AHB2ENR;
} RCC_TypeDef;

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t SR;
    volatile uint32_t DR;
} RNG_TypeDef;

extern RCC_TypeDef test_rng_rcc;
extern RNG_TypeDef test_rng_regs;

#define RCC (&test_rng_rcc)
#define RNG (&test_rng_regs)

#define RCC_CCIPR5_RNGSEL 0x3u
#define RCC_AHB2ENR_RNGEN (1u << 6)
#define RNG_CR_RNGEN (1u << 2)
#define RNG_SR_DRDY (1u << 0)
#define RNG_SR_SEIS (1u << 2)
#define RNG_SR_CEIS (1u << 5)

static inline uint32_t HAL_GetTick(void) {
    return 0;
}

static inline void __disable_irq(void) {}
static inline void __enable_irq(void) {}

#endif
