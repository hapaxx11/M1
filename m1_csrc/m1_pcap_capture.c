/* See COPYING.txt for license details. */

/*
 * m1_pcap_capture.c
 *
 * SD-card PCAPNG capture session (see m1_pcap_capture.h).  Streams the
 * pure-logic PCAPNG blocks produced by wifi_pcapng.c to a FatFS file, one
 * radiotap Enhanced Packet Block per captured 802.11 frame.
 *
 * M1 Project
 */

#include "m1_pcap_capture.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- helpers ----------------------------------------------------------- */

/* Append @p len bytes to the open session file; returns true iff all written. */
static bool pcap_write_all(m1_pcap_session_t *s, const uint8_t *buf, size_t len)
{
    UINT bw = 0u;
    if (f_write(&s->fil, buf, (UINT)len, &bw) != FR_OK || (size_t)bw != len)
        return false;
    return true;
}

size_t m1_pcap_format_name(char *out, size_t cap, unsigned index)
{
    if (!out || index > M1_PCAP_MAX_INDEX)
        return 0u;
    /* "capture/sniffNNN.pcapng" = 7 + 1 + 5 + 3 + 7 = 23 chars + NUL. */
    int n = snprintf(out, cap, "%s/sniff%03u.pcapng", M1_PCAP_DIR, index);
    if (n <= 0 || (size_t)n >= cap)
        return 0u;
    return (size_t)n;
}

bool m1_pcap_session_open_path(m1_pcap_session_t *s, const char *path,
                               uint16_t snaplen)
{
    if (!s || !path)
        return false;

    memset(s, 0, sizeof(*s));
    s->snaplen = snaplen ? snaplen : (uint16_t)M1_PCAP_SNAPLEN;

    /* EPB = 32 B overhead + radiotap header + snaplen frame bytes + <=3 pad. */
    s->scratch_cap = (size_t)WIFI_PCAPNG_EPB_OVERHEAD + WIFI_RADIOTAP_LEN +
                     s->snaplen + 4u;
    s->scratch = (uint8_t *)malloc(s->scratch_cap);
    if (!s->scratch)
        return false;

    if (f_open(&s->fil, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        free(s->scratch);
        s->scratch = NULL;
        return false;
    }
    s->open = true;

    /* Section Header Block followed by one radiotap Interface Description. */
    uint8_t hdr[WIFI_PCAPNG_SHB_LEN + WIFI_PCAPNG_IDB_LEN];
    size_t off = wifi_pcapng_build_shb(hdr, sizeof(hdr));
    off += wifi_pcapng_build_idb(hdr + off, sizeof(hdr) - off, s->snaplen);
    if (off != sizeof(hdr) || !pcap_write_all(s, hdr, off)) {
        m1_pcap_session_close(s);
        return false;
    }

    (void)snprintf(s->path, sizeof(s->path), "%s", path);
    return true;
}

bool m1_pcap_session_open(m1_pcap_session_t *s)
{
    if (!s)
        return false;

    (void)f_mkdir(M1_PCAP_DIR);

    char path[M1_PCAP_PATH_MAX];
    for (unsigned i = 0u; i <= M1_PCAP_MAX_INDEX; i++) {
        if (m1_pcap_format_name(path, sizeof(path), i) == 0u)
            return false;
        FILINFO fno;
        if (f_stat(path, &fno) != FR_OK)        /* first free index */
            return m1_pcap_session_open_path(s, path, (uint16_t)M1_PCAP_SNAPLEN);
    }
    return false;                                /* all indices in use */
}

bool m1_pcap_session_write(m1_pcap_session_t *s, const uint8_t *frame,
                           uint16_t len, uint8_t channel, int8_t rssi,
                           uint64_t ts_us)
{
    if (!s || !s->open || s->errored || !s->scratch)
        return false;
    if (len > 0u && !frame)
        return false;

    /* Capture truncated to snaplen; the EPB keeps the true original length. */
    uint16_t caplen = (len < s->snaplen) ? len : s->snaplen;

    size_t epb = wifi_pcapng_build_epb_radiotap(s->scratch, s->scratch_cap,
                                                frame, caplen, len,
                                                channel, rssi, ts_us);
    if (epb == 0u) {
        s->errored = true;
        return false;
    }
    if (!pcap_write_all(s, s->scratch, epb)) {
        s->errored = true;
        return false;
    }
    s->packets++;
    return true;
}

void m1_pcap_session_close(m1_pcap_session_t *s)
{
    if (!s)
        return;
    if (s->open) {
        (void)f_close(&s->fil);
        s->open = false;
    }
    if (s->scratch) {
        free(s->scratch);
        s->scratch = NULL;
    }
}

bool m1_pcap_session_active(const m1_pcap_session_t *s)
{
    return s && s->open && !s->errored;
}
