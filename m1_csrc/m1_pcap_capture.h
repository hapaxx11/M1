/* See COPYING.txt for license details. */

/*
 * m1_pcap_capture.h
 *
 * SD-card PCAPNG capture session for the WiFi sniffers — the on-target glue
 * that turns the raw 802.11 frames delivered by the ESP32 monitor transport
 * into a Wireshark-openable `capture/sniffNNN.pcapng` file.
 *
 * This is the file-sink half of the MonstaShark-equivalent capture feature: the
 * wire bytes are produced by the pure-logic encoder in wifi_pcapng.c (SHB / IDB
 * / EPB + radiotap), and this module streams them to FatFS.  A session is a
 * single open PCAPNG file:
 *
 *   m1_pcap_session_open(&s);                 // picks the next free filename,
 *                                             // writes the SHB + IDB header
 *   m1_pcap_session_write(&s, frame, len,     // one radiotap EPB per frame
 *                         channel, rssi, ts);
 *   m1_pcap_session_close(&s);                // flush + close
 *
 * Frames longer than the session snap length are captured truncated (the EPB
 * records the true original length, matching libpcap/radiotap semantics).  All
 * writes are best-effort: a failed write marks the session errored and is
 * silently skipped so a full/removed SD card never disrupts the live sniffer.
 *
 * Because the FatFS calls are exercised through the host test stub
 * (tests/stubs/ff.h, which wraps stdio), the whole module — including the
 * on-SD byte layout — is host-tested by tests/test_pcap_capture.c.
 *
 * M1 Project
 */

#ifndef M1_PCAP_CAPTURE_H_
#define M1_PCAP_CAPTURE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ff.h"
#include "wifi_pcapng.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Directory (relative to the SD root) that capture files are written to. */
#define M1_PCAP_DIR        "capture"

/** Default per-frame snap length (bytes of each frame stored). */
#define M1_PCAP_SNAPLEN    512u

/** Highest sniffNNN index probed when picking a free filename. */
#define M1_PCAP_MAX_INDEX  999u

/** Buffer size required to hold a formatted capture path, incl. NUL. */
#define M1_PCAP_PATH_MAX   32u

/** An open PCAPNG capture file. */
typedef struct {
    FIL      fil;                      /**< FatFS file handle                   */
    bool     open;                     /**< true between open() and close()     */
    bool     errored;                  /**< a write failed; stop emitting       */
    uint16_t snaplen;                  /**< per-frame captured-length cap        */
    uint32_t packets;                  /**< EPBs written so far                 */
    uint8_t *scratch;                  /**< EPB assembly buffer (heap)          */
    size_t   scratch_cap;              /**< capacity of @ref scratch            */
    char     path[M1_PCAP_PATH_MAX];   /**< chosen capture file path            */
} m1_pcap_session_t;

/**
 * Format the capture path for a given index into @p out, e.g.
 * "capture/sniff007.pcapng" for index 7.
 *
 * @return number of characters written (excluding NUL), or 0 on bad args /
 *         insufficient capacity / index > M1_PCAP_MAX_INDEX.
 */
size_t m1_pcap_format_name(char *out, size_t cap, unsigned index);

/**
 * Open a capture session on an explicit path, writing the SHB + IDB header.
 *
 * Mostly for tests; m1_pcap_session_open() is the normal entry point.  A
 * snaplen of 0 selects M1_PCAP_SNAPLEN.
 *
 * @return true on success (file open, header written, scratch allocated).
 */
bool m1_pcap_session_open_path(m1_pcap_session_t *s, const char *path,
                               uint16_t snaplen);

/**
 * Open a capture session: create the M1_PCAP_DIR directory, pick the next free
 * `capture/sniffNNN.pcapng`, and write the SHB + IDB header.
 *
 * @return true on success, false if no free filename / open / alloc failed.
 */
bool m1_pcap_session_open(m1_pcap_session_t *s);

/**
 * Append one captured frame as a radiotap-encapsulated Enhanced Packet Block.
 *
 * @param frame     raw 802.11 frame bytes (may be NULL iff @p len == 0)
 * @param len       frame length on the air (captured truncated to snaplen)
 * @param channel   2.4 GHz channel number (for the radiotap Channel field)
 * @param rssi      RSSI in dBm (radiotap antenna-signal field)
 * @param ts_us     capture timestamp in microseconds
 * @return true if the EPB was written; false on a closed/errored session or
 *         a write failure (which also marks the session errored).
 */
bool m1_pcap_session_write(m1_pcap_session_t *s, const uint8_t *frame,
                           uint16_t len, uint8_t channel, int8_t rssi,
                           uint64_t ts_us);

/** Flush and close the session, releasing the scratch buffer. */
void m1_pcap_session_close(m1_pcap_session_t *s);

/** True while the session is open and has not errored. */
bool m1_pcap_session_active(const m1_pcap_session_t *s);

#ifdef __cplusplus
}
#endif

#endif /* M1_PCAP_CAPTURE_H_ */
