/* Minimal globals stub for host-side protocol-decoder unit tests.
 *
 * The M1 Sub-GHz decoders read pulse data from the global
 * subghz_decenc_ctl struct and write decoded values back into it.
 * In production this struct is defined in m1_sub_ghz_decenc.c (which has
 * STM32/FreeRTOS dependencies).  This stub provides only the globals
 * that the decoders actually access, so they link on the host.
 *
 * Also provides an external definition for get_diff(), which is declared
 * as `inline` in m1_sub_ghz_decenc.h but whose definition (in
 * m1_sub_ghz_decenc.c) is not included in test builds.
 */

#include <stdint.h>
#include <stdlib.h>        /* abs() */
#include "m1_sub_ghz_decenc.h"

/* ---------------------------------------------------------------------- */
/* Global decode-state struct used by all protocol decoders               */
/* ---------------------------------------------------------------------- */
SubGHz_DecEnc_t subghz_decenc_ctl;

/* ---------------------------------------------------------------------- */
/* Legacy protocol-list pointer.                                          */
/* Decoders access timing via subghz_protocols_list[p].te_short etc.,    */
/* which is a macro for subghz_protocols_list_ptr[p].                    */
/* Tests initialise the desired entry before calling the decoder.         */
/* ---------------------------------------------------------------------- */
static SubGHz_protocol_t _proto_storage[8]; /* space for a few test protos */
SubGHz_protocol_t *subghz_protocols_list_ptr = _proto_storage;

/* Legacy protocol-name table (unused by the decoders under test) */
static const char *_text_storage[8];
const char **protocol_text_ptr = _text_storage;

/* ---------------------------------------------------------------------- */
/* get_diff — external definition required by decoders.                  */
/* Declared `inline` in m1_sub_ghz_decenc.h / defined in decenc.c.       */
/* We provide the external definition here for the test link unit.        */
/* ---------------------------------------------------------------------- */
uint16_t get_diff(uint16_t n_a, uint16_t n_b)
{
    return (uint16_t)abs((int)n_a - (int)n_b);
}

/* ---------------------------------------------------------------------- */
/* Stub for subghz_get_weather_data — declared in m1_sub_ghz_decenc.h,   */
/* referenced by some TUs but not called by the decoders under test.      */
/* ---------------------------------------------------------------------- */
const SubGHz_Weather_Data_t *subghz_get_weather_data(void)
{
    return NULL;
}
