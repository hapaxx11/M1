/* See COPYING.txt for license details. */

/*
*
*  m1_sub_ghz_decenc.c
*
*  M1 sub-ghz decoding encoding
*
* M1 Project
*
*/

/*************************** I N C L U D E S **********************************/
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include "stm32h5xx_hal.h"
#include <m1_sub_ghz_decenc.h>
#include "m1_sub_ghz.h"
#include "m1_sub_ghz_api.h"
#include "si446x_cmd.h"
#include "m1_io_defs.h" // Test only
#include "m1_log_debug.h"

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG	"SUBGHZ_DECENC"

//************************** C O N S T A N T **********************************/

const SubGHz_protocol_t subghz_protocols_list[] =
{
	/*{160, 470, PACKET_PULSE_TIME_TOLERANCE20, 0, 24}, // Princeton: bit 0 |^|___, bit 1 |^^^|_*/
	{370, 1140, PACKET_PULSE_TIME_TOLERANCE20, 0, 24}, // Princeton: bit 0 |^|___, bit 1 |^^^|_
	{250, 500, PACKET_PULSE_TIME_TOLERANCE20, 16, 46}, // Security+ 2.0: bit 0 |^|___, bit 1 |^|_
	{500, 1500, PACKET_PULSE_TIME_TOLERANCE20, 0, 21}, // Security+ 1.0
	{320, 640, PACKET_PULSE_TIME_TOLERANCE30, 0, 42}, // CAME / Prastel / Airforce
	{700, 1400, PACKET_PULSE_TIME_TOLERANCE30, 0, 24}, // Nice FLO
	{500, 1500, PACKET_PULSE_TIME_TOLERANCE30, 0, 10}, // Linear
	{430, 870, PACKET_PULSE_TIME_TOLERANCE25, 0, 40}, // Holtek
	{500, 1000, PACKET_PULSE_TIME_TOLERANCE30, 0, 44}, // Hormann
	{400, 1100, PACKET_PULSE_TIME_TOLERANCE30, 0, 37}, // Doitrand
	{366, 733, PACKET_PULSE_TIME_TOLERANCE30, 0, 40}, // Dooya
	{600, 1200, PACKET_PULSE_TIME_TOLERANCE30, 0, 62}, // CAME Atomo
	{500, 1000, PACKET_PULSE_TIME_TOLERANCE30, 0, 54}, // CAME Twee
	{555, 1111, PACKET_PULSE_TIME_TOLERANCE30, 0, 12}, // Ansonic
	{340, 2000, PACKET_PULSE_TIME_TOLERANCE30, 0, 18}, // BETT
	{385, 2695, PACKET_PULSE_TIME_TOLERANCE30, 0, 18}, // Clemsa
	{350, 700, PACKET_PULSE_TIME_TOLERANCE30, 0, 24}, // Gate TX
	{1000, 2000, PACKET_PULSE_TIME_TOLERANCE25, 0, 49}, // Marantec
	{1072, 2145, PACKET_PULSE_TIME_TOLERANCE25, 0, 36}, // Mastercode
	{300, 900, PACKET_PULSE_TIME_TOLERANCE30, 0, 25}, // SMC5326
	{320, 640, PACKET_PULSE_TIME_TOLERANCE30, 0, 12}, // Holtek HT12x
	{200, 400, PACKET_PULSE_TIME_TOLERANCE30, 0, 56}, // Nero Radio
	{500, 1000, PACKET_PULSE_TIME_TOLERANCE30, 0, 72}, // Nice Flor S / Nice One
	{400, 800, PACKET_PULSE_TIME_TOLERANCE30, 0, 72}, // Alutech AT-4N
	{400, 1100, PACKET_PULSE_TIME_TOLERANCE30, 0, 89}, // Kinggates Stylo 4K
	{160, 320, PACKET_PULSE_TIME_TOLERANCE30, 0, 48}, // Honeywell WDB
	{1000, 1000, PACKET_PULSE_TIME_TOLERANCE20, 0, 24}, // MegaCode
	{1000, 3000, PACKET_PULSE_TIME_TOLERANCE20, 0, 10}, // Chamberlain 7/8/9-bit
	{500, 2000, PACKET_PULSE_TIME_TOLERANCE30, 0, 8}, // Linear Delta3
	{350, 750, PACKET_PULSE_TIME_TOLERANCE30, 0, 32}, // Feron
	{500, 1200, PACKET_PULSE_TIME_TOLERANCE30, 0, 34}, // GangQi
	{300, 700, PACKET_PULSE_TIME_TOLERANCE30, 0, 21}, // Hay21
	{800, 1600, PACKET_PULSE_TIME_TOLERANCE25, 0, 24}, // Marantec24
	{500, 1000, PACKET_PULSE_TIME_TOLERANCE30, 0, 28}, // Roger
	{200, 1000, PACKET_PULSE_TIME_TOLERANCE30, 0, 42}, // Hollarm
	{400, 800, PACKET_PULSE_TIME_TOLERANCE30, 0, 64}, // KeeLoq raw
	{250, 500, PACKET_PULSE_TIME_TOLERANCE30, 0, 64}, // Star Line raw
	{255, 595, PACKET_PULSE_TIME_TOLERANCE30, 0, 64}, // FAAC SLH raw
	{427, 853, PACKET_PULSE_TIME_TOLERANCE30, 0, 52}, // Phoenix V2 raw
	{200, 400, PACKET_PULSE_TIME_TOLERANCE30, 0, 32}, // Magellan raw
	{375, 1125, PACKET_PULSE_TIME_TOLERANCE30, 0, 18}, // Legrand
	{400, 800, PACKET_PULSE_TIME_TOLERANCE30, 0, 36}, // Dickert MAHS
	{250, 500, PACKET_PULSE_TIME_TOLERANCE30, 0, 61}, // Kia raw
	{750, 1100, PACKET_PULSE_TIME_TOLERANCE30, 0, 35}, // Scher-Khan raw
	{450, 1450, PACKET_PULSE_TIME_TOLERANCE30, 0, 48}, // iDo raw
	{330, 660, PACKET_PULSE_TIME_TOLERANCE30, 0, 40}, // Nero Sketch
	{275, 1375, PACKET_PULSE_TIME_TOLERANCE30, 0, 32}, // Intertechno V3
};

const char *protocol_text[] =
{
	"Princeton",
	"Security+ 2.0",
	"Security+ 1.0",
	"CAME",
	"Nice FLO",
	"Linear",
	"Holtek",
	"Hormann",
	"Doitrand",
	"Dooya",
	"CAME Atomo",
	"CAME Twee",
	"Ansonic",
	"BETT",
	"Clemsa",
	"Gate TX",
	"Marantec",
	"Mastercode",
	"SMC5326",
	"Holtek HT12x",
	"Nero Radio",
	"Nice Flor S",
	"Alutech AT-4N",
	"Kinggates 4K",
	"Honeywell WDB",
	"MegaCode",
	"Chamberlain",
	"Linear Delta3",
	"Feron",
	"GangQi",
	"Hay21",
	"Marantec24",
	"Roger",
	"Hollarm",
	"KeeLoq",
	"Star Line",
	"FAAC SLH",
	"Phoenix V2",
	"Magellan",
	"Legrand",
	"Dickert MAHS",
	"Kia",
	"Scher-Khan",
	"iDo",
	"Nero Sketch",
	"Intertechno V3",
};


enum {
   n_protocol = sizeof(subghz_protocols_list) / sizeof(subghz_protocols_list[0])
};


//************************** S T R U C T U R E S *******************************

/***************************** V A R I A B L E S ******************************/

SubGHz_DecEnc_t subghz_decenc_ctl;

typedef struct {
    uint8_t min_bits;
    uint8_t max_bits;
    bool one_is_long_short;
    uint64_t data_mask;
    uint64_t data_match;
} SubGHz_PairRule_t;

typedef struct {
    uint16_t zero_high;
    uint16_t zero_low;
    uint16_t one_high;
    uint16_t one_low;
    uint8_t bits;
    bool allow_last_high_only;
} SubGHz_TimedPairRule_t;

static uint8_t subghz_decode_pair_rule(uint16_t p, uint16_t pulsecount);
static uint8_t subghz_decode_timed_pair_rule(uint16_t p, uint16_t pulsecount);
static uint8_t subghz_decode_secplus_v1(uint16_t p, uint16_t pulsecount);
static uint8_t subghz_decode_megacode(uint16_t p, uint16_t pulsecount);
static uint8_t subghz_decode_chamberlain(uint16_t p, uint16_t pulsecount);
static bool subghz_match_duration(uint16_t duration, uint16_t target, uint8_t tolerance);
static void subghz_update_details(uint16_t p, uint16_t bits, uint64_t hi, uint64_t lo);

static const SubGHz_PairRule_t subghz_pair_rules[] = {
    [PRINCETON] = {0, 0, true, 0, 0},
    [SECURITY_PLUS_20] = {0, 0, true, 0, 0},
    [SECURITY_PLUS_10] = {0, 0, true, 0, 0},
    [CAME] = {12, 42, true, 0, 0},
    [NICE_FLO] = {12, 24, true, 0, 0},
    [LINEAR] = {10, 10, true, 0, 0},
    [HOLTEK] = {40, 40, true, 0xF000000000ULL, 0x5000000000ULL},
    [HORMANN] = {44, 44, true, 0, 0},
    [DOITRAND] = {37, 37, true, 0, 0},
    [DOOYA] = {40, 40, true, 0, 0},
    [CAME_ATOMO] = {62, 62, true, 0, 0},
    [CAME_TWEE] = {54, 54, true, 0, 0},
    [ANSONIC] = {12, 12, false, 0, 0},
    [BETT] = {18, 18, true, 0, 0},
    [CLEMSA] = {18, 18, true, 0, 0},
    [GATE_TX] = {24, 24, true, 0, 0},
    [MARANTEC] = {49, 49, true, 0, 0},
    [MASTERCODE] = {36, 36, true, 0, 0},
    [SMC5326] = {25, 25, true, 0, 0},
    [HOLTEK_HT12X] = {12, 12, false, 0, 0},
    [NERO_RADIO] = {56, 56, true, 0, 0},
    [NICE_FLOR_S] = {52, 72, true, 0, 0},
    [ALUTECH_AT_4N] = {72, 72, false, 0, 0},
    [KINGGATES_STYLO_4K] = {89, 89, false, 0, 0},
    [HONEYWELL_WDB] = {47, 48, true, 0, 0},
    [KEELOQ] = {64, 64, true, 0, 0},
    [STAR_LINE] = {64, 64, false, 0, 0},
    [FAAC_SLH] = {64, 64, true, 0, 0},
    [PHOENIX_V2] = {52, 52, true, 0, 0},
    [MAGELLAN] = {32, 32, true, 0, 0},
    [NERO_SKETCH] = {40, 40, true, 0, 0},
};

static const SubGHz_TimedPairRule_t subghz_timed_pair_rules[] = {
    [LINEAR_DELTA3] = {2000, 2000, 500, 3500, 8, true},
    [FERON] = {350, 750, 750, 350, 32, true},
    [GANGQI] = {500, 1200, 1200, 500, 34, true},
    [HAY21] = {300, 700, 700, 300, 21, true},
    [MARANTEC24] = {1600, 2400, 800, 3200, 24, true},
    [ROGER] = {500, 1000, 1000, 500, 28, true},
    [HOLLARM] = {200, 1000, 200, 1600, 42, true},
    [LEGRAND] = {1125, 375, 375, 1125, 18, false},
    [DICKERT_MAHS] = {400, 800, 800, 400, 36, false},
    [KIA] = {250, 250, 500, 500, 61, false},
    [SCHER_KHAN] = {750, 750, 1100, 1100, 35, false},
    [IDO] = {450, 1450, 450, 450, 48, false},
    [INTERTECHNO_V3] = {275, 275, 1375, 275, 32, false},
};

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

inline uint16_t get_diff(uint16_t n_a, uint16_t n_b);
uint8_t subghz_pulse_handler(uint16_t duration);
static bool subghz_decode_protocol(uint16_t p, uint16_t pulsecount);
bool subghz_decenc_read(SubGHz_Dec_Info_t *received, bool raw);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
inline uint16_t get_diff(uint16_t n_a, uint16_t n_b)
{
	return abs(n_a - n_b);
}



/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
bool subghz_data_ready()
{
  return ((subghz_decenc_ctl.n64_decodedvalue != 0) ||
          (subghz_decenc_ctl.n64_decodedvalue_hi != 0));
}


/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
bool subghz_raw_data_ready()
{
  return (subghz_decenc_ctl.pulse_times[0] != 0);
}


/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
void subghz_reset_data()
{
	subghz_decenc_ctl.n64_decodedvalue = 0;
	subghz_decenc_ctl.n64_decodedvalue_hi = 0;
	subghz_decenc_ctl.ndecodedbitlength = 0;
	memset(subghz_decenc_ctl.detail_line1, 0, sizeof(subghz_decenc_ctl.detail_line1));
	memset(subghz_decenc_ctl.detail_line2, 0, sizeof(subghz_decenc_ctl.detail_line2));
	memset(subghz_decenc_ctl.detail_line3, 0, sizeof(subghz_decenc_ctl.detail_line3));
	memset(subghz_decenc_ctl.pulse_times, 0, sizeof(subghz_decenc_ctl.pulse_times));
}


/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
uint64_t subghz_get_decoded_value()
{
	return subghz_decenc_ctl.n64_decodedvalue;
}


/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
uint64_t subghz_get_decoded_value_hi()
{
	return subghz_decenc_ctl.n64_decodedvalue_hi;
}


/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
uint16_t subghz_get_decoded_bitlength()
{
	return subghz_decenc_ctl.ndecodedbitlength;
}


/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
uint16_t subghz_get_decoded_delay()
{
	return subghz_decenc_ctl.ndecodeddelay;
}


/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
uint16_t subghz_get_decoded_protocol()
{
	return subghz_decenc_ctl.ndecodedprotocol;
}


/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
int16_t subghz_get_decoded_rssi()
{
	return subghz_decenc_ctl.ndecodedrssi;
}


/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
uint16_t *subghz_get_rawdata()
{
	return subghz_decenc_ctl.pulse_times;
}

static bool subghz_match_duration(uint16_t duration, uint16_t target, uint8_t tolerance)
{
    uint32_t delta = ((uint32_t)target * tolerance) / 100U;

    if (delta < 80U)
    {
        delta = 80U;
    }

    return get_diff(duration, target) <= delta;
}

static bool subghz_bit_count_allowed(const SubGHz_PairRule_t *rule, uint8_t bits)
{
    return (bits >= rule->min_bits) && (bits <= rule->max_bits);
}

static void subghz_push_decoded_bit(uint64_t *hi, uint64_t *lo, bool bit)
{
    *hi = (*hi << 1) | (*lo >> 63);
    *lo = (*lo << 1) | (bit ? 1ULL : 0ULL);
}

static void subghz_store_decoded(uint16_t p, uint16_t delay, uint16_t bits, uint64_t hi, uint64_t lo)
{
    subghz_decenc_ctl.n64_decodedvalue_hi = hi;
    subghz_decenc_ctl.n64_decodedvalue = lo;
    subghz_decenc_ctl.ndecodedbitlength = bits;
    subghz_decenc_ctl.ndecodeddelay = delay;
    subghz_decenc_ctl.ndecodedprotocol = p;
    subghz_update_details(p, bits, hi, lo);
}

static bool subghz_scan_inside_frame(uint16_t p)
{
    switch (p)
    {
        case KEELOQ:
        case STAR_LINE:
        case FAAC_SLH:
        case PHOENIX_V2:
        case MAGELLAN:
        case LEGRAND:
        case DICKERT_MAHS:
        case KIA:
        case SCHER_KHAN:
        case IDO:
        case NERO_SKETCH:
        case INTERTECHNO_V3:
            return true;

        default:
            return false;
    }
}

static uint64_t subghz_reverse_key64(uint64_t data, uint16_t bits)
{
    uint64_t out = 0;

    if (bits > 64U)
    {
        bits = 64U;
    }

    for (uint16_t i = 0; i < bits; i++)
    {
        out <<= 1;
        out |= (data >> i) & 1ULL;
    }

    return out;
}

static const char *subghz_magellan_event_text(uint8_t event)
{
    switch (event)
    {
        case 0x0B:
            return "Disarm";
        case 0x0C:
            return "Arm";
        case 0x0D:
            return "Arm Stay";
        case 0x0E:
            return "Panic";
        default:
            return "Event";
    }
}

static const char *subghz_scher_khan_variant(uint16_t bits)
{
    if (bits == 35U)
    {
        return "Scher-Khan";
    }
    if (bits == 36U)
    {
        return "MagicCode";
    }
    return "Unknown";
}

static uint16_t subghz_phoenix_v2_decrypt_counter(uint64_t full_key)
{
    uint16_t encrypted_value = (uint16_t)((full_key >> 40) & 0xFFFFU);
    uint8_t byte1 = (uint8_t)(encrypted_value >> 8);
    uint8_t byte2 = (uint8_t)(encrypted_value & 0xFFU);
    uint8_t xor_key1 = (uint8_t)(full_key >> 24);
    uint8_t xor_key2 = (uint8_t)((full_key >> 16) & 0xFFU);

    for (uint8_t i = 0; i < 16U; i++)
    {
        uint8_t msb_of_byte1 = byte1 & 0x80U;
        uint8_t lsb_of_byte2 = byte2 & 1U;

        byte2 = (uint8_t)((byte2 >> 1) | msb_of_byte1);
        byte1 = (uint8_t)((byte1 << 1) | lsb_of_byte2);

        if (msb_of_byte1 == 0U)
        {
            byte1 ^= xor_key1;
            byte2 = (uint8_t)((byte2 ^ xor_key2) & 0x7FU);
        }
    }

    return (uint16_t)(((uint16_t)byte2 << 8) | byte1);
}

static void subghz_detail_key_parts(uint16_t bits, uint64_t lo, uint32_t *fix, uint32_t *hop)
{
    uint64_t reversed = subghz_reverse_key64(lo, bits);

    *fix = (uint32_t)(reversed >> 32);
    *hop = (uint32_t)reversed;
}

static void subghz_update_details(uint16_t p, uint16_t bits, uint64_t hi, uint64_t lo)
{
    uint32_t fix;
    uint32_t hop;
    uint32_t serial;
    uint32_t cnt;
    uint8_t btn;

    (void)hi;
    memset(subghz_decenc_ctl.detail_line1, 0, sizeof(subghz_decenc_ctl.detail_line1));
    memset(subghz_decenc_ctl.detail_line2, 0, sizeof(subghz_decenc_ctl.detail_line2));
    memset(subghz_decenc_ctl.detail_line3, 0, sizeof(subghz_decenc_ctl.detail_line3));

    switch (p)
    {
        case MEGACODE:
            serial = (uint32_t)((lo >> 3) & 0xFFFFU);
            btn = (uint8_t)(lo & 0x07U);
            cnt = (uint32_t)((lo >> 19) & 0x0FU);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%04lX Btn:%u", (unsigned long)serial, btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Facility:%lX", (unsigned long)cnt);
            break;

        case FERON:
            serial = (uint32_t)(lo >> 16);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%04lX", (unsigned long)serial);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Cmd:%04lX", (unsigned long)(lo & 0xFFFFU));
            break;

        case MARANTEC24:
            serial = (uint32_t)(lo >> 4);
            btn = (uint8_t)(lo & 0x0FU);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%05lX Btn:%X", (unsigned long)serial, btn);
            break;

        case ROGER:
            serial = (uint32_t)(lo >> 12);
            btn = (uint8_t)((lo >> 8) & 0x0FU);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%04lX Btn:%X", (unsigned long)serial, btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "End:%02lX", (unsigned long)(lo & 0xFFU));
            break;

        case HOLLARM:
            btn = (uint8_t)((lo >> 8) & 0x0FU);
            serial = (uint32_t)((lo >> 16) & 0x0FFFFFFFU);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%07lX Btn:%X", (unsigned long)serial, btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Sum:%02lX", (unsigned long)(lo & 0xFFU));
            break;

        case HAY21:
            btn = (uint8_t)((lo >> 13) & 0xFFU);
            serial = (uint32_t)((lo >> 5) & 0xFFU);
            cnt = (uint32_t)((lo >> 1) & 0x0FU);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%02lX Btn:%02X", (unsigned long)serial, btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Cnt:%lX", (unsigned long)cnt);
            break;

        case KEELOQ:
            subghz_detail_key_parts(bits, lo, &fix, &hop);
            serial = fix & 0x0FFFFFFFU;
            btn = (uint8_t)(fix >> 28);
            cnt = (uint32_t)(hop >> 16);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%07lX Btn:%X", (unsigned long)serial, btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Hop:%08lX", (unsigned long)hop);
            if (((hop >> 24) == ((hop >> 16) & 0xFFU)) &&
                ((fix >> 28) == ((hop >> 12) & 0x0FU)) &&
                ((hop & 0xFFFU) == 0x404U))
            {
                snprintf(subghz_decenc_ctl.detail_line3, sizeof(subghz_decenc_ctl.detail_line3),
                         "MF:AN-Motors Cnt:%04lX", (unsigned long)cnt);
            }
            else if (((hop & 0xFFFU) == 0U) && ((fix >> 28) == ((hop >> 12) & 0x0FU)))
            {
                snprintf(subghz_decenc_ctl.detail_line3, sizeof(subghz_decenc_ctl.detail_line3),
                         "MF:HCS101 Cnt:%04lX", (unsigned long)cnt);
            }
            else
            {
                snprintf(subghz_decenc_ctl.detail_line3, sizeof(subghz_decenc_ctl.detail_line3),
                         "MF:Key required");
            }
            break;

        case STAR_LINE:
            subghz_detail_key_parts(bits, lo, &fix, &hop);
            serial = fix & 0x00FFFFFFU;
            btn = (uint8_t)(fix >> 24);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%06lX Btn:%02X", (unsigned long)serial, btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Hop:%08lX", (unsigned long)hop);
            snprintf(subghz_decenc_ctl.detail_line3, sizeof(subghz_decenc_ctl.detail_line3),
                     "MF:Key required");
            break;

        case FAAC_SLH:
            subghz_detail_key_parts(bits, lo, &fix, &hop);
            serial = fix & 0x0FFFFFFFU;
            btn = (uint8_t)((fix >> 28) & 0x0FU);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%07lX Btn:%X", (unsigned long)serial, btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Fix:%08lX", (unsigned long)fix);
            snprintf(subghz_decenc_ctl.detail_line3, sizeof(subghz_decenc_ctl.detail_line3),
                     "Hop:%08lX", (unsigned long)hop);
            break;

        case PHOENIX_V2:
        {
            uint64_t data_rev = subghz_reverse_key64(lo, bits + 4U);
            serial = (uint32_t)(data_rev & 0xFFFFFFFFU);
            btn = (uint8_t)((data_rev >> 32) & 0x0FU);
            cnt = subghz_phoenix_v2_decrypt_counter(data_rev);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%08lX Btn:%X", (unsigned long)serial, btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Cnt:%04lX", (unsigned long)cnt);
            snprintf(subghz_decenc_ctl.detail_line3, sizeof(subghz_decenc_ctl.detail_line3),
                     "Enc:%04lX", (unsigned long)((data_rev >> 40) & 0xFFFFU));
            break;
        }

        case IDO:
            subghz_detail_key_parts(bits, lo, &fix, &hop);
            fix &= 0x00FFFFFFU;
            hop &= 0x00FFFFFFU;
            serial = fix & 0x000FFFFFU;
            btn = (uint8_t)((fix >> 20) & 0x0FU);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%05lX Btn:%X", (unsigned long)serial, btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Fix:%06lX Hop:%06lX", (unsigned long)fix, (unsigned long)hop);
            break;

        case KIA:
            serial = (uint32_t)((lo >> 12) & 0x0FFFFFFFU);
            btn = (uint8_t)((lo >> 8) & 0x0FU);
            cnt = (uint32_t)((lo >> 40) & 0xFFFFU);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%07lX Btn:%X", (unsigned long)serial, btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Cnt:%04lX", (unsigned long)cnt);
            break;

        case SCHER_KHAN:
            serial = (uint32_t)(((lo >> 24) & 0x0FFFFFF0U) | ((lo >> 20) & 0x0FU));
            btn = (uint8_t)((lo >> 24) & 0x0FU);
            cnt = (uint32_t)(lo & 0xFFFFU);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%07lX Btn:%X", (unsigned long)serial, btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Cnt:%04lX %s", (unsigned long)cnt, subghz_scher_khan_variant(bits));
            break;

        case MAGELLAN:
        {
            uint64_t data_rev = subghz_reverse_key64(lo >> 8, 24U);
            serial = (uint32_t)(data_rev & 0xFFFFU);
            btn = (uint8_t)((data_rev >> 16) & 0xFFU);
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Sn:%03lu%03lu Ev:%02X",
                     (unsigned long)((serial >> 8) & 0xFFU),
                     (unsigned long)(serial & 0xFFU),
                     btn);
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "%s", subghz_magellan_event_text(btn));
            break;
        }

        case INTERTECHNO_V3:
            if (bits == 32U)
            {
                serial = (uint32_t)((lo >> 6) & 0x03FFFFFFU);
                cnt = ((lo >> 5) & 1U) ? (1U << 5) : (~lo & 0x0FU);
                btn = (uint8_t)((lo >> 4) & 1U);
                snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                         "Sn:%07lX", (unsigned long)serial);
                snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                         "Ch:%s Btn:%s", (cnt >> 5) ? "All" : "Sel", btn ? "On" : "Off");
            }
            break;

        case NERO_SKETCH:
            snprintf(subghz_decenc_ctl.detail_line1, sizeof(subghz_decenc_ctl.detail_line1),
                     "Key:%08lX", (unsigned long)(lo & 0xFFFFFFFFU));
            snprintf(subghz_decenc_ctl.detail_line2, sizeof(subghz_decenc_ctl.detail_line2),
                     "Rev:%08lX", (unsigned long)subghz_reverse_key64(lo, bits));
            break;

        default:
            break;
    }
}

static uint16_t subghz_data_pulse_count(uint16_t pulsecount)
{
    while (pulsecount && (subghz_decenc_ctl.pulse_times[pulsecount - 1] >= INTERPACKET_GAP_MIN))
    {
        pulsecount--;
    }

    return pulsecount;
}

static uint8_t subghz_decode_secplus_v1(uint16_t p, uint16_t pulsecount)
{
    const SubGHz_protocol_t *proto;
    uint16_t data_count;
    uint16_t offset;
    uint16_t te_middle;

    if (p >= n_protocol)
    {
        return 1;
    }

    proto = &subghz_protocols_list[p];
    te_middle = proto->te_short * 2U;
    data_count = pulsecount;

    while (data_count && (subghz_decenc_ctl.pulse_times[data_count - 1] >= INTERPACKET_GAP_MIN))
    {
        data_count--;
    }

    for (offset = 0; offset < 2; offset++)
    {
        uint64_t code = 0;
        uint8_t trits = 0;
        uint16_t i;

        for (i = offset; (i + 1) < data_count; i += 2)
        {
            uint16_t first = subghz_decenc_ctl.pulse_times[i];
            uint16_t second = subghz_decenc_ctl.pulse_times[i + 1];
            uint8_t trit;

            if (subghz_match_duration(first, proto->te_long, proto->te_tolerance) &&
                subghz_match_duration(second, proto->te_short, proto->te_tolerance))
            {
                trit = 0;
            }
            else if (subghz_match_duration(first, te_middle, proto->te_tolerance) &&
                     subghz_match_duration(second, te_middle, proto->te_tolerance))
            {
                trit = 1;
            }
            else if (subghz_match_duration(first, proto->te_short, proto->te_tolerance) &&
                     subghz_match_duration(second, proto->te_long, proto->te_tolerance))
            {
                trit = 2;
            }
            else
            {
                break;
            }

            code = (code * 3ULL) + trit;
            trits++;

            if (trits >= proto->data_bits)
            {
                break;
            }
        }

        if (trits == proto->data_bits)
        {
            subghz_store_decoded(p, proto->te_short, trits, 0, code);
            return 0;
        }
    }

    return 1;
}

static uint8_t subghz_decode_pair_rule(uint16_t p, uint16_t pulsecount)
{
    const SubGHz_PairRule_t *rule;
    const SubGHz_protocol_t *proto;
    uint16_t data_count;
    uint16_t offset;

    if (p >= n_protocol)
    {
        return 1;
    }

    rule = &subghz_pair_rules[p];
    proto = &subghz_protocols_list[p];
    data_count = subghz_data_pulse_count(pulsecount);
    uint16_t max_offset = subghz_scan_inside_frame(p) ? data_count : 2U;

    if (data_count < 2)
    {
        return 1;
    }

    for (offset = 0; offset < max_offset; offset++)
    {
        uint64_t hi = 0;
        uint64_t lo = 0;
        uint8_t bits = 0;
        uint16_t i;

        for (i = offset; (i + 1) < data_count; i += 2)
        {
            uint16_t first = subghz_decenc_ctl.pulse_times[i];
            uint16_t second = subghz_decenc_ctl.pulse_times[i + 1];
            bool short_long =
                subghz_match_duration(first, proto->te_short, proto->te_tolerance) &&
                subghz_match_duration(second, proto->te_long, proto->te_tolerance);
            bool long_short =
                subghz_match_duration(first, proto->te_long, proto->te_tolerance) &&
                subghz_match_duration(second, proto->te_short, proto->te_tolerance);

            if (!short_long && !long_short)
            {
                break;
            }

            if (rule->one_is_long_short ? long_short : short_long)
            {
                subghz_push_decoded_bit(&hi, &lo, true);
            }
            else
            {
                subghz_push_decoded_bit(&hi, &lo, false);
            }
            bits++;

            if (bits >= rule->max_bits)
            {
                break;
            }
        }

        if (subghz_bit_count_allowed(rule, bits))
        {
            if (rule->data_mask && ((lo & rule->data_mask) != rule->data_match))
            {
                continue;
            }

            subghz_store_decoded(p, proto->te_short, bits, hi, lo);
            return 0;
        }
    }

    return 1;
}

static uint8_t subghz_decode_timed_pair_rule(uint16_t p, uint16_t pulsecount)
{
    const SubGHz_TimedPairRule_t *rule;
    const SubGHz_protocol_t *proto;
    uint16_t data_count;
    uint16_t offset;

    if (p >= n_protocol)
    {
        return 1;
    }

    rule = &subghz_timed_pair_rules[p];
    proto = &subghz_protocols_list[p];
    data_count = subghz_data_pulse_count(pulsecount);
    uint16_t max_offset = subghz_scan_inside_frame(p) ? data_count : 2U;

    if (data_count < 2 || rule->bits == 0)
    {
        return 1;
    }

    for (offset = 0; offset < max_offset; offset++)
    {
        uint64_t hi = 0;
        uint64_t lo = 0;
        uint8_t bits = 0;
        uint16_t i;

        for (i = offset; i < data_count; i += 2)
        {
            uint16_t high = subghz_decenc_ctl.pulse_times[i];
            uint16_t low = ((i + 1) < data_count) ? subghz_decenc_ctl.pulse_times[i + 1] : 0;
            bool zero_match = false;
            bool one_match = false;

            if ((i + 1) < data_count)
            {
                zero_match = subghz_match_duration(high, rule->zero_high, proto->te_tolerance) &&
                             subghz_match_duration(low, rule->zero_low, proto->te_tolerance);
                one_match = subghz_match_duration(high, rule->one_high, proto->te_tolerance) &&
                            subghz_match_duration(low, rule->one_low, proto->te_tolerance);
            }
            else if (rule->allow_last_high_only && ((bits + 1U) == rule->bits))
            {
                zero_match = subghz_match_duration(high, rule->zero_high, proto->te_tolerance);
                one_match = subghz_match_duration(high, rule->one_high, proto->te_tolerance);
            }

            if (!zero_match && !one_match)
            {
                break;
            }

            subghz_push_decoded_bit(&hi, &lo, one_match);
            bits++;

            if (bits >= rule->bits)
            {
                break;
            }
        }

        if (bits == rule->bits)
        {
            subghz_store_decoded(p, proto->te_short, bits, hi, lo);
            return 0;
        }
    }

    return 1;
}

static uint8_t subghz_decode_megacode(uint16_t p, uint16_t pulsecount)
{
    const SubGHz_protocol_t *proto;
    uint16_t data_count;
    uint16_t offset;

    if (p >= n_protocol)
    {
        return 1;
    }

    proto = &subghz_protocols_list[p];
    data_count = subghz_data_pulse_count(pulsecount);

    for (offset = 0; offset < data_count; offset++)
    {
        uint64_t hi = 0;
        uint64_t lo = 0;
        uint8_t bits = 0;
        bool last_bit = true;
        uint16_t i;

        if (!subghz_match_duration(subghz_decenc_ctl.pulse_times[offset], proto->te_short, proto->te_tolerance))
        {
            continue;
        }

        subghz_push_decoded_bit(&hi, &lo, true);
        bits = 1;

        for (i = offset + 1; (i + 1) < data_count; i += 2)
        {
            uint16_t low = subghz_decenc_ctl.pulse_times[i];
            uint16_t high = subghz_decenc_ctl.pulse_times[i + 1];
            uint16_t adjusted_low = low;
            bool bit;

            if (!subghz_match_duration(high, proto->te_short, proto->te_tolerance))
            {
                break;
            }

            if (!last_bit)
            {
                if (adjusted_low < (proto->te_short * 3U))
                {
                    break;
                }
                adjusted_low -= proto->te_short * 3U;
            }

            if (subghz_match_duration(adjusted_low, proto->te_short * 5U, proto->te_tolerance))
            {
                bit = true;
            }
            else if (subghz_match_duration(adjusted_low, proto->te_short * 2U, proto->te_tolerance))
            {
                bit = false;
            }
            else
            {
                break;
            }

            subghz_push_decoded_bit(&hi, &lo, bit);
            last_bit = bit;
            bits++;

            if (bits >= proto->data_bits)
            {
                break;
            }
        }

        if (bits == proto->data_bits)
        {
            subghz_store_decoded(p, proto->te_short, bits, hi, lo);
            return 0;
        }
    }

    return 1;
}

static bool subghz_chamberlain_nibbles_to_bits(uint64_t *code, uint8_t *bits)
{
    uint64_t data = *code;
    uint64_t out = 0;
    uint8_t i;

    if ((data & 0xF000000FF0FULL) == 0x10000001101ULL)
    {
        *bits = 7;
        data &= ~0xF000000FF0FULL;
        data = (data >> 12) | ((data >> 4) & 0xFULL);
    }
    else if ((data & 0xF00000F00FULL) == 0x1000001001ULL)
    {
        *bits = 8;
        data &= ~0xF00000F00FULL;
        data = (data >> 4) | (0x7ULL << 8);
    }
    else if ((data & 0xF000000000FULL) == 0x10000000001ULL)
    {
        *bits = 9;
        data &= ~0xF000000000FULL;
        data >>= 4;
    }
    else
    {
        return false;
    }

    for (i = 0; i < *bits; i++)
    {
        uint8_t nibble = data & 0xFU;

        if (nibble == 0x7U)
        {
            out |= 0ULL << i;
        }
        else if (nibble == 0x3U)
        {
            out |= 1ULL << i;
        }
        else
        {
            return false;
        }

        data >>= 4;
    }

    *code = out;
    return true;
}

static uint8_t subghz_decode_chamberlain(uint16_t p, uint16_t pulsecount)
{
    const SubGHz_protocol_t *proto;
    uint16_t data_count;
    uint8_t offset;

    if (p >= n_protocol)
    {
        return 1;
    }

    proto = &subghz_protocols_list[p];
    data_count = subghz_data_pulse_count(pulsecount);

    for (offset = 0; offset < 2; offset++)
    {
        uint64_t code = 0x1ULL;
        uint8_t nibbles = 1;
        uint16_t i;

        if (!subghz_match_duration(subghz_decenc_ctl.pulse_times[offset], proto->te_short, proto->te_tolerance))
        {
            continue;
        }

        for (i = offset + 1; (i + 1) < data_count; i += 2)
        {
            uint16_t low = subghz_decenc_ctl.pulse_times[i];
            uint16_t high = subghz_decenc_ctl.pulse_times[i + 1];
            uint8_t nibble;

            if (subghz_match_duration(low, proto->te_long, proto->te_tolerance) &&
                subghz_match_duration(high, proto->te_short, proto->te_tolerance))
            {
                nibble = 0x1U;
            }
            else if (subghz_match_duration(low, proto->te_short * 2U, proto->te_tolerance) &&
                     subghz_match_duration(high, proto->te_short * 2U, proto->te_tolerance))
            {
                nibble = 0x3U;
            }
            else if (subghz_match_duration(low, proto->te_short, proto->te_tolerance) &&
                     subghz_match_duration(high, proto->te_long, proto->te_tolerance))
            {
                nibble = 0x7U;
            }
            else
            {
                break;
            }

            code = (code << 4) | nibble;
            nibbles++;

            if (nibbles >= 11U)
            {
                break;
            }
        }

        if (nibbles >= 10U)
        {
            uint8_t bits = 0;
            uint64_t out = code;

            if (subghz_chamberlain_nibbles_to_bits(&out, &bits))
            {
                subghz_store_decoded(p, proto->te_short, bits, 0, out);
                return 0;
            }
        }
    }

    return 1;
}



/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
bool subghz_decode_protocol(uint16_t p, uint16_t pulsecount)
{
    uint8_t ret = false;

    switch ( p )
    {
    	case PRINCETON:
    		ret = subghz_decode_princeton(p, pulsecount);
    		break;

    	case SECURITY_PLUS_20:
    		ret = subghz_decode_security_plus_20(p, pulsecount);
    		break;

        case SECURITY_PLUS_10:
            ret = subghz_decode_secplus_v1(p, pulsecount);
            break;

        case CAME:
        case NICE_FLO:
        case LINEAR:
        case HOLTEK:
        case HORMANN:
        case DOITRAND:
        case DOOYA:
        case CAME_ATOMO:
        case CAME_TWEE:
        case ANSONIC:
        case BETT:
        case CLEMSA:
        case GATE_TX:
        case MARANTEC:
        case MASTERCODE:
        case SMC5326:
        case HOLTEK_HT12X:
        case NERO_RADIO:
        case NICE_FLOR_S:
        case ALUTECH_AT_4N:
        case KINGGATES_STYLO_4K:
        case HONEYWELL_WDB:
        case KEELOQ:
        case STAR_LINE:
        case FAAC_SLH:
        case PHOENIX_V2:
        case MAGELLAN:
        case NERO_SKETCH:
            ret = subghz_decode_pair_rule(p, pulsecount);
            break;

        case MEGACODE:
            ret = subghz_decode_megacode(p, pulsecount);
            break;

        case CHAMBERLAIN_CODE:
            ret = subghz_decode_chamberlain(p, pulsecount);
            break;

        case LINEAR_DELTA3:
        case FERON:
        case GANGQI:
        case HAY21:
        case MARANTEC24:
        case ROGER:
        case HOLLARM:
        case LEGRAND:
        case DICKERT_MAHS:
        case KIA:
        case SCHER_KHAN:
        case IDO:
        case INTERTECHNO_V3:
            ret = subghz_decode_timed_pair_rule(p, pulsecount);
            break;

    	default:
    		break;
    } // switch ( p )

    return ret;
} // bool subghz_decode_protocol(uint16_t p, uint16_t subghz_decenc_ctl.npulsecount)


/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
uint8_t subghz_pulse_handler(uint16_t duration)
{
	  static uint32_t interpacket_gap = 0;
	  uint8_t i;
	  int16_t rssi;
	  struct si446x_reply_GET_MODEM_STATUS_map *pmodemstat;

	  if (duration >= PACKET_PULSE_TIME_MIN)
	  {
		  if (duration >= INTERPACKET_GAP_MIN) // Possible gap between packets?
		  {
			  subghz_decenc_ctl.pulse_times[subghz_decenc_ctl.npulsecount++] = duration; // End bit

			  M1_LOG_D(M1_LOGDB_TAG, "Valid gap: %d, pulses:%d\r\n", duration, subghz_decenc_ctl.npulsecount);
			  if ( subghz_decenc_ctl.npulsecount >= PACKET_PULSE_COUNT_MIN ) // Potential packet received?
			  {
				  for(i = 0; i < n_protocol; i++)
				  {
					  if ( !subghz_decode_protocol(i, subghz_decenc_ctl.npulsecount) )
					  {
						  // receive successfully for protocol i
						  break;
					  }
				  } // for(i = 0; i < n_protocol; i++)
			  } // if ( subghz_decenc_ctl.npulsecount >= PACKET_PULSE_COUNT_MIN )
			  interpacket_gap = duration; // update
			  subghz_decenc_ctl.npulsecount = 0;
			  // A potential interpacket gap has been detected, so it's not required to check for this condition for the next packet, if any.
			  return PULSE_DET_EOP; // error or end of packet has been met
		  } // if (duration >= INTERPACKET_GAP_MIN)
	  } // if (duration >= PACKET_PULSE_TIME_MIN)
	  else
	  {
		  subghz_decenc_ctl.npulsecount = 0; // reset
		  interpacket_gap += duration;
		  // Interpacket gap has been timeout for a potential packet
		  if ( interpacket_gap > INTERPACKET_GAP_MAX )
		  {
			  interpacket_gap = 0; // reset
			  return PULSE_DET_IDLE; // error
		  }
		  else
		  {
			  return PULSE_DET_NORMAL;
		  }
	  } // else
	  // detect overflow
	  if (subghz_decenc_ctl.npulsecount >= PACKET_PULSE_COUNT_MAX)
	  {
		  subghz_decenc_ctl.npulsecount = 0; // Reset rx buffer
		  return PULSE_DET_IDLE; // error
	  }
	  subghz_decenc_ctl.pulse_times[subghz_decenc_ctl.npulsecount++] = duration;
	  // Read RSSI when half of this potential packet has been received
	  if ( subghz_decenc_ctl.npulsecount==PACKET_PULSE_COUNT_MIN/2 )
	  {
		  // Read INTs, clear pending ones
		  SI446x_Get_IntStatus(0, 0, 0);
		  pmodemstat = SI446x_Get_ModemStatus(0x00);
		  // RF_Input_Level_dBm = (RSSI_value / 2) – MODEM_RSSI_COMP – 70
		  rssi = pmodemstat->CURR_RSSI/2 - MODEM_RSSI_COMP - 70;
		  subghz_decenc_ctl.ndecodedrssi = rssi;
	  } // if ( subghz_decenc_ctl.npulsecount==PACKET_PULSE_COUNT_MIN/2 )

	  return PULSE_DET_NORMAL;
} // uint8_t subghz_pulse_handler(uint16_t duration)



/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
bool subghz_decenc_read(SubGHz_Dec_Info_t *received, bool raw)
{
    bool ret = false;

	if ( subghz_decenc_ctl.subghz_data_ready() )
    {
        uint64_t value = subghz_decenc_ctl.subghz_get_decoded_value();
        uint64_t value_hi = subghz_decenc_ctl.subghz_get_decoded_value_hi();
        if (value || value_hi)
        {
            received->frequency = 0;
            received->key = value;
            received->key_hi = value_hi;
            received->protocol = subghz_decenc_ctl.subghz_get_decoded_protocol();
            received->rssi = subghz_decenc_ctl.subghz_get_decoded_rssi();
            received->te = subghz_decenc_ctl.subghz_get_decoded_delay();
            received->bit_len = subghz_decenc_ctl.subghz_get_decoded_bitlength();
            received->detail_line1[0] = '\0';
            received->detail_line2[0] = '\0';
            received->detail_line3[0] = '\0';
            strncpy(received->detail_line1, subghz_decenc_ctl.detail_line1, sizeof(received->detail_line1) - 1U);
            strncpy(received->detail_line2, subghz_decenc_ctl.detail_line2, sizeof(received->detail_line2) - 1U);
            strncpy(received->detail_line3, subghz_decenc_ctl.detail_line3, sizeof(received->detail_line3) - 1U);
            received->detail_line1[sizeof(received->detail_line1) - 1U] = '\0';
            received->detail_line2[sizeof(received->detail_line2) - 1U] = '\0';
            received->detail_line3[sizeof(received->detail_line3) - 1U] = '\0';
        } // if (value)
        subghz_decenc_ctl.subghz_reset_data();
        ret = true;
    } // if ( subghz_decenc_ctl.subghz_data_ready() )

    if (raw && subghz_decenc_ctl.subghz_raw_data_ready())
    {
    	received->raw = true;
    	received->raw_data = subghz_decenc_ctl.subghz_get_rawdata();
        subghz_decenc_ctl.subghz_reset_data();
        ret = true;
    } // if (raw && subghz_decenc_ctl.subghz_raw_data_ready())

    return ret;
} // bool subghz_decenc_read(bool raw)


/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
void subghz_decenc_init(void)
{
    subghz_decenc_ctl.subghz_data_ready = subghz_data_ready;
    subghz_decenc_ctl.subghz_raw_data_ready = subghz_raw_data_ready;
    subghz_decenc_ctl.subghz_reset_data = subghz_reset_data;

    subghz_decenc_ctl.subghz_get_decoded_value = subghz_get_decoded_value;
    subghz_decenc_ctl.subghz_get_decoded_value_hi = subghz_get_decoded_value_hi;
    subghz_decenc_ctl.subghz_get_decoded_bitlength = subghz_get_decoded_bitlength;
    subghz_decenc_ctl.subghz_get_decoded_delay = subghz_get_decoded_delay;
    subghz_decenc_ctl.subghz_get_decoded_protocol = subghz_get_decoded_protocol;
    subghz_decenc_ctl.subghz_get_decoded_rssi = subghz_get_decoded_rssi;
    subghz_decenc_ctl.subghz_get_rawdata = subghz_get_rawdata;
    subghz_decenc_ctl.subghz_pulse_handler = subghz_pulse_handler;

	subghz_decenc_ctl.n64_decodedvalue = 0;
	subghz_decenc_ctl.ndecodedbitlength = 0;
	subghz_decenc_ctl.ndecodedrssi = 0;
	subghz_decenc_ctl.ndecodeddelay = 0;
	subghz_decenc_ctl.ndecodedprotocol = 0;
	subghz_decenc_ctl.npulsecount = 0;
	subghz_decenc_ctl.pulse_det_stat = PULSE_DET_IDLE;
	memset(subghz_decenc_ctl.pulse_times, 0, sizeof(subghz_decenc_ctl.pulse_times));
	memset(subghz_decenc_ctl.detail_line1, 0, sizeof(subghz_decenc_ctl.detail_line1));
	memset(subghz_decenc_ctl.detail_line2, 0, sizeof(subghz_decenc_ctl.detail_line2));
	memset(subghz_decenc_ctl.detail_line3, 0, sizeof(subghz_decenc_ctl.detail_line3));
	subghz_decenc_ctl.n64_decodedvalue = 0;
	subghz_decenc_ctl.n64_decodedvalue_hi = 0;
} // void subghz_decenc_init(void)
