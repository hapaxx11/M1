/* See COPYING.txt for license details. */

#include "unity.h"

#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#include "lfrfid_hal.h"

void setUp(void) {}
void tearDown(void) {}

void test_tim5_irq_allows_isr_safe_freertos_calls(void)
{
    TEST_ASSERT_EQUAL_UINT32(configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
                             LFRFID_TIM5_IRQ_PRIORITY);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
                                        LFRFID_TIM5_IRQ_PRIORITY);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tim5_irq_allows_isr_safe_freertos_calls);
    return UNITY_END();
}
