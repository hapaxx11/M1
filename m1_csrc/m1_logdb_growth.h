/* See COPYING.txt for license details. */

/*
 *
 * m1_logdb_growth.h
 *
 * Pure-logic buffer-growth decision used by m1_logdb_dyn_vsprintf() to size
 * the dynamic log-message buffer. Extracted so the size-growth arithmetic
 * (in particular the retry-vs-give-up decision) can be unit tested on the
 * host without pulling in FreeRTOS/HAL/vsnprintf.
 *
 * M1 Project
 *
 */

#ifndef M1_LOGDB_GROWTH_H
#define M1_LOGDB_GROWTH_H

/*************************** I N C L U D E S **********************************/

/*************************** D E F I N E S ************************************/

/*************************** T Y P E S *****************************************/

typedef enum
{
	M1_LOGDB_GROW_DONE,		/* current buffer was big enough; use it as-is */
	M1_LOGDB_GROW_RETRY,	/* reallocate to *out_new_size and try again */
	M1_LOGDB_GROW_GIVE_UP	/* exceeded max_size; abandon the message */
} m1_logdb_grow_action_t;

/*************************** F U N C T I O N S *********************************/

/*============================================================================*/
/*
 * Decide the next step for m1_logdb_dyn_vsprintf()'s dynamic buffer growth,
 * given the vsnprintf() return value (ret_n) and the size that was just
 * tried (cur_size). max_size is the largest buffer size the caller is
 * willing to allocate.
 *
 * On M1_LOGDB_GROW_RETRY, *out_new_size holds the size to allocate next.
 *
 * NOTE: this mirrors m1_logdb_dyn_vsprintf()'s loop body exactly. It must
 * use a signed integer type wide enough to hold (ret_n + 20) without
 * wrapping — using a narrow type here (e.g. uint8_t) reproduces the fixed
 * bug where any message >= (max_size - 20) bytes wraps the requested size
 * back below max_size and spins forever instead of giving up.
 */
/*============================================================================*/
static inline m1_logdb_grow_action_t m1_logdb_next_alloc_size(int ret_n, int cur_size, int max_size, int *out_new_size)
{
	int next_size;

	if ( ret_n > -1 ) // Good try?
	{
		if ( ret_n < cur_size ) // Good size?
			return M1_LOGDB_GROW_DONE;
		next_size = ret_n + 20; // Adjust the allocated space
	}
	else
		next_size = 2 * max_size + 1; // Force the give-up path below

	if ( next_size > max_size ) // Memory size exceeds the maximum allowed size?
		return M1_LOGDB_GROW_GIVE_UP;

	*out_new_size = next_size;
	return M1_LOGDB_GROW_RETRY;
} // static inline m1_logdb_grow_action_t m1_logdb_next_alloc_size(...)

#endif /* M1_LOGDB_GROWTH_H */
