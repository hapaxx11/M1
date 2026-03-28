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
	{370, 1140, PACKET_PULSE_TIME_TOLERANCE20, 0, 24},  // Princeton: bit 0 |^|___, bit 1 |^^^|_
	{250, 500, PACKET_PULSE_TIME_TOLERANCE20, 16, 46},  // Security+ 2.0
	{320, 640, PACKET_PULSE_TIME_TOLERANCE20, 0, 12},   // CAME 12-bit
	{700, 1400, PACKET_PULSE_TIME_TOLERANCE20, 0, 12},  // Nice FLO 12-bit
	{500, 1500, PACKET_PULSE_TIME_TOLERANCE25, 0, 10},  // Linear 10-bit
	{340, 1020, PACKET_PULSE_TIME_TOLERANCE20, 0, 12},  // Holtek HT12E
	{400, 800, PACKET_PULSE_TIME_TOLERANCE20, 0, 66},   // KeeLoq
	{488, 976, PACKET_PULSE_TIME_TOLERANCE25, 24, 64},  // Oregon Scientific v2.1 (Manchester)
	{200, 600, PACKET_PULSE_TIME_TOLERANCE25, 4, 56},   // Acurite
	{550, 1100, PACKET_PULSE_TIME_TOLERANCE25, 0, 44},  // LaCrosse TX
	{255, 510, PACKET_PULSE_TIME_TOLERANCE20, 0, 64},   // FAAC SLH (Manchester)
	{500, 1000, PACKET_PULSE_TIME_TOLERANCE20, 0, 44},  // Hormann BiSecur
	{800, 1600, PACKET_PULSE_TIME_TOLERANCE20, 0, 12},  // Marantec
	{640, 1280, PACKET_PULSE_TIME_TOLERANCE25, 4, 56},  // Somfy Telis (Manchester)
	{400, 800, PACKET_PULSE_TIME_TOLERANCE20, 0, 64},   // Star Line (KeeLoq variant)
	{350, 700, PACKET_PULSE_TIME_TOLERANCE20, 0, 24},   // Gate TX
	{300, 900, PACKET_PULSE_TIME_TOLERANCE25, 0, 25},   // SMC5326
	{225, 675, PACKET_PULSE_TIME_TOLERANCE25, 0, 16},   // Power Smart
	{450, 1350, PACKET_PULSE_TIME_TOLERANCE20, 0, 48},  // iDo
	{555, 1110, PACKET_PULSE_TIME_TOLERANCE20, 0, 12},  // Ansonic
	{500, 1500, PACKET_PULSE_TIME_TOLERANCE25, 4, 40},  // Infactory (weather)
	{120, 240, PACKET_PULSE_TIME_TOLERANCE25, 8, 40},   // Schrader TPMS (Manchester)
	/* --- New protocols --- */
	/* Security+ 1.0: te_short=500us, ternary (3-symbol) encoding.
	 * Each sub-packet = 1 header + 20 data symbols = 21 symbol pairs = 42 pulses.
	 * Two sub-packets required to reconstruct rolling+fixed code.
	 * Matches Flipper Zero SUBGHZ_PROTOCOL_SECPLUS_V1_NAME = "Security+ 1.0"
	 * Reference: https://github.com/argilo/secplus */
	{500, 1500, PACKET_PULSE_TIME_TOLERANCE20, 0, 40},  // Security+ 1.0 (was {1000,3000} generic PWM)
	{385, 1155, PACKET_PULSE_TIME_TOLERANCE20, 0, 18},  // Clemsa
	{450, 900, PACKET_PULSE_TIME_TOLERANCE20, 0, 37},   // Doitrand
	{340, 680, PACKET_PULSE_TIME_TOLERANCE20, 0, 18},   // BETT
	{330, 990, PACKET_PULSE_TIME_TOLERANCE20, 0, 36},   // Nero Sketch/Radio
	{300, 900, PACKET_PULSE_TIME_TOLERANCE20, 0, 10},   // FireFly
	{260, 520, PACKET_PULSE_TIME_TOLERANCE20, 0, 54},   // CAME Twee
	{200, 400, PACKET_PULSE_TIME_TOLERANCE20, 0, 62},   // CAME Atomo (rolling code)
	{500, 1000, PACKET_PULSE_TIME_TOLERANCE20, 0, 52},  // Nice Flor S
	{400, 800, PACKET_PULSE_TIME_TOLERANCE20, 0, 72},   // Alutech AT-4N
	{336, 672, PACKET_PULSE_TIME_TOLERANCE20, 0, 24},   // Centurion
	{400, 1200, PACKET_PULSE_TIME_TOLERANCE20, 0, 60},  // Kinggates Stylo 4K
	{1000, 2000, PACKET_PULSE_TIME_TOLERANCE20, 0, 24}, // Megacode
	{500, 1500, PACKET_PULSE_TIME_TOLERANCE20, 0, 36},  // Mastercode
	{2000, 6000, PACKET_PULSE_TIME_TOLERANCE25, 0, 7},  // Chamberlain 7-bit
	{2000, 6000, PACKET_PULSE_TIME_TOLERANCE25, 0, 8},  // Chamberlain 8-bit
	{2000, 6000, PACKET_PULSE_TIME_TOLERANCE25, 0, 9},  // Chamberlain 9-bit
	{1000, 3000, PACKET_PULSE_TIME_TOLERANCE25, 0, 10}, // Liftmaster 10-bit
	{400, 1200, PACKET_PULSE_TIME_TOLERANCE20, 0, 40},  // Dooya
	{250, 750, PACKET_PULSE_TIME_TOLERANCE25, 0, 48},   // Honeywell
	{250, 750, PACKET_PULSE_TIME_TOLERANCE25, 0, 32},   // Intertechno
	{330, 990, PACKET_PULSE_TIME_TOLERANCE25, 0, 32},   // Elro
	{500, 1000, PACKET_PULSE_TIME_TOLERANCE25, 0, 40},  // Ambient Weather (Manchester)
	{250, 500, PACKET_PULSE_TIME_TOLERANCE25, 0, 40},   // Bresser 3ch
	{250, 500, PACKET_PULSE_TIME_TOLERANCE25, 0, 56},   // Bresser 5in1
	{250, 500, PACKET_PULSE_TIME_TOLERANCE25, 0, 104},  // Bresser 6in1
	{500, 1000, PACKET_PULSE_TIME_TOLERANCE25, 0, 48},  // TFA Dostmann
	{500, 1000, PACKET_PULSE_TIME_TOLERANCE25, 0, 36},  // Nexus-TH
	{250, 500, PACKET_PULSE_TIME_TOLERANCE25, 0, 37},   // ThermoPro TX-2
	{500, 1000, PACKET_PULSE_TIME_TOLERANCE25, 0, 40},  // GT-WT03
	{400, 800, PACKET_PULSE_TIME_TOLERANCE20, 0, 64},   // Scher-Khan Magicar
	{400, 1200, PACKET_PULSE_TIME_TOLERANCE20, 0, 64},  // Scher-Khan Logicar
	{250, 750, PACKET_PULSE_TIME_TOLERANCE20, 0, 56},   // Toyota
	{100, 300, PACKET_PULSE_TIME_TOLERANCE30, 0, 64},   // BinRAW (generic fallback)
	/* --- Flipper compatibility protocols --- */
	{400,  800,  PACKET_PULSE_TIME_TOLERANCE20, 0, 36},  // Dickert_MAHS
	{350,  750,  PACKET_PULSE_TIME_TOLERANCE20, 0, 32},  // Feron
	{500,  1200, PACKET_PULSE_TIME_TOLERANCE20, 0, 34},  // GangQi
	{300,  700,  PACKET_PULSE_TIME_TOLERANCE20, 0, 21},  // Hay21
	{200,  1000, PACKET_PULSE_TIME_TOLERANCE20, 0, 42},  // Hollarm
	{430,  870,  PACKET_PULSE_TIME_TOLERANCE20, 0, 40},  // Holtek
	{275,  1375, PACKET_PULSE_TIME_TOLERANCE20, 0, 32},  // Intertechno V3
	{250,  500,  PACKET_PULSE_TIME_TOLERANCE20, 0, 61},  // KIA Seed
	{375,  1125, PACKET_PULSE_TIME_TOLERANCE20, 0, 18},  // Legrand
	{500,  2000, PACKET_PULSE_TIME_TOLERANCE20, 0, 8},   // LinearDelta3
	{200,  400,  PACKET_PULSE_TIME_TOLERANCE20, 0, 32},  // Magellan
	{800,  1600, PACKET_PULSE_TIME_TOLERANCE20, 0, 24},  // Marantec24
	{330,  660,  PACKET_PULSE_TIME_TOLERANCE20, 0, 40},  // Nero Sketch
	{427,  853,  PACKET_PULSE_TIME_TOLERANCE20, 0, 52},  // Phoenix V2
	{250,  500,  PACKET_PULSE_TIME_TOLERANCE25, 0, 64},  // Revers_RB2
	{500,  1000, PACKET_PULSE_TIME_TOLERANCE25, 0, 28},  // Roger
	{640,  1280, PACKET_PULSE_TIME_TOLERANCE25, 0, 80},  // Somfy Keytis
};

/*
 * Protocol name strings MUST match Flipper Zero's SUBGHZ_PROTOCOL_*_NAME constants
 * (defined in lib/subghz/protocols/<proto>.h) so that .sub files saved on either
 * device are readable on the other without conversion.
 *
 * If no Flipper equivalent exists the name is kept as-is; see comments below.
 */
const char *protocol_text[] =
{
	"Princeton",          /* SUBGHZ_PROTOCOL_PRINCETON_NAME        */
	"Security+ 2.0",     /* SUBGHZ_PROTOCOL_SECPLUS_V2_NAME       */
	"CAME",              /* SUBGHZ_PROTOCOL_CAME_NAME             */
	"Nice FLO",          /* SUBGHZ_PROTOCOL_NICE_FLO_NAME         */
	"Linear",            /* SUBGHZ_PROTOCOL_LINEAR_NAME           */
	"Holtek_HT12X",      /* SUBGHZ_PROTOCOL_HOLTEK_HT12X_NAME     (was "Holtek") */
	"KeeLoq",            /* SUBGHZ_PROTOCOL_KEELOQ_NAME           */
	"Oregon v2",         /* no Flipper lib/subghz equivalent       */
	"Acurite",           /* no Flipper lib/subghz equivalent       */
	"LaCrosse TX",       /* no Flipper lib/subghz equivalent       */
	"Faac SLH",          /* SUBGHZ_PROTOCOL_FAAC_SLH_NAME         (was "FAAC SLH") */
	"Hormann HSM",       /* SUBGHZ_PROTOCOL_HORMANN_HSM_NAME      (was "Hormann") */
	"Marantec",          /* SUBGHZ_PROTOCOL_MARANTEC_NAME         */
	"Somfy Telis",       /* SUBGHZ_PROTOCOL_SOMFY_TELIS_NAME      */
	"Star Line",         /* SUBGHZ_PROTOCOL_STAR_LINE_NAME        */
	"GateTX",            /* SUBGHZ_PROTOCOL_GATE_TX_NAME          (was "Gate TX") */
	"SMC5326",           /* SUBGHZ_PROTOCOL_SMC5326_NAME          */
	"Power Smart",       /* SUBGHZ_PROTOCOL_POWER_SMART_NAME      */
	"iDo 117/111",       /* SUBGHZ_PROTOCOL_IDO_NAME              (was "iDo") */
	"Ansonic",           /* SUBGHZ_PROTOCOL_ANSONIC_NAME          */
	"Infactory",         /* no Flipper lib/subghz equivalent       */
	"Schrader TPMS",     /* no Flipper lib/subghz equivalent       */
	/* --- Protocols added by community contributions --- */
	"Cham_Code",         /* SUBGHZ_PROTOCOL_CHAMB_CODE_NAME       (was "Chamberlain") */
	"Clemsa",            /* SUBGHZ_PROTOCOL_CLEMSA_NAME           */
	"Doitrand",          /* SUBGHZ_PROTOCOL_DOITRAND_NAME         */
	"BETT",              /* SUBGHZ_PROTOCOL_BETT_NAME             */
	"Nero Radio",        /* SUBGHZ_PROTOCOL_NERO_RADIO_NAME       */
	"FireFly",           /* no Flipper lib/subghz equivalent       */
	"CAME TWEE",         /* SUBGHZ_PROTOCOL_CAME_TWEE_NAME        (was "CAME Twee") */
	"CAME Atomo",        /* SUBGHZ_PROTOCOL_CAME_ATOMO_NAME       */
	"Nice FloR-S",       /* SUBGHZ_PROTOCOL_NICE_FLOR_S_NAME      (was "Nice Flor S") */
	"Alutech AT-4N",     /* SUBGHZ_PROTOCOL_ALUTECH_AT_4N_NAME   */
	"Centurion",         /* no Flipper lib/subghz equivalent       */
	"KingGates Stylo4k", /* SUBGHZ_PROTOCOL_KINGGATES_STYLO_4K_NAME (was "Kinggates Stylo") */
	"MegaCode",          /* SUBGHZ_PROTOCOL_MEGACODE_NAME         (was "Megacode") */
	"Mastercode",        /* SUBGHZ_PROTOCOL_MASTERCODE_NAME       */
	"Cham_Code",         /* SUBGHZ_PROTOCOL_CHAMB_CODE_NAME 7-bit  (was "Chamberlain 7") */
	"Cham_Code",         /* SUBGHZ_PROTOCOL_CHAMB_CODE_NAME 8-bit  (was "Chamberlain 8") */
	"Cham_Code",         /* SUBGHZ_PROTOCOL_CHAMB_CODE_NAME 9-bit  (was "Chamberlain 9") */
	"Security+ 1.0",     /* SUBGHZ_PROTOCOL_SECPLUS_V1_NAME (Liftmaster 10-bit variant) */
	"Dooya",             /* SUBGHZ_PROTOCOL_DOOYA_NAME            */
	"Honeywell",         /* no Flipper lib/subghz equivalent       */
	"Intertechno",       /* no Flipper lib/subghz equivalent       */
	"Elro",              /* no Flipper lib/subghz equivalent       */
	"Ambient Weather",   /* no Flipper lib/subghz equivalent       */
	"Bresser 3ch",       /* no Flipper lib/subghz equivalent       */
	"Bresser 5in1",      /* no Flipper lib/subghz equivalent       */
	"Bresser 6in1",      /* no Flipper lib/subghz equivalent       */
	"TFA Dostmann",      /* no Flipper lib/subghz equivalent       */
	"Nexus-TH",          /* no Flipper lib/subghz equivalent       */
	"ThermoPro TX-2",    /* no Flipper lib/subghz equivalent       */
	"GT-WT03",           /* no Flipper lib/subghz equivalent       */
	"Scher-Khan",        /* SUBGHZ_PROTOCOL_SCHER_KHAN_NAME       (was "Scher-Khan Magicar") */
	"Scher-Khan",        /* SUBGHZ_PROTOCOL_SCHER_KHAN_NAME       (was "Scher-Khan Logicar") */
	"Toyota",            /* no Flipper lib/subghz equivalent       */
	"BinRAW",            /* SUBGHZ_PROTOCOL_BIN_RAW_NAME          */
	/* --- Flipper compatibility protocols --- */
	"Dickert_MAHS",     /* SUBGHZ_PROTOCOL_DICKERT_MAHS_NAME     */
	"Feron",            /* SUBGHZ_PROTOCOL_FERON_NAME            */
	"GangQi",           /* SUBGHZ_PROTOCOL_GANGQI_NAME           */
	"Hay21",            /* SUBGHZ_PROTOCOL_HAY21_NAME            */
	"Hollarm",          /* SUBGHZ_PROTOCOL_HOLLARM_NAME          */
	"Holtek",           /* SUBGHZ_PROTOCOL_HOLTEK_NAME           */
	"Intertechno_V3",   /* SUBGHZ_PROTOCOL_INTERTECHNO_V3_NAME   */
	"KIA Seed",         /* SUBGHZ_PROTOCOL_KIA_NAME              */
	"Legrand",          /* SUBGHZ_PROTOCOL_LEGRAND_NAME          */
	"LinearDelta3",     /* SUBGHZ_PROTOCOL_LINEAR_DELTA3_NAME    */
	"Magellan",         /* SUBGHZ_PROTOCOL_MAGELLAN_NAME         */
	"Marantec24",       /* SUBGHZ_PROTOCOL_MARANTEC24_NAME       */
	"Nero Sketch",      /* SUBGHZ_PROTOCOL_NERO_SKETCH_NAME      */
	"Phoenix_V2",       /* SUBGHZ_PROTOCOL_PHOENIX_V2_NAME       */
	"Revers_RB2",       /* SUBGHZ_PROTOCOL_REVERSRB2_NAME        */
	"Roger",            /* SUBGHZ_PROTOCOL_ROGER_NAME            */
	"Somfy Keytis"      /* SUBGHZ_PROTOCOL_SOMFY_KEYTIS_NAME     */
};


enum {
   n_protocol = sizeof(subghz_protocols_list) / sizeof(subghz_protocols_list[0])
};


//************************** S T R U C T U R E S *******************************

/***************************** V A R I A B L E S ******************************/

SubGHz_DecEnc_t subghz_decenc_ctl;
SubGHz_Weather_Data_t weather_data;

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
  return (subghz_decenc_ctl.n64_decodedvalue != 0);
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
	subghz_decenc_ctl.ndecodedbitlength = 0;
	subghz_decenc_ctl.n32_serialnumber = 0;
	subghz_decenc_ctl.n32_rollingcode = 0;
	subghz_decenc_ctl.n8_buttonid = 0;
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



/*============================================================================*/
/**
  * @brief
  * @param  None
  * @retval None
  */
/*============================================================================*/
const SubGHz_Weather_Data_t* subghz_get_weather_data(void)
{
    return &weather_data;
}

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

    	case CAME_12BIT:
    		ret = subghz_decode_came(p, pulsecount);
    		break;

    	case NICE_FLO:
    		ret = subghz_decode_nice_flo(p, pulsecount);
    		break;

    	case LINEAR_10BIT:
    		ret = subghz_decode_linear(p, pulsecount);
    		break;

    	case HOLTEK_HT12E:
    		ret = subghz_decode_holtek(p, pulsecount);
    		break;

    	case KEELOQ:
    		ret = subghz_decode_keeloq(p, pulsecount);
    		break;

    	case OREGON_V2:
    		ret = subghz_decode_oregon_v2(p, pulsecount);
    		break;

    	case ACURITE:
    		ret = subghz_decode_acurite(p, pulsecount);
    		break;

    	case LACROSSE_TX:
    		ret = subghz_decode_lacrosse_tx(p, pulsecount);
    		break;

    	case FAAC_SLH:
    		ret = subghz_decode_faac_slh(p, pulsecount);
    		break;

    	case HORMANN:
    		ret = subghz_decode_hormann(p, pulsecount);
    		break;

    	case MARANTEC:
    		ret = subghz_decode_marantec(p, pulsecount);
    		break;

    	case SOMFY_TELIS:
    		ret = subghz_decode_somfy_telis(p, pulsecount);
    		break;

    	case STAR_LINE:
    		ret = subghz_decode_starline(p, pulsecount);
    		break;

    	case GATE_TX:
    		ret = subghz_decode_gate_tx(p, pulsecount);
    		break;

    	case SMC5326:
    		ret = subghz_decode_smc5326(p, pulsecount);
    		break;

    	case POWER_SMART:
    		ret = subghz_decode_power_smart(p, pulsecount);
    		break;

    	case IDO:
    		ret = subghz_decode_ido(p, pulsecount);
    		break;

    	case ANSONIC:
    		ret = subghz_decode_ansonic(p, pulsecount);
    		break;

    	case INFACTORY:
    		ret = subghz_decode_infactory(p, pulsecount);
    		break;

    	case SCHRADER_TPMS:
    		ret = subghz_decode_schrader(p, pulsecount);
    		break;

    	/* --- New protocols --- */
    	case CHAMBERLAIN:
    		ret = subghz_decode_chamberlain(p, pulsecount);
    		break;

    	case CLEMSA:
    		ret = subghz_decode_clemsa(p, pulsecount);
    		break;

    	case DOITRAND:
    		ret = subghz_decode_doitrand(p, pulsecount);
    		break;

    	case BETT:
    		ret = subghz_decode_bett(p, pulsecount);
    		break;

    	case NERO_RADIO:
    		ret = subghz_decode_nero(p, pulsecount);
    		break;

    	case FIREFLY:
    		ret = subghz_decode_firefly(p, pulsecount);
    		break;

    	case CAME_TWEE:
    		ret = subghz_decode_came_twee(p, pulsecount);
    		break;

    	case CAME_ATOMO:
    		ret = subghz_decode_came_atomo(p, pulsecount);
    		break;

    	case NICE_FLOR_S:
    		ret = subghz_decode_nice_flor_s(p, pulsecount);
    		break;

    	case ALUTECH_AT4N:
    		ret = subghz_decode_alutech(p, pulsecount);
    		break;

    	case CENTURION:
    		ret = subghz_decode_centurion(p, pulsecount);
    		break;

    	case KINGGATES_STYLO:
    		ret = subghz_decode_kinggates(p, pulsecount);
    		break;

    	case MEGACODE:
    		ret = subghz_decode_megacode(p, pulsecount);
    		break;

    	case MASTERCODE:
    		ret = subghz_decode_mastercode(p, pulsecount);
    		break;

    	case CHAMBERLAIN_7BIT:
    		ret = subghz_decode_chamberlain_7bit(p, pulsecount);
    		break;

    	case CHAMBERLAIN_8BIT:
    		ret = subghz_decode_chamberlain_8bit(p, pulsecount);
    		break;

    	case CHAMBERLAIN_9BIT:
    		ret = subghz_decode_chamberlain_9bit(p, pulsecount);
    		break;

    	case LIFTMASTER_10BIT:
    		ret = subghz_decode_liftmaster(p, pulsecount);
    		break;

    	case DOOYA:
    		ret = subghz_decode_dooya(p, pulsecount);
    		break;

    	case HONEYWELL:
    		ret = subghz_decode_honeywell(p, pulsecount);
    		break;

    	case INTERTECHNO:
    		ret = subghz_decode_intertechno(p, pulsecount);
    		break;

    	case ELRO:
    		ret = subghz_decode_elro(p, pulsecount);
    		break;

    	case AMBIENT_WEATHER:
    		ret = subghz_decode_ambient_weather(p, pulsecount);
    		break;

    	case BRESSER_3CH:
    		ret = subghz_decode_bresser_3ch(p, pulsecount);
    		break;

    	case BRESSER_5IN1:
    		ret = subghz_decode_bresser_5in1(p, pulsecount);
    		break;

    	case BRESSER_6IN1:
    		ret = subghz_decode_bresser_6in1(p, pulsecount);
    		break;

    	case TFA_DOSTMANN:
    		ret = subghz_decode_tfa_dostmann(p, pulsecount);
    		break;

    	case NEXUS_TH:
    		ret = subghz_decode_nexus_th(p, pulsecount);
    		break;

    	case THERMOPRO_TX2:
    		ret = subghz_decode_thermopro_tx2(p, pulsecount);
    		break;

    	case GT_WT03:
    		ret = subghz_decode_gt_wt03(p, pulsecount);
    		break;

    	case SCHER_KHAN_MAGICAR:
    		ret = subghz_decode_scher_khan_magicar(p, pulsecount);
    		break;

    	case SCHER_KHAN_LOGICAR:
    		ret = subghz_decode_scher_khan_logicar(p, pulsecount);
    		break;

    	case TOYOTA:
    		ret = subghz_decode_toyota(p, pulsecount);
    		break;

    	case BIN_RAW:
    		ret = subghz_decode_bin_raw(p, pulsecount);
    		break;

    	case DICKERT_MAHS:
    		ret = subghz_decode_dickert_mahs(p, pulsecount);
    		break;

    	case FERON:
    		ret = subghz_decode_feron(p, pulsecount);
    		break;

    	case GANGQI:
    		ret = subghz_decode_gangqi(p, pulsecount);
    		break;

    	case HAY21:
    		ret = subghz_decode_hay21(p, pulsecount);
    		break;

    	case HOLLARM:
    		ret = subghz_decode_hollarm(p, pulsecount);
    		break;

    	case HOLTEK_BASE:
    		ret = subghz_decode_holtek_base(p, pulsecount);
    		break;

    	case INTERTECHNO_V3:
    		ret = subghz_decode_intertechno_v3(p, pulsecount);
    		break;

    	case KIA_SEED:
    		ret = subghz_decode_kia_seed(p, pulsecount);
    		break;

    	case LEGRAND:
    		ret = subghz_decode_legrand(p, pulsecount);
    		break;

    	case LINEAR_DELTA3:
    		ret = subghz_decode_linear_delta3(p, pulsecount);
    		break;

    	case MAGELLAN:
    		ret = subghz_decode_magellan(p, pulsecount);
    		break;

    	case MARANTEC24:
    		ret = subghz_decode_marantec24(p, pulsecount);
    		break;

    	case NERO_SKETCH:
    		ret = subghz_decode_nero_sketch(p, pulsecount);
    		break;

    	case PHOENIX_V2:
    		ret = subghz_decode_phoenix_v2(p, pulsecount);
    		break;

    	case REVERS_RB2:
    		ret = subghz_decode_revers_rb2(p, pulsecount);
    		break;

    	case ROGER:
    		ret = subghz_decode_roger(p, pulsecount);
    		break;

    	case SOMFY_KEYTIS:
    		ret = subghz_decode_somfy_keytis(p, pulsecount);
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
        if (value)
        {
            received->frequency = 0;
            received->key = subghz_decenc_ctl.subghz_get_decoded_value();
            received->protocol = subghz_decenc_ctl.subghz_get_decoded_protocol();
            received->rssi = subghz_decenc_ctl.subghz_get_decoded_rssi();
            received->te = subghz_decenc_ctl.subghz_get_decoded_delay();
            received->bit_len = subghz_decenc_ctl.subghz_get_decoded_bitlength();
            /* Copy extended fields set by protocol-specific decoders */
            received->serial_number = subghz_decenc_ctl.n32_serialnumber;
            received->rolling_code  = subghz_decenc_ctl.n32_rollingcode;
            received->button_id     = subghz_decenc_ctl.n8_buttonid;
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
	subghz_decenc_ctl.n64_decodedvalue = 0;
} // void subghz_decenc_init(void)


/*============================================================================*/
/* Signal History Ring Buffer Implementation                                   */
/*============================================================================*/

void subghz_history_reset(SubGHz_History_t *hist)
{
	hist->count = 0;
	hist->head  = 0;
}

uint8_t subghz_history_add(SubGHz_History_t *hist, const SubGHz_Dec_Info_t *info, uint32_t freq_hz)
{
	/* Check for duplicate: same protocol + key + bit_len as the most recent entry */
	if (hist->count > 0)
	{
		uint8_t last_idx = (hist->head == 0) ? SUBGHZ_HISTORY_MAX - 1 : hist->head - 1;
		SubGHz_History_Entry_t *last = &hist->entries[last_idx];
		if (last->info.protocol == info->protocol &&
		    last->info.key      == info->key &&
		    last->info.bit_len  == info->bit_len)
		{
			/* Duplicate — increment count, update RSSI to latest */
			if (last->count < 255)
				last->count++;
			last->info.rssi = info->rssi;
			return hist->count;
		}
	}

	/* Write new entry at head position */
	SubGHz_History_Entry_t *entry = &hist->entries[hist->head];
	entry->info          = *info;
	entry->info.raw_data = NULL;   /* Do not store raw data pointer */
	entry->info.raw      = false;
	entry->frequency     = freq_hz;
	entry->count         = 1;

	hist->head = (hist->head + 1) % SUBGHZ_HISTORY_MAX;
	if (hist->count < SUBGHZ_HISTORY_MAX)
		hist->count++;

	return hist->count;
}

const SubGHz_History_Entry_t *subghz_history_get(const SubGHz_History_t *hist, uint8_t idx)
{
	if (idx >= hist->count)
		return NULL;
	/* Index 0 = most recent, index (count-1) = oldest */
	uint8_t pos;
	if (hist->head >= (idx + 1))
		pos = hist->head - idx - 1;
	else
		pos = SUBGHZ_HISTORY_MAX - (idx + 1 - hist->head);
	return &hist->entries[pos];
}
