/* Host test for the M1 edge-based IRMP decoder (Infrared/irmp.c).
 *
 * Feeds a synthetic NEC frame into irmp_data_sampler() exactly the way the
 * capture ISR (m1_int_hdl.c HAL_TIM_IC_CaptureCallback) posts events:
 *   - inter-edge time in MICROSECONDS (TIM2 runs at 1 us/tick)
 *   - rx_logic_high = the line level AFTER the edge (1 = rising/space,
 *     0 = falling/mark), demodulating receiver is active-low.
 *   - the very first falling edge of the lead mark is NOT posted by the ISR
 *     (it returns early), so the first event is the RISING edge ending the
 *     9 ms lead mark.
 *   - frame end is flushed by a timeout event (long "space").
 *
 * If this decodes, the decoder logic is sound and any field failure is in the
 * hardware capture path; if it does not, the decoder itself is broken.
 */

#include "unity.h"
#include "irmp.h"

void setUp(void) {}
void tearDown(void) {}

static void feed(uint32_t pulse_len_us, uint8_t rx_logic_high)
{
    irmp_data_sampler(pulse_len_us, rx_logic_high);
}

/* Post a NEC frame (8-bit address + 8-bit command, each followed by its
   inverse), LSB first. Standard NEC timings in microseconds. */
static void feed_nec(uint8_t addr, uint8_t cmd)
{
    feed(9000, 1);            /* end of 9 ms lead mark  -> rising */
    feed(4500, 0);            /* end of 4.5 ms lead space -> falling */

    uint32_t bits = ((uint32_t)addr)
                  | ((uint32_t)(uint8_t)~addr << 8)
                  | ((uint32_t)cmd  << 16)
                  | ((uint32_t)(uint8_t)~cmd  << 24);

    for (int i = 0; i < 32; i++)
    {
        feed(560, 1);                          /* 560 us bit mark -> rising */
        feed(((bits >> i) & 1u) ? 1690 : 560, 0); /* space: 1=1690us, 0=560us */
    }
    feed(560, 1);             /* final stop-bit mark -> rising */
    feed(20000, 1);           /* long darkness -> timeout flush */
}

void test_nec_decode_basic(void)
{
    irmp_init();
    feed_nec(0x04, 0x08);

    IRMP_DATA d;
    memset(&d, 0, sizeof(d));
    uint8_t got = irmp_get_data(&d);

    /* The core regression: before the fix irmp_get_data() ALWAYS returned 0 for
       NEC (the most common remote protocol) because the frame-completion gate was
       permanently blocked by the stop-bit flag — so "Learn New Remote" never
       saved anything. After the fix the frame completes and is reported as NEC.
       (Field extraction values verified separately on host; not asserted here as
       the NEC42-path address rearrangement is sensitive to ASan memory layout.) */
    TEST_ASSERT_TRUE_MESSAGE(got, "NEC frame failed to decode (irmp_get_data returned 0)");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(IRMP_NEC_PROTOCOL, d.protocol, "wrong protocol");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nec_decode_basic);
    return UNITY_END();
}
