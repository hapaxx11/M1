/* See COPYING.txt for license details. */

/*
*
*  m1_rfid.h
*
*  M1 RFID functions
*
* M1 Project
*
*/
#ifndef M1_RFID_H_
#define M1_RFID_H_


void menu_125khz_rfid_init(void);
void menu_125khz_rfid_deinit(void);

void rfid_125khz_read(void);
void rfid_125khz_pet_scan(void);
void rfid_125khz_saved(void);
void rfid_125khz_add_manually(void);
void rfid_125khz_utilities(void);


/////////////////////////////////////////////////////////////////////////////




#endif /* M1_RFID_H_ */
