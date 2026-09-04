/* See COPYING.txt for license details. */

#ifndef M1_RFID_WATCHDOG_H_
#define M1_RFID_WATCHDOG_H_

#include <stdbool.h>

typedef void (*m1_rfid_watchdog_kick_fn)(void);

/* A processed RFID scan message indicates sustained work that may starve the
 * watchdog task; idle iterations blocked waiting for a message do not. */
static inline void m1_rfid_scan_watchdog_kick(
	bool message_processed,
	m1_rfid_watchdog_kick_fn kick)
{
	if(message_processed && kick)
	{
		kick();
	}
}

#endif /* M1_RFID_WATCHDOG_H_ */
