/* See COPYING.txt for license details. */

/*
*
* m1_fw_update.h
*
* Firmware update functionality
*
* M1 Project
*
*/

#ifndef M1_FW_UPDATE_H_
#define M1_FW_UPDATE_H_

void firmware_update_init(void);
void firmware_update_exit(void);
void firmware_update_get_image_file(void);
void firmware_update_start(void);
void firmware_swap_banks(void);

#endif /* M1_FW_UPDATE_H_ */
