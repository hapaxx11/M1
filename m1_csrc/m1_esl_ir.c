/* See COPYING.txt for license details. */

/**
 * @file   m1_esl_ir.c
 * @brief  ESL infrared transmitter — hardware driver.
 *
 * Port of the IR driver from TagTinker (github.com/i12bp8/TagTinker),
 * adapted for the M1 Hapax (STM32H573VIT at 250 MHz):
 *
 *   - TIM1_CH4N (PC5, GPIO_AF1_TIM1) instead of Flipper's TIM1_CH3N.
 *   - TIM1 APB2 timer clock = 75 MHz:  ARR = 59 → 75 MHz/60 ≈ 1.25 MHz.
 *   - Symbol timing uses DWT->CYCCNT at 250 MHz core clock.
 *   - Cycle counts are scaled from the original Flipper 64 MHz values
 *     by the factor 250/64 = 3.90625 (all original counts are multiples
 *     of 64, so no rounding error).
 *
 * Based on reverse-engineering research by furrtek (github.com/furrtek/PrecIR).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "m1_esl_ir.h"
#include "stm32h5xx_hal.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* --------------------------------------------------------------------------- */
/* Carrier timer constants                                                     */
/*                                                                             */
/* TIM1 APB2 timer clock = 75 MHz (no prescaler).                             */
/* ARR = 59  →  75 000 000 / 60 = 1 250 000 Hz  (~1.25 MHz carrier).         */
/* CCR4 = 30 →  50 % duty cycle.                                              */
/* --------------------------------------------------------------------------- */
#define ESL_CARRIER_ARR   59U
#define ESL_CARRIER_CCR   30U

/*
 * OC4M output-compare mode field for TIM1 CCMR2 (channel 4).
 * Bits [14:12] hold OC4M[2:0].  Bit 25 holds OC4M[3] (extended modes).
 * Only bits [14:12] are needed here: PWM2 = 0b0111, Force-inactive = 0b0100.
 */
#define ESL_OC4M_BITS_MASK  ((7U << 12U) | (1U << 25U))
#define ESL_OC4M_PWM2       (7U << 12U)    /* carrier on  */
#define ESL_OC4M_FORCE_OFF  (4U << 12U)    /* carrier off */

/* --------------------------------------------------------------------------- */
/* PP4 symbol timing                                                           */
/*                                                                             */
/* Source: TagTinker constants measured at Flipper's 64 MHz clock.            */
/* Scale factor: 250 / 64 = 3.90625.  All original values are exact multiples */
/* of 64, so the scaled results are exact integers.                           */
/*                                                                             */
/* PP4 burst  =  2581 × 3.90625 = 10082 cycles  ≈  40.3 µs                  */
/* Gap sym 0  =  3871 × 3.90625 = 15122 cycles  ≈  60.5 µs                  */
/* Gap sym 1  = 15483 × 3.90625 = 60481 cycles  ≈ 241.9 µs (longest)         */
/* Gap sym 2  =  7741 × 3.90625 = 30238 cycles  ≈ 121.0 µs                  */
/* Gap sym 3  = 11612 × 3.90625 = 45359 cycles  ≈ 181.4 µs                  */
/* --------------------------------------------------------------------------- */
#define ESL_PP4_BURST_CYCLES  10082U

static const uint32_t esl_pp4_gap_cycles[4U] = {
    15122U,   /* symbol 0  ≈  60.5 µs */
    60481U,   /* symbol 1  ≈ 241.9 µs */
    30238U,   /* symbol 2  ≈ 121.0 µs */
    45359U,   /* symbol 3  ≈ 181.4 µs */
};

/* --------------------------------------------------------------------------- */
/* Module state                                                                */
/* --------------------------------------------------------------------------- */
static bool esl_ir_initialized = false;

/* --------------------------------------------------------------------------- */
/* Carrier control (inlined for minimal jitter)                               */
/* --------------------------------------------------------------------------- */

static inline void carrier_on(void)
{
    TIM1->CCMR2 = (TIM1->CCMR2 & ~ESL_OC4M_BITS_MASK) | ESL_OC4M_PWM2;
}

static inline void carrier_off(void)
{
    TIM1->CCMR2 = (TIM1->CCMR2 & ~ESL_OC4M_BITS_MASK) | ESL_OC4M_FORCE_OFF;
}

/* --------------------------------------------------------------------------- */
/* Cycle-accurate delay via DWT->CYCCNT                                       */
/* --------------------------------------------------------------------------- */

static inline void delay_cycles(uint32_t cycles)
{
    uint32_t start = DWT->CYCCNT;
    while((DWT->CYCCNT - start) < cycles) {
        /* busy-wait */
    }
}

/* --------------------------------------------------------------------------- */
/* PP4 frame transmit                                                          */
/* --------------------------------------------------------------------------- */

static void send_frame_pp4(const uint8_t *data, size_t len)
{
    for(size_t i = 0U; i < len; i++) {
        uint8_t byte = data[i];
        for(int s = 0; s < 4; s++) {
            uint8_t sym = byte & 0x03U;
            byte >>= 2U;

            carrier_on();
            delay_cycles(ESL_PP4_BURST_CYCLES);
            carrier_off();
            delay_cycles(esl_pp4_gap_cycles[sym]);
        }
    }
    /* Terminal burst closes the last symbol's trailing silence. */
    carrier_on();
    delay_cycles(ESL_PP4_BURST_CYCLES);
    carrier_off();
}

/* --------------------------------------------------------------------------- */
/* Public API                                                                  */
/* --------------------------------------------------------------------------- */

void m1_esl_ir_init(void)
{
    if(esl_ir_initialized) return;

    /* Enable peripheral clocks. */
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Configure PC5 as TIM1_CH4N (AF1, push-pull). */
    GPIO_InitTypeDef gpio = {
        .Pin       = GPIO_PIN_5,
        .Mode      = GPIO_MODE_AF_PP,
        .Pull      = GPIO_NOPULL,
        .Speed     = GPIO_SPEED_FREQ_VERY_HIGH,
        .Alternate = GPIO_AF1_TIM1,
    };
    HAL_GPIO_Init(GPIOC, &gpio);

    /* Reset TIM1 to a clean state before reconfiguring. */
    __HAL_RCC_TIM1_FORCE_RESET();
    __HAL_RCC_TIM1_RELEASE_RESET();

    /* 1.25 MHz carrier, 50 % duty cycle on CH4N. */
    TIM1->PSC  = 0U;
    TIM1->ARR  = ESL_CARRIER_ARR;
    TIM1->RCR  = 0U;
    TIM1->CCR4 = ESL_CARRIER_CCR;

    /* OC4PE (output compare 4 preload enable) = bit 11 of CCMR2.
     * Start with carrier forced off. */
    TIM1->CCMR2 = (TIM1->CCMR2 & ~ESL_OC4M_BITS_MASK) | ESL_OC4M_FORCE_OFF;
    TIM1->CCMR2 |= TIM_CCMR2_OC4PE;

    /* Enable CH4N complementary output (CCER bit 13 = CC4NE). */
    TIM1->CCER |= TIM_CCER_CC4NE;

    /* Enable main output (BDTR MOE). */
    TIM1->BDTR |= TIM_BDTR_MOE;

    /* Generate an update event to transfer preloaded values to active regs. */
    TIM1->EGR = TIM_EGR_UG;

    /* Start the counter. */
    TIM1->CR1 |= TIM_CR1_CEN;

    /* Enable DWT cycle counter (required for delay_cycles). */
    if(!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk))
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    if(!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
        DWT->CYCCNT = 0U;
        DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    }

    esl_ir_initialized = true;
}

void m1_esl_ir_deinit(void)
{
    if(!esl_ir_initialized) return;

    carrier_off();

    /* Disable CH4N output and main output. */
    TIM1->CCER &= ~TIM_CCER_CC4NE;
    TIM1->BDTR &= ~TIM_BDTR_MOE;

    /* Stop the counter. */
    TIM1->CR1 &= ~TIM_CR1_CEN;

    /* Disable TIM1 clock. */
    __HAL_RCC_TIM1_CLK_DISABLE();

    /* Return PC5 to floating analog input. */
    GPIO_InitTypeDef gpio = {
        .Pin   = GPIO_PIN_5,
        .Mode  = GPIO_MODE_ANALOG,
        .Pull  = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_Init(GPIOC, &gpio);

    esl_ir_initialized = false;
}

bool m1_esl_ir_transmit_pp4(const uint8_t *data, size_t len,
                             uint8_t repeats, uint16_t delay_ms)
{
    if(!esl_ir_initialized) return false;
    if(!data || len == 0U || len > 255U) return false;

    for(uint32_t r = 0U; r <= (uint32_t)repeats; r++) {
        send_frame_pp4(data, len);
        if(r < (uint32_t)repeats && delay_ms > 0U)
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    return true;
}
