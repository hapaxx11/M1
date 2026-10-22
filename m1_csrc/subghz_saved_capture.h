/* See COPYING.txt for license details. */

#ifndef SUBGHZ_SAVED_CAPTURE_H_
#define SUBGHZ_SAVED_CAPTURE_H_

#include <stdbool.h>
#include <stddef.h>
#include "m1_sub_ghz_decenc.h"
#include "flipper_subghz.h"
#include "subghz_saved_match.h"

void subghz_saved_capture_identity_from_info(
    const SubGHz_Dec_Info_t *info, const char *protocol_name,
    subghz_saved_match_identity_t *out);

void subghz_saved_capture_from_history(
    const SubGHz_History_Entry_t *entry, const char *protocol_name,
    flipper_subghz_key_capture_t *out);

bool subghz_saved_capture_read_identity(
    const char *path, subghz_saved_match_identity_t *out);

bool subghz_saved_capture_find_match(
    const char *directory, const SubGHz_Dec_Info_t *received,
    const char *protocol_name, bool allow_serial_match,
    char *out_path, size_t out_path_size);

bool subghz_saved_capture_get_format(
    const char *path, bool *is_m1_native, char *preset, size_t preset_size);

typedef bool (*subghz_saved_capture_writer_t)(
    const char *path, bool m1_native,
    const flipper_subghz_key_capture_t *capture);

bool subghz_saved_capture_update(
    const char *path, bool m1_native,
    const flipper_subghz_key_capture_t *capture,
    subghz_saved_capture_writer_t writer);

#endif /* SUBGHZ_SAVED_CAPTURE_H_ */
