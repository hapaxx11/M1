/* See COPYING.txt for license details. */

/*
*
*  m1_sub_ghz_decenc.h
*
*  M1 sub-ghz decoding encoding
*
* M1 Project
*
*/
#ifndef _M1_SUB_GHZ_DECENC_H
#define _M1_SUB_GHZ_DECENC_H

#include <stdint.h>
#include <stdbool.h>
#include "m1_sub_ghz.h"

#define SUBGHZ_RAW_DATA_PULSE_COUNT_MAX		20

#define INTERPACKET_GAP_MIN					1500 // uS
#define INTERPACKET_GAP_MAX					80000//5000 // uS
#define PACKET_PULSE_TIME_MIN				120 // uS
#define PACKET_PULSE_COUNT_MIN				20 // 10 bits
#define PACKET_PULSE_COUNT_MAX				256 // 128 bits

#define PACKET_PULSE_TIME_TOLERANCE20		20 // percentage
#define PACKET_PULSE_TIME_TOLERANCE25		25
#define PACKET_PULSE_TIME_TOLERANCE30		30

typedef struct SubGHz_protocol
{
    uint16_t te_short;
    uint16_t te_long;
    uint8_t te_tolerance;
    uint8_t preamble_bits;
    uint16_t data_bits;
} SubGHz_protocol_t;


typedef struct
{
    bool (*subghz_data_ready)(void);
    bool (*subghz_raw_data_ready)(void);
    void (*subghz_reset_data)(void);
    uint64_t (*subghz_get_decoded_value)(void);
    uint64_t (*subghz_get_decoded_value_hi)(void);
    uint16_t (*subghz_get_decoded_bitlength)(void);
    uint16_t (*subghz_get_decoded_delay)(void);
    uint16_t (*subghz_get_decoded_protocol)(void);
    int16_t (*subghz_get_decoded_rssi)(void);
    uint16_t *(*subghz_get_rawdata)(void);
    uint8_t (*subghz_pulse_handler)(uint16_t duration);
    bool (*subghz_decode_protocol)(const uint16_t p, uint16_t changeCount);

    uint64_t n64_decodedvalue;
    uint64_t n64_decodedvalue_hi;
    uint32_t n32_serialnumber;
    uint32_t n32_rollingcode;
    uint8_t n8_buttonid;
    int16_t ndecodedrssi;
    uint16_t ndecodedbitlength;
    uint16_t ndecodeddelay;
    uint16_t ndecodedprotocol;
    uint16_t npulsecount;
    uint16_t ntx_raw_repeat;
    uint32_t ntx_raw_len;
    uint32_t ntx_raw_src;
    uint32_t ntx_raw_dest;
    char detail_line1[32];
    char detail_line2[32];
    char detail_line3[32];
    volatile uint8_t pulse_det_stat; // Updated in interrupt
    volatile uint8_t pulse_det_pol; // Updated in interrupt
    uint16_t pulse_times[PACKET_PULSE_COUNT_MAX];
} SubGHz_DecEnc_t;

typedef struct
{
    uint32_t frequency;
    uint64_t key;
    uint64_t key_hi;
    uint16_t protocol;
    int16_t rssi;
    uint16_t *raw_data;
    uint16_t te;
    uint16_t bit_len;
    char detail_line1[32];
    char detail_line2[32];
    char detail_line3[32];
    bool raw;
} SubGHz_Dec_Info_t;

enum {
	PRINCETON = 0,
	SECURITY_PLUS_20,
	SECURITY_PLUS_10,
	CAME,
	NICE_FLO,
	LINEAR,
	HOLTEK,
	HORMANN,
	DOITRAND,
	DOOYA,
	CAME_ATOMO,
	CAME_TWEE,
	ANSONIC,
	BETT,
	CLEMSA,
	GATE_TX,
	MARANTEC,
	MASTERCODE,
	SMC5326,
	HOLTEK_HT12X,
	NERO_RADIO,
	NICE_FLOR_S,
	ALUTECH_AT_4N,
	KINGGATES_STYLO_4K,
	HONEYWELL_WDB,
	MEGACODE,
	CHAMBERLAIN_CODE,
	LINEAR_DELTA3,
	FERON,
	GANGQI,
	HAY21,
	MARANTEC24,
	ROGER,
	HOLLARM,
	KEELOQ,
	STAR_LINE,
	FAAC_SLH,
	PHOENIX_V2,
	MAGELLAN,
	LEGRAND,
	DICKERT_MAHS,
	KIA,
	SCHER_KHAN,
	IDO,
	NERO_SKETCH,
	INTERTECHNO_V3,
};

extern SubGHz_DecEnc_t subghz_decenc_ctl;
extern const char *protocol_text[];
extern const SubGHz_protocol_t subghz_protocols_list[];

void subghz_decenc_init(void);
bool subghz_decenc_read(SubGHz_Dec_Info_t *received, bool raw);
uint16_t get_diff(uint16_t n_a, uint16_t n_b);
uint8_t subghz_decode_princeton(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_security_plus_20(uint16_t p, uint16_t pulsecount);
uint8_t m1_secplus_v2_decode(uint32_t fixed[], uint8_t half_codes[][10], uint32_t *rolling_code, uint64_t *out_bits);
uint8_t m1_secplus_v2_decode_half(uint64_t in_bits, uint8_t *half_code, uint32_t *out_bits);

#endif // #ifndef _M1_SUB_GHZ_DECENC_H
