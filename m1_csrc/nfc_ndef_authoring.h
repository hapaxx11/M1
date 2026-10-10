/* See COPYING.txt for license details. */

#ifndef NFC_NDEF_AUTHORING_H_
#define NFC_NDEF_AUTHORING_H_

#include <stddef.h>
#include <stdint.h>

typedef enum {
    NFC_NDEF_AUTHOR_URI = 0,
    NFC_NDEF_AUTHOR_TEXT,
    NFC_NDEF_AUTHOR_PHONE,
    NFC_NDEF_AUTHOR_WIFI,
} nfc_ndef_authoring_type_t;

size_t nfc_ndef_authoring_encode(
    nfc_ndef_authoring_type_t type,
    const char *value,
    const char *secondary_value,
    uint8_t *out,
    size_t out_size);

#endif
