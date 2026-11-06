/* See COPYING.txt for license details. */
#include "wifi_pmkid_capture.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool extract_frame(const uint8_t *f, size_t len,
                          uint8_t bssid[6], uint8_t pmkid[16])
{
    if (len < 24 + 8 + 99 || (f[0] & 0x0C) != 0x08) return false;
    size_t hdrlen = 24 + ((f[0] & 0x80) ? 2 : 0);
    if (len < hdrlen + 8 + 99) return false;
    const uint8_t *llc = f + hdrlen;
    static const uint8_t snap[] = { 0xAA, 0xAA, 0x03, 0, 0, 0, 0x88, 0x8E };
    if (memcmp(llc, snap, sizeof(snap)) != 0) return false;

    const uint8_t *eapol = llc + 8;
    size_t avail = len - hdrlen - 8;
    if (eapol[1] != 3) return false;
    uint16_t key_info = ((uint16_t)eapol[5] << 8) | eapol[6];
    if (!(key_info & 0x0080) || (key_info & 0x0100)) return false;
    uint16_t kd_len = ((uint16_t)eapol[97] << 8) | eapol[98];
    if (kd_len > avail - 99) return false;

    const uint8_t *kd = eapol + 99;
    for (uint16_t off = 0; off + 2 <= kd_len;) {
        uint8_t el_len = kd[off + 1];
        if (off + 2 + el_len > kd_len) break;
        if (kd[off] == 0xDD && el_len >= 20 &&
            kd[off + 2] == 0x00 && kd[off + 3] == 0x0F &&
            kd[off + 4] == 0xAC && kd[off + 5] == 0x04) {
            memcpy(bssid, f + 10, 6);
            memcpy(pmkid, kd + off + 6, 16);
            return true;
        }
        off = (uint16_t)(off + 2 + el_len);
    }
    return false;
}

bool wifi_pmkid_pcap_extract(const uint8_t *pcap, size_t pcap_len,
                             uint8_t bssid[6], uint8_t pmkid[16])
{
    if (!pcap || !bssid || !pmkid || pcap_len < 24 + 16) return false;
    if (rd32(pcap) != 0xA1B2C3D4 || rd32(pcap + 20) != 105) return false;

    size_t off = 24;
    while (off + 16 <= pcap_len) {
        uint32_t incl = rd32(pcap + off + 8);
        off += 16;
        if (incl == 0 || incl > pcap_len - off) return false;
        if (extract_frame(pcap + off, incl, bssid, pmkid)) return true;
        off += incl;
    }
    return false;
}
