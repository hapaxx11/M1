/* See COPYING.txt for license details. */

#include "nfc_ndef_authoring.h"

#include <stdbool.h>
#include <string.h>

#include "nfc_ndef_encode.h"

bool nfc_ndef_authoring_fits_capacity(
    size_t ndef_size,
    const uint8_t capability_container[4])
{
    if (ndef_size == 0u || !capability_container ||
        capability_container[0] != 0xE1u ||
        (capability_container[1] & 0xF0u) != 0x10u ||
        capability_container[2] == 0u)
        return false;

    size_t user_pages = (size_t)capability_container[2] * 2u;
    return ndef_pages_needed(ndef_size) <= user_pages;
}

size_t nfc_ndef_authoring_encode(
    nfc_ndef_authoring_type_t type,
    const char *value,
    const char *secondary_value,
    uint8_t *out,
    size_t out_size)
{
    if (!value || !out || out_size == 0 || value[0] == '\0')
        return 0;

    switch (type) {
        case NFC_NDEF_AUTHOR_URI:
            return ndef_encode_uri_auto(out, out_size, value);
        case NFC_NDEF_AUTHOR_TEXT:
            return ndef_encode_text(out, out_size, "en", value);
        case NFC_NDEF_AUTHOR_PHONE:
            return ndef_encode_phone(out, out_size, value);
        case NFC_NDEF_AUTHOR_WIFI:
            if (secondary_value && secondary_value[0] != '\0')
                return ndef_encode_wifi(out, out_size, value, secondary_value, true);
            return ndef_encode_wifi(out, out_size, value, NULL, false);
        default:
            return 0;
    }
}
