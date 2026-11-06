/* See COPYING.txt for license details. */
#ifndef WIFI_PMKID_CAPTURE_H_
#define WIFI_PMKID_CAPTURE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Extract the AP BSSID, frame receiver address, and standard RSN PMKID KDE from
 * a LINKTYPE_IEEE802_11 PCAP produced by the native brain. Returns false for
 * malformed input or when no complete PMKID element is present. */
bool wifi_pmkid_pcap_extract(const uint8_t *pcap, size_t pcap_len,
                             uint8_t bssid[6], uint8_t station[6], uint8_t pmkid[16]);

#endif /* WIFI_PMKID_CAPTURE_H_ */
