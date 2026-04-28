/* See COPYING.txt for license details. */

/*
*
* m1_bt.h
*
* Header for M1 bluetooth
*
* M1 Project
*
*/

#ifndef M1_BT_H_
#define M1_BT_H_

void menu_bluetooth_init(void);
void bluetooth_config(void);
void bluetooth_scan(void);
void bluetooth_advertise(void);
void ble_sniff_analyzer(void);
void ble_sniff_generic(void);
void ble_sniff_flipper(void);
void ble_sniff_airtag(void);
void ble_monitor_airtag(void);
void ble_wardrive(void);
void ble_wardrive_continuous(void);
void ble_detect_skimmers(void);
void ble_sniff_flock(void);
void ble_wardrive_flock(void);
void ble_detect_meta(void);
void ble_spam_sour_apple(void);
void ble_spam_swiftpair(void);
void ble_spam_samsung(void);
void ble_spam_flipper(void);
void ble_spam_all(void);
void ble_spoof_airtag(void);

#endif /* M1_BT_H_ */
