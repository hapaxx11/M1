/* Minimal source file that provides ONLY the SubGHz history ring buffer
 * functions for host-side unit testing.  These functions are pure data
 * structure operations with no hardware dependencies.
 *
 * The canonical implementations live in Sub_Ghz/m1_sub_ghz_decenc.c.
 * This file re-includes that implementation via a focused compilation
 * that excludes all the hardware-dependent code. */

#include <string.h>
#include <stdlib.h>
#include "m1_sub_ghz_decenc.h"

/* ---- Copied from Sub_Ghz/m1_sub_ghz_decenc.c (lines 447-498) ---- */

void subghz_history_reset(SubGHz_History_t *hist)
{
	hist->count = 0;
	hist->head  = 0;
}

uint8_t subghz_history_add(SubGHz_History_t *hist, const SubGHz_Dec_Info_t *info, uint32_t freq_hz)
{
	/* Default semantics: dedup against last entry, evict oldest on overflow. */
	return subghz_history_add_ex(hist, info, freq_hz, true, true);
}

uint8_t subghz_history_add_ex(SubGHz_History_t *hist, const SubGHz_Dec_Info_t *info,
                              uint32_t freq_hz,
                              bool remove_duplicates, bool delete_old_signals)
{
	/* Phase 12: when remove_duplicates is true, check against the most
	 * recent entry on (protocol, key, bit_len) and merge instead of
	 * creating a new row.  When false, every reception adds a new entry. */
	if (remove_duplicates && hist->count > 0)
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

	/* Phase 12: when delete_old_signals is false and the ring is full,
	 * drop the new entry instead of evicting the oldest. */
	if (!delete_old_signals && hist->count >= SUBGHZ_HISTORY_MAX)
		return hist->count;

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
