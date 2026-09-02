/* See COPYING.txt for license details. */

/*
*
* m1_ir_universal.h
*
* Universal Remote with Flipper IRDB support
*
* M1 Project
*
*/

#ifndef M1_IR_UNIVERSAL_H_
#define M1_IR_UNIVERSAL_H_

#include <stdint.h>
#include <stdbool.h>
#include "ir_signal_record.h"   /* ir_universal_cmd_t (Phase G) */

/* IR_UNIVERSAL_NAME_MAX_LEN kept for backward compatibility; see IR_CMD_NAME_MAX_LEN */
#define IR_UNIVERSAL_NAME_MAX_LEN    IR_CMD_NAME_MAX_LEN
#define IR_UNIVERSAL_MAX_CMDS        64
#define IR_UNIVERSAL_BROWSE_PAGE_SIZE 6
#define IR_UNIVERSAL_MAX_FAVORITES   10
#define IR_UNIVERSAL_MAX_RECENT      10
#define IR_UNIVERSAL_PATH_MAX_LEN    256
#define IR_UNIVERSAL_IRDB_ROOT       "0:/IR"

typedef enum {
	IR_UNIVERSAL_MODE_DASHBOARD = 0,
	IR_UNIVERSAL_MODE_BROWSE_CATEGORY,
	IR_UNIVERSAL_MODE_BROWSE_BRAND,
	IR_UNIVERSAL_MODE_BROWSE_DEVICE,
	IR_UNIVERSAL_MODE_COMMANDS,
	IR_UNIVERSAL_MODE_SEARCH,
	IR_UNIVERSAL_MODE_FAVORITES,
	IR_UNIVERSAL_MODE_RECENT
} ir_universal_mode_t;

/* Main entry point - called from m1_infrared.c */
void ir_universal_run(void);

/* Browse directly to learned files - called from infrared_saved_remotes() */
void ir_universal_run_learned(void);

/* Initialize/cleanup */
void ir_universal_init(void);
void ir_universal_deinit(void);

/* Bounded file-level transmit helper used by ESP-NOW remote trigger. */
bool m1_ir_universal_send_file_all(const char *ir_file_path);

typedef enum {
    M1_IR_FILE_TX_RUNNING = 0,
    M1_IR_FILE_TX_DONE,
    M1_IR_FILE_TX_FAILED,
} m1_ir_file_tx_status_t;

bool m1_ir_universal_start_file_all(const char *ir_file_path);
m1_ir_file_tx_status_t m1_ir_universal_continue_file_all(void);
void m1_ir_universal_abort_file_all(void);

#endif /* M1_IR_UNIVERSAL_H_ */
