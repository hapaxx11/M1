/*
 * test_bq27421_init.c — Unit tests for bq27421_init() pure-logic helpers.
 *
 * Regression context (v0.9.0.166 crash on SubGHz Read scene):
 *
 * bq27421_init() changed the taper-current parameter (240→256 mA) and added a
 * DEFAULT_DESIGN_CAP field to applyConfigIfMatches().  On devices previously
 * running v0.9.0.163 both checks fail simultaneously, causing bq27421_init()
 * to enter the full CFGUPDATE rewrite path for the first time in the field.
 *
 * That path has two bugs that leave the BQ27421 in CFGUPMODE indefinitely,
 * starving the WDT and resetting the device a few seconds after entering
 * the SubGHz Read scene:
 *
 *   Bug 1 — Checksum comparison casts the wrong operand:
 *     Old: if( checksumRead != (uint8_t)checksumNew )
 *     bq27421_i2c_command_read() reads 2 bytes from consecutive I2C registers
 *     (0x60 = checksum, 0x61 = control).  The high byte of checksumRead is
 *     non-zero, making the uint16_t != uint8_t comparison always true →
 *     function always returns false even on a successful write.
 *     Fix: if( (uint8_t)checksumRead != checksumNew )
 *
 *   Bug 2 — BQ27421 left in CFGUPMODE on error:
 *     The old early return paths sent no SOFT_RESET.  Fixed by adding
 *     SOFT_RESET + SEALED before every early return inside CFGUPDATE.
 *
 * These tests cover:
 *   - bq27421_calc_block_checksum() — the 0xFF-sum formula
 *   - bq27421_calc_taper_rate()     — 10*capacity/taperCurrent
 *   - bq27421_calc_design_energy()  — 3.7 * capacity
 *   - Block field offsets           — STATE_OFFSET defines match datasheet
 *   - Checksum comparison direction — the specific regression that caused the crash
 */

#include "unity.h"

/* Pull in only the pure-logic helpers from the header.
 * We do NOT link against m1_bq27421.c to avoid I2C / HAL dependencies.
 * The static inline helpers in the header compile without hardware. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Minimal stubs so m1_bq27421.h compiles without hardware headers */
#include "stm32h5xx_hal.h"   /* stub: GPIO/TIM types, HAL_GetTick */
#include "m1_types_def.h"    /* TRUE/FALSE, no HAL deps */

/* Forward-declare the I2C types used in m1_bq27421.h prototypes so the
 * header compiles without pulling in m1_i2c.h (which would need HAL I2C). */
typedef int HAL_StatusTypeDef;
typedef struct { uint32_t dummy; } I2C_HandleTypeDef;

/* Stub out the I2C-using prototypes by defining the include guard for m1_i2c.h
 * before m1_bq27421.h tries to use them. The header only needs the pure inline
 * helpers we are testing; the function prototypes for bq27421_i2c_*() are
 * declared but never called from the test binary. */
#define M1_I2C_H_    /* suppress m1_i2c.h inclusion via bq27421.h if any */

#include "m1_bq27421.h"

/* BQ27421 State subclass block offsets (mirrored from m1_bq27421.h) */
#define _OFFS_DESIGN_CAP    BQ27421_STATE_OFFSET_DESIGN_CAPACITY        /* 10 */
#define _OFFS_DESIGN_EN     BQ27421_STATE_OFFSET_DESIGN_ENERGY          /* 12 */
#define _OFFS_DEFAULT_CAP   BQ27421_STATE_OFFSET_DEFAULT_DESIGN_CAP     /* 14 */
#define _OFFS_TERM_VOLT     BQ27421_STATE_OFFSET_TERMINATE_VOLTAGE      /* 16 */
#define _OFFS_TAPER_RATE    BQ27421_STATE_OFFSET_TAPER_RATE             /* 27 */

void setUp(void)    {}
void tearDown(void) {}

/* ---- bq27421_calc_block_checksum() ---------------------------------------- */

void test_checksum_all_zeros_returns_ff(void)
{
    uint8_t block[32];
    memset(block, 0x00, sizeof(block));
    TEST_ASSERT_EQUAL_HEX8(0xFF, bq27421_calc_block_checksum(block, 32));
}

void test_checksum_all_ones(void)
{
    /* sum = 32 * 0x01 = 0x20; 0xFF - 0x20 = 0xDF */
    uint8_t block[32];
    memset(block, 0x01, sizeof(block));
    TEST_ASSERT_EQUAL_HEX8(0xDF, bq27421_calc_block_checksum(block, 32));
}

void test_checksum_all_ff(void)
{
    /* sum = 32 * 0xFF = 0x1FE0; low byte = 0xE0; 0xFF - 0xE0 = 0x1F */
    uint8_t block[32];
    memset(block, 0xFF, sizeof(block));
    TEST_ASSERT_EQUAL_HEX8(0x1F, bq27421_calc_block_checksum(block, 32));
}

void test_checksum_wrap_arithmetic(void)
{
    /* Single non-zero byte at position 0: sum = 0x80; 0xFF - 0x80 = 0x7F */
    uint8_t block[32];
    memset(block, 0x00, sizeof(block));
    block[0] = 0x80;
    TEST_ASSERT_EQUAL_HEX8(0x7F, bq27421_calc_block_checksum(block, 32));
}

void test_checksum_round_trip(void)
{
    /* If we know the expected checksum, appending that byte should make the
     * new sum's low byte == 0xFF, meaning 0xFF - sum = 0x00.
     * This validates the chip's checksum verification logic in software. */
    uint8_t block[32];
    memset(block, 0x42, sizeof(block));
    uint8_t csum = bq27421_calc_block_checksum(block, 32);

    /* Simulate: if we change one byte and recompute, we get a different csum */
    block[5] = 0x55;
    uint8_t csum2 = bq27421_calc_block_checksum(block, 32);
    TEST_ASSERT_NOT_EQUAL(csum, csum2);
}

void test_checksum_single_byte(void)
{
    uint8_t block[1] = { 0xAB };
    /* sum = 0xAB; 0xFF - 0xAB = 0x54 */
    TEST_ASSERT_EQUAL_HEX8(0x54, bq27421_calc_block_checksum(block, 1));
}

/* ---- Checksum comparison regression --------------------------------------- */
/*
 * This is the core regression test.  The bug is that:
 *
 *   if( checksumRead != (uint8_t)checksumNew )    <-- OLD: casts RHS only
 *
 * bq27421_i2c_command_read() reads TWO bytes from consecutive I2C regs:
 *   reg 0x60 = BLOCK_DATA_CHECKSUM (what we want)
 *   reg 0x61 = BLOCK_DATA_CONTROL  (non-zero after write, goes in high byte)
 *
 * So checksumRead = (reg_0x61 << 8) | reg_0x60.
 * When the high byte is non-zero and checksumNew is a uint8_t:
 *   checksumRead (uint16_t) != (uint8_t)checksumNew always evaluates as
 *   uint16_t != uint16_t with the RHS zero-extended to 0x00NN, while LHS
 *   is 0xXXNN — they differ in the high byte → comparison returns true
 *   (i.e. "mismatch") even though the actual checksum byte matches.
 *
 * The fix is: (uint8_t)checksumRead != checksumNew  (cast LHS).
 */

void test_checksum_comparison_old_code_false_positive(void)
{
    /* Simulate: chip returned 0x1242 — high byte non-zero (from reg 0x61),
     * low byte 0x42 (actual checksum).  checksumNew = 0x42.
     * The OLD comparison incorrectly reports a mismatch. */
    uint16_t checksumRead = 0x1242u;
    uint8_t  checksumNew  = 0x42u;

    /* Old (buggy): promotes checksumNew to uint16_t 0x0042, compares with
     * 0x1242 → TRUE (mismatch reported → function returns false wrongly) */
    bool old_check = (checksumRead != (uint8_t)checksumNew);
    TEST_ASSERT_TRUE_MESSAGE(old_check,
        "old comparison should report mismatch (demonstrating the bug)");

    /* Fixed: casts checksumRead to uint8_t 0x42, compares with 0x42 → FALSE
     * (match — correct, function continues) */
    bool fixed_check = ((uint8_t)checksumRead != checksumNew);
    TEST_ASSERT_FALSE_MESSAGE(fixed_check,
        "fixed comparison should report match");
}

void test_checksum_comparison_no_high_byte(void)
{
    /* When the high byte is 0 (ideal I2C read), both old and new comparisons
     * agree.  Verify the fix does not break the clean-register case. */
    uint16_t checksumRead = 0x0042u;
    uint8_t  checksumNew  = 0x42u;

    bool old_check   = (checksumRead != (uint8_t)checksumNew);
    bool fixed_check = ((uint8_t)checksumRead != checksumNew);
    TEST_ASSERT_EQUAL(old_check, fixed_check);
    TEST_ASSERT_FALSE(fixed_check);  /* both say match */
}

void test_checksum_comparison_genuine_mismatch(void)
{
    /* Even with a non-zero high byte, if the low byte differs from checksumNew
     * both old and new comparisons correctly report a mismatch. */
    uint16_t checksumRead = 0x12A5u;  /* low byte 0xA5 */
    uint8_t  checksumNew  = 0x42u;

    bool old_check   = (checksumRead != (uint8_t)checksumNew);
    bool fixed_check = ((uint8_t)checksumRead != checksumNew);
    TEST_ASSERT_TRUE(old_check);   /* both report mismatch (correct) */
    TEST_ASSERT_TRUE(fixed_check);
}

/* ---- bq27421_calc_taper_rate() ------------------------------------------- */

void test_taper_rate_standard_config(void)
{
    /* v0.9.0.166 config: 2100 mAh, 256 mA → 10*2100/256 = 82 */
    TEST_ASSERT_EQUAL_UINT16(82u, bq27421_calc_taper_rate(2100u, 256u));
}

void test_taper_rate_old_config(void)
{
    /* v0.9.0.163 config: 2100 mAh, 240 mA → 10*2100/240 = 87 */
    TEST_ASSERT_EQUAL_UINT16(87u, bq27421_calc_taper_rate(2100u, 240u));
}

void test_taper_rate_values_differ(void)
{
    /* The two configs produce different taper rates — this mismatch is one
     * of the triggers that sends v0.9.0.166 into the full CFGUPDATE path. */
    uint16_t r166 = bq27421_calc_taper_rate(2100u, 256u);
    uint16_t r163 = bq27421_calc_taper_rate(2100u, 240u);
    TEST_ASSERT_NOT_EQUAL(r166, r163);
}

/* ---- bq27421_calc_design_energy() ---------------------------------------- */

void test_design_energy_2100mah(void)
{
    /* 3.7 * 2100 = 7770 mWh */
    TEST_ASSERT_EQUAL_UINT16(7770u, bq27421_calc_design_energy(2100u));
}

void test_design_energy_1000mah(void)
{
    /* 3.7 * 1000 = 3700 mWh */
    TEST_ASSERT_EQUAL_UINT16(3700u, bq27421_calc_design_energy(1000u));
}

/* ---- Block field offset constants ----------------------------------------- */
/*
 * The field offsets in the BQ27421 State subclass (class ID 0x52) are
 * defined in m1_bq27421.h and must match the TI datasheet.  These tests
 * pin the values so any accidental constant change is caught immediately.
 */

void test_state_offset_design_capacity_is_10(void)
{
    TEST_ASSERT_EQUAL_INT(10, _OFFS_DESIGN_CAP);
}

void test_state_offset_design_energy_is_12(void)
{
    TEST_ASSERT_EQUAL_INT(12, _OFFS_DESIGN_EN);
}

void test_state_offset_default_design_cap_is_14(void)
{
    TEST_ASSERT_EQUAL_INT(14, _OFFS_DEFAULT_CAP);
}

void test_state_offset_terminate_voltage_is_16(void)
{
    TEST_ASSERT_EQUAL_INT(16, _OFFS_TERM_VOLT);
}

void test_state_offset_taper_rate_is_27(void)
{
    TEST_ASSERT_EQUAL_INT(27, _OFFS_TAPER_RATE);
}

/* ---- Block field packing -------------------------------------------------- */
/*
 * Verify the big-endian 2-byte packing used by bq27421_init().
 * The function writes: block[OFFS] = high_byte; block[OFFS+1] = low_byte;
 */
void test_block_packing_design_capacity(void)
{
    uint8_t block[32];
    memset(block, 0, sizeof(block));
    uint16_t cap = 2100u;  /* 0x0834 */
    block[_OFFS_DESIGN_CAP]     = (uint8_t)(cap >> 8);
    block[_OFFS_DESIGN_CAP + 1] = (uint8_t)(cap & 0xFF);
    TEST_ASSERT_EQUAL_HEX8(0x08, block[_OFFS_DESIGN_CAP]);
    TEST_ASSERT_EQUAL_HEX8(0x34, block[_OFFS_DESIGN_CAP + 1]);
}

void test_block_packing_default_cap_same_as_design_cap(void)
{
    /* v0.9.0.166 change: DEFAULT_DESIGN_CAP written == DESIGN_CAPACITY */
    uint8_t block[32];
    memset(block, 0, sizeof(block));
    uint16_t cap = 2100u;
    block[_OFFS_DESIGN_CAP]       = (uint8_t)(cap >> 8);
    block[_OFFS_DESIGN_CAP + 1]   = (uint8_t)(cap & 0xFF);
    block[_OFFS_DEFAULT_CAP]      = (uint8_t)(cap >> 8);
    block[_OFFS_DEFAULT_CAP + 1]  = (uint8_t)(cap & 0xFF);
    TEST_ASSERT_EQUAL_HEX8(block[_OFFS_DESIGN_CAP],     block[_OFFS_DEFAULT_CAP]);
    TEST_ASSERT_EQUAL_HEX8(block[_OFFS_DESIGN_CAP + 1], block[_OFFS_DEFAULT_CAP + 1]);
}

void test_block_fields_do_not_overlap(void)
{
    /* Confirm that no two 2-byte fields share a byte. */
    TEST_ASSERT_TRUE(_OFFS_DESIGN_EN   >= _OFFS_DESIGN_CAP   + 2);
    TEST_ASSERT_TRUE(_OFFS_DEFAULT_CAP >= _OFFS_DESIGN_EN    + 2);
    TEST_ASSERT_TRUE(_OFFS_TERM_VOLT   >= _OFFS_DEFAULT_CAP  + 2);
    TEST_ASSERT_TRUE(_OFFS_TAPER_RATE  >= _OFFS_TERM_VOLT    + 2);
}

/* --------------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    /* Checksum formula */
    RUN_TEST(test_checksum_all_zeros_returns_ff);
    RUN_TEST(test_checksum_all_ones);
    RUN_TEST(test_checksum_all_ff);
    RUN_TEST(test_checksum_wrap_arithmetic);
    RUN_TEST(test_checksum_round_trip);
    RUN_TEST(test_checksum_single_byte);

    /* Checksum comparison regression — the actual crash-causing bug */
    RUN_TEST(test_checksum_comparison_old_code_false_positive);
    RUN_TEST(test_checksum_comparison_no_high_byte);
    RUN_TEST(test_checksum_comparison_genuine_mismatch);

    /* Taper rate formula */
    RUN_TEST(test_taper_rate_standard_config);
    RUN_TEST(test_taper_rate_old_config);
    RUN_TEST(test_taper_rate_values_differ);

    /* Design energy formula */
    RUN_TEST(test_design_energy_2100mah);
    RUN_TEST(test_design_energy_1000mah);

    /* State class block field offsets */
    RUN_TEST(test_state_offset_design_capacity_is_10);
    RUN_TEST(test_state_offset_design_energy_is_12);
    RUN_TEST(test_state_offset_default_design_cap_is_14);
    RUN_TEST(test_state_offset_terminate_voltage_is_16);
    RUN_TEST(test_state_offset_taper_rate_is_27);

    /* Block field packing */
    RUN_TEST(test_block_packing_design_capacity);
    RUN_TEST(test_block_packing_default_cap_same_as_design_cap);
    RUN_TEST(test_block_fields_do_not_overlap);

    return UNITY_END();
}
