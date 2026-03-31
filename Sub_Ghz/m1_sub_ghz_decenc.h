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
/* Minimum pulse count to trigger decode.  Set to 40 so that Security+ 1.0
 * sub-packets (41-42 pulses each) are captured.  Princeton 24-bit generates
 * 48 pulses so is unaffected.  Shorter protocols (CAME 12-bit = 24 pulses)
 * still fall below this threshold unless they repeat within one capture window. */
#define PACKET_PULSE_COUNT_MIN				40 // was 48 (24 bits)
#define PACKET_PULSE_COUNT_MAX				256 // 128 bits (weather protocols need longer packets)

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
    uint16_t (*subghz_get_decoded_bitlength)(void);
    uint16_t (*subghz_get_decoded_delay)(void);
    uint16_t (*subghz_get_decoded_protocol)(void);
    int16_t (*subghz_get_decoded_rssi)(void);
    uint16_t *(*subghz_get_rawdata)(void);
    uint8_t (*subghz_pulse_handler)(uint16_t duration);
    bool (*subghz_decode_protocol)(const uint16_t p, uint16_t changeCount);

    uint64_t n64_decodedvalue;
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
    volatile uint8_t pulse_det_stat; // Updated in interrupt
    volatile uint8_t pulse_det_pol; // Updated in interrupt
    uint16_t pulse_times[PACKET_PULSE_COUNT_MAX];
} SubGHz_DecEnc_t;

typedef struct
{
    uint32_t frequency;
    uint64_t key;
    uint16_t protocol;
    int16_t rssi;
    uint16_t *raw_data;
    uint16_t te;
    uint16_t bit_len;
    bool raw;
    /* Extended fields — populated by protocol decoders that set them */
    uint32_t serial_number;
    uint32_t rolling_code;
    uint8_t  button_id;
} SubGHz_Dec_Info_t;

/*============================================================================*/
/* Signal History Ring Buffer                                                  */
/*============================================================================*/
#define SUBGHZ_HISTORY_MAX   50   /* Maximum signals stored in history */

typedef struct {
    SubGHz_Dec_Info_t info;       /* Decoded signal data (no raw_data ptr) */
    uint32_t frequency;           /* Capture frequency in Hz */
    uint8_t  count;               /* Duplicate reception count (1 = first) */
} SubGHz_History_Entry_t;

typedef struct {
    SubGHz_History_Entry_t entries[SUBGHZ_HISTORY_MAX];
    uint8_t  count;               /* Number of valid entries (0..SUBGHZ_HISTORY_MAX) */
    uint8_t  head;                /* Next write position (circular) */
} SubGHz_History_t;

void subghz_history_reset(SubGHz_History_t *hist);
uint8_t subghz_history_add(SubGHz_History_t *hist, const SubGHz_Dec_Info_t *info, uint32_t freq_hz);
const SubGHz_History_Entry_t *subghz_history_get(const SubGHz_History_t *hist, uint8_t idx);

enum {
	PRINCETON = 0,
	SECURITY_PLUS_20,
	CAME_12BIT,
	NICE_FLO,
	LINEAR_10BIT,
	HOLTEK_HT12E,
	KEELOQ,
	OREGON_V2,
	ACURITE,
	LACROSSE_TX,
	FAAC_SLH,
	HORMANN,
	MARANTEC,
	SOMFY_TELIS,
	STAR_LINE,
	GATE_TX,
	SMC5326,
	POWER_SMART,
	IDO,
	ANSONIC,
	INFACTORY,
	SCHRADER_TPMS,
	/* --- New protocols (added after original 23) --- */
	CHAMBERLAIN,
	CLEMSA,
	DOITRAND,
	BETT,
	NERO_RADIO,
	FIREFLY,
	CAME_TWEE,
	CAME_ATOMO,
	NICE_FLOR_S,
	ALUTECH_AT4N,
	CENTURION,
	KINGGATES_STYLO,
	MEGACODE,
	MASTERCODE,
	CHAMBERLAIN_7BIT,
	CHAMBERLAIN_8BIT,
	CHAMBERLAIN_9BIT,
	LIFTMASTER_10BIT,
	DOOYA,
	HONEYWELL,
	INTERTECHNO,
	ELRO,
	AMBIENT_WEATHER,
	BRESSER_3CH,
	BRESSER_5IN1,
	BRESSER_6IN1,
	TFA_DOSTMANN,
	NEXUS_TH,
	THERMOPRO_TX2,
	GT_WT03,
	SCHER_KHAN_MAGICAR,
	SCHER_KHAN_LOGICAR,
	TOYOTA,
	BIN_RAW,
	/* --- Flipper compatibility protocols --- */
	DICKERT_MAHS,
	FERON,
	GANGQI,
	HAY21,
	HOLLARM,
	HOLTEK_BASE,
	INTERTECHNO_V3,
	KIA_SEED,
	LEGRAND,
	LINEAR_DELTA3,
	MAGELLAN,
	MARANTEC24,
	NERO_SKETCH,
	PHOENIX_V2,
	REVERS_RB2,
	ROGER,
	SOMFY_KEYTIS,
	/* --- Specialty protocols --- */
	TREADMILL37,
	POCSAG,
	TPMS_GENERIC,
	PCSG_GENERIC,
};

/* Weather station decoded data */
typedef struct {
    uint16_t id;            /* Sensor ID */
    uint8_t  channel;       /* Channel (1-8) */
    int16_t  temp_raw;      /* Temperature in 0.1 deg C units */
    uint8_t  humidity;      /* Humidity 0-100% */
    uint8_t  battery_low;   /* 1 = battery low */
    uint8_t  valid;         /* 1 = checksum/CRC passed */
} SubGHz_Weather_Data_t;

extern SubGHz_DecEnc_t subghz_decenc_ctl;

/*
 * Legacy compatibility: protocol_text[] and subghz_protocols_list[] are now
 * populated at init time from the protocol registry (subghz_protocol_registry.h).
 * These pointers provide backward-compatible array-style indexing:
 *   protocol_text[i]           → protocol name string
 *   subghz_protocols_list[i]   → timing parameters
 *
 * For new code, prefer subghz_protocol_registry[i].name and .timing directly.
 */
extern const char **protocol_text_ptr;
extern SubGHz_protocol_t *subghz_protocols_list_ptr;

/* Redirect old names to pointers so existing code compiles unchanged */
#define protocol_text          protocol_text_ptr
#define subghz_protocols_list  subghz_protocols_list_ptr

void subghz_decenc_init(void);
bool subghz_decenc_read(SubGHz_Dec_Info_t *received, bool raw);
uint16_t get_diff(uint16_t n_a, uint16_t n_b);

/* Protocol decoders */
uint8_t subghz_decode_princeton(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_security_plus_20(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_came(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_nice_flo(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_linear(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_holtek(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_keeloq(uint16_t p, uint16_t pulsecount);

/* Weather station decoders */
uint8_t subghz_decode_oregon_v2(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_acurite(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_lacrosse_tx(uint16_t p, uint16_t pulsecount);

/* Additional protocol decoders */
uint8_t subghz_decode_faac_slh(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_hormann(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_marantec(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_somfy_telis(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_starline(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_gate_tx(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_smc5326(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_power_smart(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_ido(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_ansonic(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_infactory(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_schrader(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_bin_raw(uint16_t p, uint16_t pulsecount);

/* Generic decoders */
uint8_t subghz_decode_generic_pwm(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_generic_manchester(uint16_t p, uint16_t pulsecount);

/* New protocol decoders */
uint8_t subghz_decode_chamberlain(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_clemsa(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_doitrand(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_bett(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_nero(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_firefly(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_came_twee(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_came_atomo(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_nice_flor_s(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_alutech(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_centurion(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_kinggates(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_megacode(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_mastercode(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_chamberlain_7bit(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_chamberlain_8bit(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_chamberlain_9bit(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_liftmaster(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_dooya(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_honeywell(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_intertechno(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_elro(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_ambient_weather(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_bresser_3ch(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_bresser_5in1(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_bresser_6in1(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_tfa_dostmann(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_nexus_th(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_thermopro_tx2(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_gt_wt03(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_scher_khan_magicar(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_scher_khan_logicar(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_toyota(uint16_t p, uint16_t pulsecount);

/* Flipper compatibility protocol decoders */
uint8_t subghz_decode_dickert_mahs(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_feron(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_gangqi(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_hay21(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_hollarm(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_holtek_base(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_intertechno_v3(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_kia_seed(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_legrand(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_linear_delta3(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_magellan(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_marantec24(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_nero_sketch(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_phoenix_v2(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_revers_rb2(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_roger(uint16_t p, uint16_t pulsecount);
uint8_t subghz_decode_somfy_keytis(uint16_t p, uint16_t pulsecount);

/* Weather data access */
const SubGHz_Weather_Data_t* subghz_get_weather_data(void);

uint8_t m1_secplus_v2_decode(uint32_t fixed[], uint8_t half_codes[][10], uint32_t *rolling_code, uint64_t *out_bits);
uint8_t m1_secplus_v2_decode_half(uint64_t in_bits, uint8_t *half_code, uint32_t *out_bits);

#endif // #ifndef _M1_SUB_GHZ_DECENC_H
