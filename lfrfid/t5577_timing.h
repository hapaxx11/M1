/* See COPYING.txt for license details. */

/*
 * t5577_timing.h — T5577 LF RFID write-timing constants
 *
 * All values are in field clocks (Tc) at 125 kHz (1 Tc = 8 us).
 * The gap-to-gap time seen by the T5577 is DATA_x + WRITE_GAP.
 *
 * T5577 datasheet requirements:
 *   "0" bit: gap-to-gap 16..32 Tc (typ 24)
 *   "1" bit: gap-to-gap 48..64 Tc (typ 56)
 *   Write gap: 8..20 Tc (typ 10)
 *
 * Fix courtesy of da-pingwing (github.com/da-pingwing/M1_T-1000_RFID, GPL-3.0).
 * The previous values (DATA_0=24, DATA_1=56, WRITE_GAP=18) put the "1" bit
 * gap-to-gap at 74 Tc — out of spec — causing the chip to latch wrong data
 * and produce a consistent but incorrect clone readback.
 */

#ifndef T5577_TIMING_H_
#define T5577_TIMING_H_

/* Field charge / reset timings (Tc). */
#define T5577_TIMING_WAIT_TIME  500   /* ~4 ms field charge before first gap (>= 3 ms) */
#define T5577_TIMING_START_GAP   30   /* initial start gap */
#define T5577_TIMING_PROGRAM    700   /* programming pulse after block write */

/* Write bit timings (Tc). gap-to-gap = DATA_x + WRITE_GAP. */
#define T5577_TIMING_WRITE_GAP   10   /* field-off gap (8..20 Tc, typ 10) */
#define T5577_TIMING_DATA_0      14   /* "0": DATA_0 + WRITE_GAP = 24 Tc (16..32 spec) */
#define T5577_TIMING_DATA_1      46   /* "1": DATA_1 + WRITE_GAP = 56 Tc (48..64 spec) */

/* Compile-time spec assertions — catch out-of-spec constants at build time. */
#if (T5577_TIMING_DATA_0 + T5577_TIMING_WRITE_GAP) < 16 || \
    (T5577_TIMING_DATA_0 + T5577_TIMING_WRITE_GAP) > 32
#error "T5577 '0' bit gap-to-gap out of spec [16..32 Tc]"
#endif

#if (T5577_TIMING_DATA_1 + T5577_TIMING_WRITE_GAP) < 48 || \
    (T5577_TIMING_DATA_1 + T5577_TIMING_WRITE_GAP) > 64
#error "T5577 '1' bit gap-to-gap out of spec [48..64 Tc]"
#endif

#if T5577_TIMING_WRITE_GAP < 8 || T5577_TIMING_WRITE_GAP > 20
#error "T5577 write gap out of spec [8..20 Tc]"
#endif

#endif /* T5577_TIMING_H_ */
