/* See COPYING.txt for license details. */

/*
 * m1_esp32_caps.c
 *
 * Runtime capability descriptor for the ESP32-C6 coprocessor.
 *
 * See m1_esp32_caps.h for full documentation.
 *
 * M1 Project
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "FreeRTOS.h"
#include "m1_esp32_caps.h"
#include "m1_esp32_cmd.h"
#include "m1_compile_cfg.h"
#include "m1_display.h"
#include "m1_lcd.h"

/* Forward declarations — implemented in m1_esp32_hal.c and esp_app_main.c */
extern uint8_t m1_esp32_get_init_status(void);
extern bool    get_esp32_main_init_status(void);
extern void    esp32_main_init(void);
extern uint8_t spi_AT_send_recv(const char *at_cmd, char *out_buf,
                                int out_buf_size, int timeout_sec);
extern uint8_t spi_AT_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                    uint8_t *rx_buf, int rx_buf_size,
                                    int *out_len, int timeout_sec);
extern uint8_t spi_m1link_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                        uint8_t *rx_buf, int rx_buf_size,
                                        int *out_len, int timeout_sec);

/*************************** D E F I N E S ************************************/

#define CAPS_QUERY_TIMEOUT_MS  500u   /* Brief: ESP32 is already running */

/* Buffer for the AT+CMD? response.  Stock ESP-AT lists ~150-200 commands at
 * roughly 30-50 bytes per line, so ~8KB is generous enough to capture the
 * full list before spi_AT_send_recv() stops at "OK\r\n".  Allocated from the
 * FreeRTOS heap to avoid stacking ~8KB inside a caller's task stack and to
 * sidestep the newlib heap (which has been observed too small for incidental
 * allocations post-SiN360 integration). */
#define AT_CMD_RESP_BUF_SZ     8192u

/* AT+CMD? probe needs a few seconds for the ESP32 to assemble and stream the
 * full command list.  5 s is the standard "long" timeout used elsewhere in
 * the AT bridge (e.g. spi_AT_send_recv 5 s in m1_http_client / m1_802154). */
#define AT_CMD_PROBE_TIMEOUT_S 5

/* Quick AT presence probe timeout.  A bare "AT\r\n" is answered almost
 * instantly by any AT firmware; 1 s is generous.  CD3 firmware (which speaks
 * M1_RPC, not AT) will not answer, so this short timeout avoids paying the
 * full AT_CMD_PROBE_TIMEOUT_S for CD3 detection. */
#define AT_PRESENCE_TIMEOUT_S  1

/* CD3 native M1_RPC probe (PING/GET_STATUS) timeout.  CD3 is already running
 * and answers immediately over the SPI-HD transport, so a short timeout keeps
 * detection snappy for non-CD3 firmware (which simply never answers). */
#define CD3_RPC_PROBE_TIMEOUT_S 2

/*************************** V A R I A B L E S ********************************/

static uint64_t s_bitmap          = 0u;
static bool     s_queried         = false;
static char     s_fw_name[32]     = "Unknown";
static uint32_t s_bss_bytes       = 0u;
static uint32_t s_free_heap_bytes = 0u;

/* =========================================================================
 * Brain-CD3 firmware version probe
 *
 * The brain's GET_STATUS fw_name is a bare identifier (e.g. "m1-native") with
 * NO dotted version, which qMonstatek's parseVerNums() cannot parse — so the
 * desktop app reports "ESP32 detected — incompatible firmware" even though the
 * link is fully functional.  The brain's actual semver is exposed via the
 * separate M1_RPC SYS_GET_FW_VERSION (0x0003) opcode.  Query it here (once the
 * device is known to be a brain) and fold it into the cached firmware-name
 * string so both the on-device display and the qMonstatek device-info RPC
 * report a parseable version, mirroring the C3 reference firmware's
 * esp_fw_status_str() ("m1_link X.Y.Z <hash>").
 * =========================================================================*/

/**
 * Issue M1_RPC SYS_GET_FW_VERSION to a confirmed brain CD3 device and format a
 * versioned firmware-name string ("<fw_name> X.Y.Z[ <hash>]") into @p out.
 * Returns true on success; false if the firmware does not answer or the reply
 * is malformed (caller keeps the bare fw_name in that case). */
static bool caps_cd3_fetch_fw_version(const char *fw_name,
                                       char *out, size_t out_size)
{
    uint8_t  tx64[32];
    uint8_t  rx64[64];
    uint16_t req_len;
    int      rx_len = 0;
    const uint8_t *rpc_pl  = NULL;
    uint16_t       rpc_plen = 0u;

    if (!out || out_size == 0u)
        return false;

    memset(tx64, 0, sizeof(tx64));
    req_len = m1_esp32_rpc_build_req(tx64, (uint16_t)sizeof(tx64),
                                      M1_ESP32_RPC_SYS_GET_FW_VERSION_CAPS,
                                      NULL, 0u);
    if (req_len == 0u)
        return false;

    memset(rx64, 0, sizeof(rx64));
    if (spi_m1link_send_recv_bin(tx64, (int)req_len,
                                 rx64, (int)sizeof(rx64),
                                 &rx_len, CD3_RPC_PROBE_TIMEOUT_S) != 0 ||
        rx_len <= 0 ||
        !m1_esp32_rpc_parse_resp(rx64, (uint16_t)rx_len,
                                  M1_ESP32_RPC_SYS_GET_FW_VERSION_CAPS,
                                  &rpc_pl, &rpc_plen) ||
        rpc_plen < (uint16_t)sizeof(m1_esp32_rpc_fw_version_t))
        return false;

    m1_esp32_rpc_fw_version_t ver;
    memcpy(&ver, rpc_pl, sizeof(ver));
    ver.git_hash[sizeof(ver.git_hash) - 1] = '\0';
    m1_esp32_rpc_format_fw_version(out, out_size, fw_name, &ver);
    return out[0] != '\0';
}

/* =========================================================================
 * AT command → capability bit mapping table
 *
 * Consulted by m1_esp32_caps_parse_at_cmd_list() against the response of a
 * stock ESP-AT `AT+CMD?` probe.  Each entry maps the full AT command name
 * (including the "AT+" prefix) to the M1_ESP32_CAP_* bit that should be set
 * when the connected ESP32 firmware advertises that command.
 *
 * Adding support for a new AT-side feature is a single-line edit to this
 * table — no curated fallback profile macros are required.
 *
 * Currently mapped commands:
 *   - "AT+CWJAP"        — stock ESP-AT, WiFi station join → CAP_WIFI_JOIN
 *   - "AT+BLEHIDINIT"   — stock ESP-AT (BLE HID example)  → CAP_BLE_HID
 *   - "AT+ZIGSNIFF"     — CD3-AT/dag/neddy299 custom      → CAP_802154
 *   - "AT+DEAUTH"       — dag/neddy299 at_custom_deauth    → CAP_DEAUTH
 *   - "AT+STASCAN"      — neddy299 at_custom_stascan       → CAP_STA_SCAN
 *   - "AT+M1DEAUTH"     — dag T-800 at_custom_wifi_cmd     → CAP_DEAUTH
 *   - "AT+M1DEAUTHALL"  — dag T-800 at_custom_wifi_cmd     → CAP_DEAUTH
 *   - "AT+M1DEAUTHSTOP" — dag T-800 at_custom_wifi_cmd     → CAP_DEAUTH
 *   - "AT+M1BEACON"     — dag T-800 at_custom_wifi_cmd     → CAP_BEACON
 *   - "AT+M1KARMA"      — dag T-800 at_custom_wifi_cmd     → CAP_KARMA
 *   - "AT+M1EVILTWIN"   — dag T-800 at_custom_wifi_cmd     → CAP_PORTAL
 *   - "AT+M1BLESPAM"    — dag T-800 at_custom_wifi_cmd     → CAP_BLE_ADV
 *   - "AT+M1MONITOR"    — dag T-800 at_custom_wifi_cmd     → CAP_PKTMON
 *   - "AT+M1PROBE"      — dag T-800 at_custom_wifi_cmd     → CAP_PROBE_FLOOD
 *   - "AT+M1PMKID"      — dag T-800 at_custom_wifi_cmd     → CAP_PKTMON
 *   - "AT+M1HSCAP"      — dag T-800 at_custom_wifi_cmd     → CAP_PKTMON
 *   - "AT+M1WIFISTATS"  — dag T-800 at_custom_wifi_cmd     → CAP_WIFI_SCAN
 *   - "AT+HIDKBINIT"    — dag T-800 at_custom_hid_cmd      → CAP_BLE_HID
 *   - "AT+HIDKBSEND"    — dag T-800 at_custom_hid_cmd      → CAP_BLE_HID
 * =========================================================================*/
static const m1_esp32_at_cmd_cap_entry_t s_at_cmd_cap_map[] = {
    /* Stock ESP-AT */
    { "AT+CWJAP",        M1_ESP32_CAP_WIFI_JOIN  },
    { "AT+BLEHIDINIT",   M1_ESP32_CAP_BLE_HID    },
    /* CD3-AT (bedge117) / neddy299 custom commands */
    { "AT+ZIGSNIFF",     M1_ESP32_CAP_802154     },
    { "AT+DEAUTH",       M1_ESP32_CAP_DEAUTH     },
    { "AT+STASCAN",      M1_ESP32_CAP_STA_SCAN   },
    /* dag T-800 custom WiFi commands (at_custom_wifi_cmd.c) */
    { "AT+M1DEAUTH",     M1_ESP32_CAP_DEAUTH     },
    { "AT+M1DEAUTHALL",  M1_ESP32_CAP_DEAUTH     },
    { "AT+M1DEAUTHSTOP", M1_ESP32_CAP_DEAUTH     },
    { "AT+M1BEACON",     M1_ESP32_CAP_BEACON     },
    { "AT+M1KARMA",      M1_ESP32_CAP_KARMA      },
    { "AT+M1EVILTWIN",   M1_ESP32_CAP_PORTAL     },
    { "AT+M1BLESPAM",    M1_ESP32_CAP_BLE_ADV    },
    { "AT+M1MONITOR",    M1_ESP32_CAP_PKTMON     },
    { "AT+M1PROBE",      M1_ESP32_CAP_PROBE_FLOOD},
    { "AT+M1PMKID",      M1_ESP32_CAP_PKTMON     },
    { "AT+M1HSCAP",      M1_ESP32_CAP_PKTMON     },
    { "AT+M1WIFISTATS",  M1_ESP32_CAP_WIFI_SCAN  },
    /* dag T-800 custom HID commands (at_custom_hid_cmd.c) */
    { "AT+HIDKBINIT",    M1_ESP32_CAP_BLE_HID    },
    { "AT+HIDKBSEND",    M1_ESP32_CAP_BLE_HID    },
};

#define S_AT_CMD_CAP_MAP_N \
    (sizeof(s_at_cmd_cap_map) / sizeof(s_at_cmd_cap_map[0]))

/*************** I N T E R N A L   H E L P E R S *****************************/

/**
 * Set memory footprint estimates based on the resolved capability bitmap.
 * Four-way discriminator (evaluated in priority order):
 *   HANDSHAKE + OTA present → CD3 native (bedge117/m1-esp32-brain) — in
 *                             practice no shipped release currently sets
 *                             either bit; see m1_esp32_caps.h CAP_OTA/
 *                             CAP_HANDSHAKE comments
 *   WIFI_JOIN + BEACON      → dag T-800 (AT firmware with custom cmds)
 *   WIFI_JOIN alone         → CD3-AT (bedge117/neddy299)
 *   neither WIFI_JOIN       → SiN360 binary-SPI
 */
static void caps_apply_footprint_estimates(uint64_t bitmap)
{
    if ((bitmap & M1_ESP32_CAP_HANDSHAKE) &&
        (bitmap & (M1_ESP32_CAP_802154_TX | M1_ESP32_CAP_BLE_SPAM)))
    {
        /* CD3 native binary RPC "brain" firmware (m1-esp32-brain).  Mirrors the
         * esp32_firmware_is_cd3() discriminator (HANDSHAKE + a CD3-unique bit),
         * NOT the old HANDSHAKE+OTA pair — the shipped brain omits OTA. */
        s_bss_bytes       = M1_ESP32_FALLBACK_BSS_CD3;
        s_free_heap_bytes = M1_ESP32_FALLBACK_HEAP_CD3;
    }
    else if ((bitmap & M1_ESP32_CAP_WIFI_JOIN) &&
             (bitmap & M1_ESP32_CAP_BEACON))
    {
        /* dag T-800: AT firmware with custom WiFi/HID/Zigbee commands */
        s_bss_bytes       = M1_ESP32_FALLBACK_BSS_T800;
        s_free_heap_bytes = M1_ESP32_FALLBACK_HEAP_T800;
    }
    else if (bitmap & M1_ESP32_CAP_WIFI_JOIN)
    {
        /* CD3-AT (bedge117 base) / neddy299 */
        s_bss_bytes       = M1_ESP32_FALLBACK_BSS_AT;
        s_free_heap_bytes = M1_ESP32_FALLBACK_HEAP_AT;
    }
    else
    {
        /* SiN360 binary SPI firmware */
        s_bss_bytes       = M1_ESP32_FALLBACK_BSS_SIN360;
        s_free_heap_bytes = M1_ESP32_FALLBACK_HEAP_SIN360;
    }
}

/*************** P U B L I C   A P I ******************************************/

void m1_esp32_caps_init(void)
{
    m1_resp_t resp;
    int       ret;
    uint64_t  bitmap        = 0u;
    char      fw_name[32]   = {0};

    if (s_queried)
        return;  /* Already cached from a previous call */

    /* Require the SPI HAL transport to be active before sending any binary
     * commands.  If the ESP32 has not been initialised yet (or was
     * deinitialized), return without caching so the next call retries once
     * the transport is ready.  Probing an uninitialised transport would time
     * out and cache the fallback, potentially mis-attributing capabilities. */
    if (!m1_esp32_get_init_status())
        return;

    /* Probe 0: binary CMD_PING (0x01) — SiN360 binary-SPI firmware detection.
     * CMD_GET_STATUS (0x02) was proposed as a capability-reporting command
     * but never fully implemented in SiN360 firmware.  The actual SiN360
     * cmd_get_status() implementation (sincere360/M1_SiN360_ESP32) returns
     * only a 5-byte version payload, not the 41-byte capability report that
     * m1_esp32_caps_parse_payload() expects.  Because SiN360 has no AT task,
     * the AT+CMD? probe also fails, leaving SiN360 misdetected as "Unknown".
     *
     * CMD_PING is universally implemented by all binary-SPI firmware and
     * returns a simple "PONG" (4 bytes) payload.  If CMD_PING succeeds, we
     * know the ESP32 speaks the binary SPI protocol.  We record that fact
     * but do NOT return here — Probe 1 (CMD_GET_STATUS) still runs so that
     * a future firmware implementing the full capability-reporting protocol
     * is detected via Probe 1 rather than defaulting to the SiN360 profile. */
    ret = m1_esp32_simple_cmd(CMD_PING, &resp, CAPS_QUERY_TIMEOUT_MS);

    bool is_binary_spi = (ret == 0 && resp.status == RESP_OK &&
                          resp.payload_len == 4 &&
                          resp.payload[0] == 'P' && resp.payload[1] == 'O' &&
                          resp.payload[2] == 'N' && resp.payload[3] == 'G');

    /* Probe 1: binary CMD_GET_STATUS (0x02).  AT firmware that implements
     * the binary extension (e.g. hapaxx11/esp32-at-monstatek-m1) would
     * respond here with a full capability report.  Unextended AT firmware
     * (bedge117, dag) will return RESP_ERR or time out.  Current SiN360
     * firmware (v0.9.1.0) returns only a 5-byte version payload, which
     * fails the parse check below. */
    ret = m1_esp32_simple_cmd(CMD_GET_STATUS, &resp, CAPS_QUERY_TIMEOUT_MS);

    if (ret == 0 && resp.status == RESP_OK &&
        m1_esp32_caps_parse_payload(resp.payload, resp.payload_len,
                                    &bitmap, fw_name))
    {
        s_bitmap = m1_esp32_caps_with_cd3_host_bits(bitmap);
        strncpy(s_fw_name, fw_name, sizeof(s_fw_name) - 1);
        s_fw_name[sizeof(s_fw_name) - 1] = '\0';
        caps_apply_footprint_estimates(s_bitmap);
        s_queried = true;
        return;
    }

    if (is_binary_spi)
    {
        /* PING succeeded (binary-SPI firmware confirmed) but CMD_GET_STATUS
         * did not provide a full capability report.  This is the current
         * SiN360 case — use the SiN360 fallback profile. */
        s_bitmap = M1_ESP32_CAP_PROFILE_SIN360;
        strncpy(s_fw_name, "SiN360 (via PING)", sizeof(s_fw_name) - 1);
        s_fw_name[sizeof(s_fw_name) - 1] = '\0';
        caps_apply_footprint_estimates(s_bitmap);
        s_queried = true;
        return;
    }

    /* From here on the firmware is not SiN360 binary-SPI.  It is either an
     * AT firmware (stock ESP-AT, bedge117/dag/neddy299 variants) or the CD3
     * native binary-RPC firmware (bedge117/m1-esp32-brain).  BOTH speak over
     * the same half-duplex SPI-HD transport (spi_slave_hd: command/address/
     * dummy phases + WRBUF/RDDMA + HANDSHAKE) driven by the AT RTOS task, so
     * the task must be running before either the AT+CMD? text probe or the
     * M1_RPC binary probe below can exchange a frame.
     *
     * The AT task is not started by the DELEGATE_FEATURE-style capability
     * gates that call us (they only bring up the SPI HAL via
     * m1_esp32_ensure_init()); the AT task is otherwise only started deep
     * inside each feature function *after* the gate has been evaluated.  Left
     * unaddressed, this permanently fails detection ("Unknown" firmware, cap
     * bits never set) even for fully-supported firmware.  Start the AT task
     * here, mirroring wifi_do_scan() (m1_wifi.c).  If the task still is not up
     * right after trying (e.g. heap pressure), return without caching so the
     * next call retries.
     *
     * SiN360 binary-SPI firmware has no AT task and was already handled above
     * via the is_binary_spi early return, so it never reaches this point. */
    {
        bool at_task_before = get_esp32_main_init_status();
        bool at_task_after  = at_task_before;

        if (!at_task_before)
        {
            esp32_main_init();
            at_task_after = get_esp32_main_init_status();
        }

        if (!m1_esp32_caps_should_run_at_probe(at_task_before, at_task_after))
            return;
    }

    /* Probe 2a: quick AT presence check.  Send a bare "AT\r\n" with a short
     * timeout — any AT firmware answers "OK" almost immediately.  CD3 native
     * firmware (which speaks M1_RPC, not AT) will not reply, so this times
     * out in AT_PRESENCE_TIMEOUT_S (1 s) instead of paying the full
     * AT_CMD_PROBE_TIMEOUT_S (5 s) for the heavyweight AT+CMD? probe.
     *
     * If the presence probe fails, skip directly to the CD3 binary probe
     * (Probe 3) — there is no point waiting 5 s for AT+CMD? when the
     * firmware does not even answer a bare AT. */
    {
        char at_presence[64];
        at_presence[0] = '\0';
        (void)spi_AT_send_recv("AT\r\n", at_presence,
                               (int)sizeof(at_presence),
                               AT_PRESENCE_TIMEOUT_S);

        if (strstr(at_presence, "OK") == NULL)
            goto probe_cd3;  /* Not AT firmware — skip to CD3 probe */
    }

    /* Probe 2b: stock ESP-AT `AT+CMD?`.  This command is part of the basic
     * ESP-AT command set and is supported by all tracked AT firmware variants
     * (bedge117, dag, neddy299) without requiring any custom extension.  The
     * response lists every AT command the firmware understands; we OR in
     * capability bits for each command our mapping table recognises.
     *
     * We only reach here if the quick AT presence probe above succeeded, so
     * this is guaranteed to be an AT firmware and will respond promptly.
     *
     * The response can be several KB, so the buffer is allocated from the
     * FreeRTOS heap rather than the caller's stack.  If the heap is
     * exhausted, return without caching so the next call retries. */
    {
        char *at_resp = (char *)pvPortMalloc(AT_CMD_RESP_BUF_SZ);
        if (!at_resp)
            return;  /* Heap exhausted — retry on next call */

        at_resp[0] = '\0';
        (void)spi_AT_send_recv("AT+CMD?\r\n", at_resp,
                               (int)AT_CMD_RESP_BUF_SZ,
                               AT_CMD_PROBE_TIMEOUT_S);

        if (m1_esp32_caps_at_cmd_response_valid(at_resp))
        {
            /* Probe succeeded — OR in every tracked AT command the
             * firmware advertised.  A response that contains "+CMD:"
             * lines but none of our tracked names is still a successful
             * probe; the bitmap simply reflects that the firmware has
             * no features we currently recognise. */
            s_bitmap = m1_esp32_caps_parse_at_cmd_list(
                at_resp, s_at_cmd_cap_map, S_AT_CMD_CAP_MAP_N);
            strncpy(s_fw_name, "AT (probed)", sizeof(s_fw_name) - 1);
            s_fw_name[sizeof(s_fw_name) - 1] = '\0';
            caps_apply_footprint_estimates(s_bitmap);
            s_queried = true;
            vPortFree(at_resp);
            return;
        }

        vPortFree(at_resp);
    }

    /* Probe 3: CD3 native "brain" binary RPC firmware (hapaxx11/m1-esp32-brain)
     * via M1_RPC PING (magic 0x4D31 "M1").
     *
     * The brain CD3 is an ESP-IDF full-duplex `spi_slave` device: every
     * transaction clocks EXACTLY 512 bytes in both directions and the reply is
     * pipelined onto a later transaction.  This is fundamentally different from
     * the ESP-AT half-duplex `spi_slave_hd` command protocol — sending AT-style
     * HD frames (spi_AT_send_recv_bin) to a full-duplex slave yields no answer,
     * which is why CD3 detection and every brain feature failed.  The
     * request/response MUST therefore go through spi_m1link_send_recv_bin(),
     * the 512-byte full-duplex "M1 Link" transport (see esp_app_main.c), which
     * builds the padded frame, honours the HANDSHAKE line, and reassembles the
     * pipelined reply.
     *
     * This probe runs only after the AT presence probe has failed (or AT+CMD?
     * did not yield a valid response), so a working AT firmware (already
     * detected above) is never driven with the full-duplex transport.
     *
     * On success we immediately follow up with M1_RPC GET_STATUS to retrieve
     * the capability bitmap.  If GET_STATUS fails after PING succeeds (early-
     * stage firmware not yet implementing GET_STATUS), we fall back to the
     * CD3 conservative profile macro. */
probe_cd3:
    {
        uint8_t  tx64[64];
        uint8_t  rx64[64];
        static const uint8_t cd3_cookie[4] = {0x4D, 0x31, 0x50, 0x49}; /* "M1PI" */
        uint16_t req_len;
        int      rx_len = 0;
        const uint8_t *rpc_pl  = NULL;
        uint16_t       rpc_plen = 0u;

        memset(tx64, 0, sizeof(tx64));
        req_len = m1_esp32_rpc_build_req(tx64, (uint16_t)sizeof(tx64),
                                          M1_ESP32_RPC_SYS_PING,
                                          cd3_cookie, 4u);
        if (req_len > 0u)
        {
            memset(rx64, 0, sizeof(rx64));
            rx_len = 0;

            if (spi_m1link_send_recv_bin(tx64, (int)req_len,
                                     rx64, (int)sizeof(rx64),
                                     &rx_len, CD3_RPC_PROBE_TIMEOUT_S) == 0 &&
                rx_len > 0 &&
                m1_esp32_rpc_parse_resp(rx64, (uint16_t)rx_len,
                                         M1_ESP32_RPC_SYS_PING,
                                         &rpc_pl, &rpc_plen))
            {
                /* CD3 confirmed — issue GET_STATUS to retrieve capabilities */
                memset(tx64, 0, sizeof(tx64));
                req_len = m1_esp32_rpc_build_req(tx64, (uint16_t)sizeof(tx64),
                                                  M1_ESP32_RPC_SYS_GET_STATUS,
                                                  NULL, 0u);
                if (req_len > 0u)
                {
                    memset(rx64, 0, sizeof(rx64));
                    rx_len   = 0;
                    rpc_pl   = NULL;
                    rpc_plen = 0u;
                    if (spi_m1link_send_recv_bin(tx64, (int)req_len,
                                             rx64, (int)sizeof(rx64),
                                             &rx_len, CD3_RPC_PROBE_TIMEOUT_S) == 0 &&
                        rx_len > 0 &&
                        m1_esp32_rpc_parse_resp(rx64, (uint16_t)rx_len,
                                                 M1_ESP32_RPC_SYS_GET_STATUS,
                                                 &rpc_pl, &rpc_plen) &&
                        rpc_plen >= (uint16_t)sizeof(m1_esp32_rpc_devstatus_t) &&
                        m1_esp32_caps_parse_payload(rpc_pl,
                                                     (uint8_t)rpc_plen,
                                                     &bitmap, fw_name))
                    {
                        s_bitmap = m1_esp32_caps_with_cd3_host_bits(bitmap);
                        /* fw_name is a bare identifier (e.g. "m1-native") with
                         * no dotted version.  Fold in the GET_FW_VERSION semver
                         * so qMonstatek's parseVerNums() recognises the brain
                         * as compatible; fall back to the bare name when the
                         * firmware does not answer the version query. */
                        char versioned[32];
                        if (caps_cd3_fetch_fw_version(fw_name, versioned,
                                                      sizeof(versioned)))
                        {
                            strncpy(s_fw_name, versioned,
                                    sizeof(s_fw_name) - 1);
                        }
                        else
                        {
                            strncpy(s_fw_name, fw_name,
                                    sizeof(s_fw_name) - 1);
                        }
                        s_fw_name[sizeof(s_fw_name) - 1] = '\0';
                        caps_apply_footprint_estimates(s_bitmap);
                        s_queried = true;
                        return;
                    }
                }

                /* PING confirmed CD3 but GET_STATUS is not yet implemented —
                 * use the conservative CD3 profile macro.  Still try to attach
                 * the real semver so the reported name is parseable. */
                s_bitmap = M1_ESP32_CAP_PROFILE_CD3;
                {
                    char versioned[32];
                    if (caps_cd3_fetch_fw_version("CD3", versioned,
                                                  sizeof(versioned)))
                    {
                        strncpy(s_fw_name, versioned, sizeof(s_fw_name) - 1);
                    }
                    else
                    {
                        strncpy(s_fw_name, "CD3 (via M1_RPC)",
                                sizeof(s_fw_name) - 1);
                    }
                    s_fw_name[sizeof(s_fw_name) - 1] = '\0';
                }
                caps_apply_footprint_estimates(s_bitmap);
                s_queried = true;
                return;
            }
        }
    }

    /* All probes failed — fail closed.  Feature gates that check specific
     * M1_ESP32_CAP_* bits will all return false and the "Feature not
     * supported" UI will appear.  This is intentional: granting capabilities
     * we cannot verify risks crashing on firmware that does not implement
     * the underlying command. */
    s_bitmap = 0u;
    strncpy(s_fw_name, "Unknown (fallback)", sizeof(s_fw_name) - 1);
    s_fw_name[sizeof(s_fw_name) - 1] = '\0';
    caps_apply_footprint_estimates(0u);
    s_queried = true;
}

void m1_esp32_caps_reset(void)
{
    s_bitmap          = 0u;
    s_bss_bytes       = 0u;
    s_free_heap_bytes = 0u;
    s_queried         = false;
    s_fw_name[0]      = '\0';
}

bool m1_esp32_has_cap(uint64_t cap)
{
    if (!s_queried)
    {
        /* Don't probe when the ESP32 HAL has not been initialised.  Probing
         * an uninitialised transport would time out and potentially cache a
         * fail-closed result that prevents legitimate capabilities later. */
        if (!m1_esp32_get_init_status())
            return false;
        m1_esp32_caps_init();
    }
    return (s_bitmap & cap) == cap;
}

const char *m1_esp32_caps_fw_name(void)
{
    if (!s_queried)
    {
        /* Return "offline" immediately — without probing or caching —
         * when the ESP32 HAL transport is not active.  This keeps the
         * RPC device-info response accurate for disconnected devices. */
        if (!m1_esp32_get_init_status())
            return "offline";
        m1_esp32_caps_init();
    }
    return s_fw_name[0] != '\0' ? s_fw_name : "Unknown";
}

bool m1_esp32_require_cap(uint64_t cap, const char *feature_name)
{
    char fw_line[22];  /* fits on 128px display with main-menu font */

    if (m1_esp32_has_cap(cap))
        return true;

    /* Draw the "not supported" screen and hold for 2 seconds so the user
     * can read it before the delegate pops the scene. */
    snprintf(fw_line, sizeof(fw_line), "%.21s", m1_esp32_caps_fw_name());

    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);

    m1_u8g2_firstpage();
    do
    {
        /* Title */
        u8g2_DrawStr(&m1_u8g2, 0, 10, feature_name);
        u8g2_DrawHLine(&m1_u8g2, 0, 11, 128);

        /* Body */
        u8g2_DrawStr(&m1_u8g2, 0, 25, M1_ESP32_UNSUPPORTED_LINE_1);
        u8g2_DrawStr(&m1_u8g2, 0, 37, fw_line);
        u8g2_DrawStr(&m1_u8g2, 0, 52, M1_ESP32_UNSUPPORTED_LINE_2);
        u8g2_DrawStr(&m1_u8g2, 0, 62, M1_ESP32_UNSUPPORTED_LINE_3);
    }
    while (u8g2_NextPage(&m1_u8g2));

    HAL_Delay(2000);

    return false;
}

uint32_t m1_esp32_caps_bss_bytes(void)
{
    if (!s_queried && m1_esp32_get_init_status())
        m1_esp32_caps_init();
    return s_bss_bytes;
}

uint32_t m1_esp32_caps_free_heap(void)
{
    if (!s_queried && m1_esp32_get_init_status())
        m1_esp32_caps_init();
    return s_free_heap_bytes;
}

uint64_t m1_esp32_caps_get_bitmap(void)
{
    /* Return the cached bitmap without re-probing.  Callers that need the
     * bitmap for esp32_feature_map queries should ensure m1_esp32_caps_init()
     * has already run (e.g. via m1_esp32_ensure_init()). */
    return s_bitmap;
}

bool m1_esp32_caps_is_queried(void)
{
    return s_queried;
}
