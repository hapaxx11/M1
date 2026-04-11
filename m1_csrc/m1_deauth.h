/* See COPYING.txt for license details. */

/*
 * m1_deauth.h
 *
 * WiFi deauthentication tool — requires custom ESP32 AT firmware
 * with AT+DEAUTH and AT+STASCAN support.
 *
 * Ported from neddy299/M1, adapted for Hapax scene architecture.
 *
 * M1 Project
 */

#ifndef M1_DEAUTH_H_
#define M1_DEAUTH_H_

#include <stdbool.h>
#include "m1_compile_cfg.h"

/**
 * @brief  Run the WiFi deauthentication tool (blocking delegate).
 *
 * Flow: preflight AT command check → main menu → AP scan → AP confirm →
 * station scan → station confirm → attack (toggle ON/OFF) → complete.
 *
 * Requires custom ESP32-C6 AT firmware with:
 *   AT+DEAUTH=<mode>,<channel>,"<sta_mac>","<bssid>"  — start deauth
 *   AT+DEAUTH=0                                       — stop deauth
 *   AT+DEAUTH?                                        — query status
 *   AT+STASCAN=1,<channel>,"<bssid>"                  — start station scan
 *   AT+STASCAN=0                                      — stop station scan
 *   AT+STASCAN?                                       — query scan status
 */
void wifi_deauth(void);

#endif /* M1_DEAUTH_H_ */
