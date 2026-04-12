/* Minimal irmp.h stub for host-side unit tests.
 * Provides only the IRMP_*_PROTOCOL constants referenced by flipper_ir.c.
 * Values match Infrared/irmpprotocols.h in the firmware tree. */

#ifndef IRMP_H_STUB
#define IRMP_H_STUB

#define IRMP_UNKNOWN_PROTOCOL                    0
#define IRMP_SIRCS_PROTOCOL                      1
#define IRMP_NEC_PROTOCOL                        2
#define IRMP_SAMSUNG_PROTOCOL                    3
#define IRMP_MATSUSHITA_PROTOCOL                 4
#define IRMP_KASEIKYO_PROTOCOL                   5
#define IRMP_RECS80_PROTOCOL                     6
#define IRMP_RC5_PROTOCOL                        7
#define IRMP_DENON_PROTOCOL                      8
#define IRMP_RC6_PROTOCOL                        9
#define IRMP_SAMSUNG32_PROTOCOL                 10
#define IRMP_APPLE_PROTOCOL                     11
#define IRMP_RECS80EXT_PROTOCOL                 12
#define IRMP_NUBERT_PROTOCOL                    13
#define IRMP_BANG_OLUFSEN_PROTOCOL              14
#define IRMP_GRUNDIG_PROTOCOL                   15
#define IRMP_NOKIA_PROTOCOL                     16
#define IRMP_SIEMENS_PROTOCOL                   17
#define IRMP_FDC_PROTOCOL                       18
#define IRMP_RCCAR_PROTOCOL                     19
#define IRMP_JVC_PROTOCOL                       20
#define IRMP_RC6A_PROTOCOL                      21
#define IRMP_NIKON_PROTOCOL                     22
#define IRMP_RUWIDO_PROTOCOL                    23
#define IRMP_IR60_PROTOCOL                      24
#define IRMP_KATHREIN_PROTOCOL                  25
#define IRMP_NETBOX_PROTOCOL                    26
#define IRMP_NEC16_PROTOCOL                     27
#define IRMP_NEC42_PROTOCOL                     28
#define IRMP_LEGO_PROTOCOL                      29
#define IRMP_THOMSON_PROTOCOL                   30
#define IRMP_BOSE_PROTOCOL                      31
#define IRMP_A1TVBOX_PROTOCOL                   32
#define IRMP_ORTEK_PROTOCOL                     33
#define IRMP_TELEFUNKEN_PROTOCOL                34
#define IRMP_ROOMBA_PROTOCOL                    35
#define IRMP_RCMM32_PROTOCOL                    36
#define IRMP_RCMM24_PROTOCOL                    37
#define IRMP_RCMM12_PROTOCOL                    38
#define IRMP_SPEAKER_PROTOCOL                   39
#define IRMP_LGAIR_PROTOCOL                     40
#define IRMP_SAMSUNG48_PROTOCOL                 41

/* Stub IRMP_DATA structure (not used by protocol mapping tests) */
typedef struct {
    uint8_t  protocol;
    uint16_t address;
    uint16_t command;
    uint8_t  flags;
} IRMP_DATA;

#endif /* IRMP_H_STUB */
