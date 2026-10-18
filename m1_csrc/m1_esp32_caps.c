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
#include "m1_esp32_caps.h"
#include "m1_esp32_cmd.h"
#include "m1_esp32_rpc.h"
#include "m1_compile_cfg.h"
#include "m1_display.h"
#include "m1_lcd.h"

/* Forward declarations — implemented in m1_esp32_hal.c and esp_app_main.c */
extern uint8_t m1_esp32_get_init_status(void);
extern uint8_t spi_m1link_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                        uint8_t *rx_buf, int rx_buf_size,
                                        int *out_len, int timeout_sec);

/*************************** D E F I N E S ************************************/

#define CAPS_QUERY_TIMEOUT_MS  500u   /* Brief: ESP32 is already running */

/* CD3 native M1_RPC probe (PING/GET_STATUS) timeout.  CD3 is already running
 * and answers immediately over M1 Link, so a short timeout keeps
 * detection snappy for non-CD3 firmware (which simply never answers). */
#define CD3_RPC_PROBE_TIMEOUT_S 2

/*************************** V A R I A B L E S ********************************/

static uint64_t s_bitmap          = 0u;
static bool     s_queried         = false;
static char     s_fw_name[32]     = "Unknown";
static uint32_t s_bss_bytes       = 0u;
static uint32_t s_free_heap_bytes = 0u;
static uint32_t s_last_probe_tick = 0u;

/* Phase 0 (issue #719): read-only snapshot of the last probe sequence,
 * surfaced on the Settings > Dashboard so the "Unknown (fallback)" failure is
 * observable on-device without a debugger.  Populated by m1_esp32_caps_init(),
 * copied out by m1_esp32_caps_get_diag().  Diagnostic only — no feature gate
 * reads it. */
static m1_esp32_caps_diag_t s_diag = { 0 };

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

/*************** I N T E R N A L   H E L P E R S *****************************/

/**
 * Set measured/estimated footprint values for supported profiles. MtkCore
 * currently has no estimate; unknown profiles are left at zero.
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
    else if (bitmap & M1_ESP32_CAP_MTKCORE)
    {
        s_bss_bytes       = 0u;
        s_free_heap_bytes = 0u;
    }
    else
    {
        /* SiN360 binary SPI firmware */
        s_bss_bytes       = M1_ESP32_FALLBACK_BSS_SIN360;
        s_free_heap_bytes = M1_ESP32_FALLBACK_HEAP_SIN360;
    }
}

/*************** P U B L I C   A P I ******************************************/

static void m1_esp32_caps_init_locked(void)
{
    m1_resp_t resp;
    int       ret;
    uint64_t  bitmap        = 0u;
    char      fw_name[32]   = {0};

    if (s_queried)
        return;  /* Already cached from a previous call */

    /* Phase 0 diagnostics (issue #719): start a fresh snapshot of this probe
     * sequence.  Fields are filled in as each probe runs and copied out by
     * m1_esp32_caps_get_diag() for the on-device dashboard. */
    memset(&s_diag, 0, sizeof(s_diag));
    s_diag.outcome        = (uint8_t)M1_ESP32_PROBE_NONE;
    s_diag.rpc_ping_rc    = 0;
    s_diag.rpc_ping_rxlen = 0;

    /* Require the SPI HAL transport to be active before sending any binary
     * commands.  If the ESP32 has not been initialised yet (or was
     * deinitialized), return without caching so the next call retries once
     * the transport is ready.  Probing an uninitialised transport would time
     * out and cache the fallback, potentially mis-attributing capabilities. */
    if (!m1_esp32_get_init_status())
    {
        s_diag.outcome = (uint8_t)M1_ESP32_PROBE_HAL_OFF;
        return;
    }
    s_diag.hal_ready = 1u;

    /* Probe 0: binary CMD_PING (0x01) — SiN360 binary-SPI firmware detection.
     * SiN360 does not implement the full capability-report payload, so a valid
     * PONG identifies it and selects its known profile. */
    ret = m1_esp32_simple_cmd(CMD_PING, &resp, CAPS_QUERY_TIMEOUT_MS);

    bool is_binary_spi = (ret == 0 && resp.status == RESP_OK &&
                          resp.payload_len == 4 &&
                          resp.payload[0] == 'P' && resp.payload[1] == 'O' &&
                          resp.payload[2] == 'N' && resp.payload[3] == 'G');
    s_diag.bin_ping_ok = is_binary_spi ? 1u : 0u;

    if (is_binary_spi)
    {
        /* PING succeeded (binary-SPI firmware confirmed) — use the SiN360
         * profile. */
        s_bitmap = M1_ESP32_CAP_PROFILE_SIN360;
        strncpy(s_fw_name, "SiN360 (via PING)", sizeof(s_fw_name) - 1);
        s_fw_name[sizeof(s_fw_name) - 1] = '\0';
        caps_apply_footprint_estimates(s_bitmap);
        s_diag.outcome = (uint8_t)M1_ESP32_PROBE_BIN_PING;
        s_diag.bitmap  = s_bitmap;
        s_queried = true;
        return;
    }

    /* Probe 2: CD3 native "brain" binary RPC firmware (hapaxx11/m1-esp32-brain)
     * via M1_RPC PING (magic 0x4D31 "M1").
     *
     * Native CD3 is an ESP-IDF full-duplex `spi_slave` device: every
     * transaction clocks EXACTLY 512 bytes in both directions and the reply is
     * pipelined onto a later transaction.  This is fundamentally different from
     * half-duplex `spi_slave_hd` command protocol — sending legacy half-duplex
     * frames to a full-duplex slave yields no answer. The request/response MUST
     * therefore go through spi_m1link_send_recv_bin(),
     * the 512-byte full-duplex "M1 Link" transport (see esp_app_main.c), which
     * builds the padded frame, honours the HANDSHAKE line, and reassembles the
     * pipelined reply.
     *
     * This probe uses the shared SPI transaction lock so the full-duplex
     * transfer cannot race another user of the peripheral.
     *
     * On success we immediately follow up with M1_RPC GET_STATUS to retrieve
     * the capability bitmap.  If GET_STATUS fails after PING succeeds (early-
     * stage firmware not yet implementing GET_STATUS), we fall back to the
     * CD3 conservative profile macro.  If the PING itself fails, we fall
     * through below to the fail-closed unknown state. */
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
            uint8_t ping_rc;

            memset(rx64, 0, sizeof(rx64));
            rx_len = 0;

            s_diag.rpc_attempted = 1u;
            ping_rc = spi_m1link_send_recv_bin(tx64, (int)req_len,
                                     rx64, (int)sizeof(rx64),
                                     &rx_len, CD3_RPC_PROBE_TIMEOUT_S);
            s_diag.rpc_ping_rc    = (int16_t)ping_rc;
            s_diag.rpc_ping_rxlen = (int16_t)rx_len;

            if (ping_rc == 0 &&
                rx_len > 0 &&
                m1_esp32_rpc_parse_resp(rx64, (uint16_t)rx_len,
                                         M1_ESP32_RPC_SYS_PING,
                                         &rpc_pl, &rpc_plen))
            {
                s_diag.rpc_ping_ok = 1u;
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
                        /* MtkCore (Monstatek/MonstaTek-Esp32-Core) Legacy-SPI
                         * firmware answers PING + GET_STATUS over the same
                         * m1_link wire protocol as brain-CD3, but its compat
                         * adapter hard-codes the capability bitmap to all zeros
                         * and reports the release string (e.g. "0.8.1.0") in
                         * fw_name.  Detect that signature and apply the fixed
                         * MtkCore profile instead of the CD3 host-bits path —
                         * otherwise the zero bitmap would leave every
                         * feature gated off. See M1_ESP32_CAP_PROFILE_MTKCORE. */
                        if (m1_esp32_caps_is_mtkcore_devstatus(bitmap, fw_name))
                        {
                            s_bitmap = M1_ESP32_CAP_PROFILE_MTKCORE;
                            /* fw_name is already a parseable dotted semver;
                             * prefix the product name for the device-info /
                             * dual-boot display while keeping the X.Y.Z that
                             * qMonstatek's parseVerNums() needs. */
                            snprintf(s_fw_name, sizeof(s_fw_name),
                                     "MtkCore %s", fw_name);
                            caps_apply_footprint_estimates(s_bitmap);
                            s_diag.rpc_status_ok = 1u;
                            s_diag.outcome = (uint8_t)M1_ESP32_PROBE_RPC_STATUS;
                            s_diag.bitmap  = s_bitmap;
                            s_queried = true;
                            return;
                        }

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
                        s_diag.rpc_status_ok = 1u;
                        s_diag.outcome = (uint8_t)M1_ESP32_PROBE_RPC_STATUS;
                        s_diag.bitmap  = s_bitmap;
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
                s_diag.outcome = (uint8_t)M1_ESP32_PROBE_RPC_PROFILE;
                s_diag.bitmap  = s_bitmap;
                s_queried = true;
                return;
            }
        }
    }

    /* Neither supported binary transport validated — fail closed. Feature
     * gates that check specific M1_ESP32_CAP_* bits will all return false and
     * the "Feature not
     * supported" UI will appear.  This is intentional: granting capabilities
     * we cannot verify risks crashing on firmware that does not implement
     * the underlying command. */
    s_bitmap = 0u;
    strncpy(s_fw_name, "Unknown (fallback)", sizeof(s_fw_name) - 1);
    s_fw_name[sizeof(s_fw_name) - 1] = '\0';
    s_diag.outcome = (uint8_t)M1_ESP32_PROBE_UNKNOWN;
    s_diag.bitmap  = 0u;
}

void m1_esp32_caps_init(void)
{
    if (!m1_esp32_rpc_transaction_lock())
        return;

    if (!s_queried &&
        !m1_esp32_caps_probe_retry_due(
            s_diag.outcome == (uint8_t)M1_ESP32_PROBE_UNKNOWN,
            s_last_probe_tick, HAL_GetTick()))
    {
        m1_esp32_rpc_transaction_unlock();
        return;
    }

    m1_esp32_caps_init_locked();
    if (!s_queried)
        s_last_probe_tick = HAL_GetTick();
    m1_esp32_rpc_transaction_unlock();
}

void m1_esp32_caps_reset(void)
{
    s_bitmap          = 0u;
    s_bss_bytes       = 0u;
    s_free_heap_bytes = 0u;
    s_queried         = false;
    s_last_probe_tick = 0u;
    s_fw_name[0]      = '\0';
    memset(&s_diag, 0, sizeof(s_diag));
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

void m1_esp32_caps_get_diag(m1_esp32_caps_diag_t *out)
{
    if (!out)
        return;
    *out = s_diag;
}
