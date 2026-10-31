/* See COPYING.txt for license details. */

#include "m1_esp32_rpc_http.h"

#include <stdlib.h>
#include <string.h>
#include "m1_esp32_rpc.h"

#define RPC_HTTP_CHUNK_SIZE   1024u
#define RPC_HTTP_URL_MAX      1024u
#define RPC_HTTP_OPEN_RESP_LEN 7u

extern void m1_wdt_reset(void);

static uint16_t rpc_http_rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8u);
}

static uint32_t rpc_http_rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

static void rpc_http_wr16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8u);
}

static void rpc_http_wr32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8u);
    p[2] = (uint8_t)(value >> 16u);
    p[3] = (uint8_t)(value >> 24u);
}

static http_status_t rpc_http_status(m1_esp32_rpc_status_t status)
{
    switch (status) {
        case M1_ESP32_RPC_OK:              return HTTP_OK;
        case M1_ESP32_RPC_ERR_NOT_RUN:     return HTTP_ERR_NO_WIFI;
        case M1_ESP32_RPC_ERR_UNSUPPORTED: return HTTP_ERR_UNSUPPORTED_TRANSPORT;
        case M1_ESP32_RPC_ERR_TIMEOUT:
        case M1_ESP32_RPC_ERR_TRANSPORT:   return HTTP_ERR_TIMEOUT;
        case M1_ESP32_RPC_ERR_NO_MEM:      return HTTP_ERR_NO_MEM;
        case M1_ESP32_RPC_ERR_INVALID:     return HTTP_ERR_INVALID_ARG;
        case M1_ESP32_RPC_ERR_BAD_FRAME:   return HTTP_ERR_PARSE_FAIL;
        default:                           return HTTP_ERR_CONNECT_FAIL;
    }
}

static http_status_t rpc_http_result(uint8_t result)
{
    switch ((m1_esp32_rpc_http_result_t)result) {
        case M1_ESP32_RPC_HTTP_RESULT_OK:       return HTTP_OK;
        case M1_ESP32_RPC_HTTP_RESULT_INVALID:  return HTTP_ERR_INVALID_ARG;
        case M1_ESP32_RPC_HTTP_RESULT_DNS:      return HTTP_ERR_DNS_FAIL;
        case M1_ESP32_RPC_HTTP_RESULT_TIMEOUT:  return HTTP_ERR_TIMEOUT;
        case M1_ESP32_RPC_HTTP_RESULT_REDIRECT: return HTTP_ERR_REDIRECT_LOOP;
        case M1_ESP32_RPC_HTTP_RESULT_NO_MEM:   return HTTP_ERR_NO_MEM;
        default:                                return HTTP_ERR_CONNECT_FAIL;
    }
}

static void rpc_http_close(void)
{
    (void)m1_esp32_rpc_call(M1_ESP32_RPC_NET_HTTP_CLOSE, NULL, 0u,
                            NULL, 0u, NULL, 2);
}

static http_status_t rpc_http_open(const char *url, bool json, uint8_t timeout_sec,
                                   uint32_t *content_length)
{
    if (!url || !content_length)
        return HTTP_ERR_INVALID_ARG;

    size_t url_len = strlen(url);
    if (url_len == 0u || url_len > RPC_HTTP_URL_MAX)
        return HTTP_ERR_INVALID_ARG;
    if (timeout_sec == 0u)
        timeout_sec = 30u;

    uint8_t *req = malloc(7u + url_len);
    if (!req)
        return HTTP_ERR_NO_MEM;

    req[0] = json ? M1_ESP32_RPC_HTTP_ACCEPT_JSON : M1_ESP32_RPC_HTTP_ACCEPT_BINARY;
    rpc_http_wr32(&req[1], (uint32_t)timeout_sec * 1000u);
    rpc_http_wr16(&req[5], (uint16_t)url_len);
    memcpy(&req[7], url, url_len);

    uint8_t resp[RPC_HTTP_OPEN_RESP_LEN];
    uint16_t rlen = 0u;
    m1_esp32_rpc_status_t rpc_status =
        m1_esp32_rpc_call(M1_ESP32_RPC_NET_HTTP_OPEN, req, (uint16_t)(7u + url_len),
                          resp, sizeof(resp), &rlen, timeout_sec);
    free(req);

    http_status_t status = rpc_http_status(rpc_status);
    if (status != HTTP_OK)
        return status;
    if (rlen != RPC_HTTP_OPEN_RESP_LEN) {
        rpc_http_close();
        return HTTP_ERR_PARSE_FAIL;
    }

    status = rpc_http_result(resp[0]);
    if (status != HTTP_OK)
        return status;

    uint16_t http_status = rpc_http_rd16(&resp[1]);
    if (http_status < 200u || http_status >= 300u) {
        rpc_http_close();
        return HTTP_ERR_HTTP_ERROR;
    }

    int32_t signed_length = (int32_t)rpc_http_rd32(&resp[3]);
    *content_length = signed_length >= 0 ? (uint32_t)signed_length : 0u;
    return HTTP_OK;
}

static http_status_t rpc_http_read(uint8_t *out, uint16_t out_size,
                                   uint16_t *out_len, uint8_t timeout_sec)
{
    uint8_t req[2];
    rpc_http_wr16(req, out_size);
    *out_len = 0u;

    m1_esp32_rpc_status_t rpc_status =
        m1_esp32_rpc_call(M1_ESP32_RPC_NET_HTTP_READ, req, sizeof(req),
                          out, out_size, out_len, timeout_sec);
    return rpc_http_status(rpc_status);
}

http_status_t m1_esp32_rpc_http_get(const char *url, char *response_buf,
                                    uint16_t buf_size, uint8_t timeout_sec)
{
    if (!url || !response_buf || buf_size < 2u)
        return HTTP_ERR_INVALID_ARG;
    response_buf[0] = '\0';
    if (timeout_sec == 0u)
        timeout_sec = 30u;

    uint32_t content_length = 0u;
    http_status_t status = rpc_http_open(url, true, timeout_sec, &content_length);
    if (status != HTTP_OK)
        return status;

    uint16_t total = 0u;
    while (total < buf_size - 1u &&
           (content_length == 0u || total < content_length)) {
        m1_wdt_reset();
        uint16_t want = (uint16_t)(buf_size - 1u - total);
        if (want > RPC_HTTP_CHUNK_SIZE)
            want = RPC_HTTP_CHUNK_SIZE;
        if (content_length > 0u && want > content_length - total)
            want = (uint16_t)(content_length - total);

        uint16_t received = 0u;
        status = rpc_http_read((uint8_t *)response_buf + total, want,
                               &received, timeout_sec);
        if (status != HTTP_OK || received == 0u)
            break;
        total = (uint16_t)(total + received);
    }
    response_buf[total] = '\0';
    rpc_http_close();

    if (status != HTTP_OK)
        return status;
    if (total == 0u)
        return HTTP_ERR_PARSE_FAIL;
    if (content_length > 0u && total < content_length)
        return total == buf_size - 1u ? HTTP_ERR_RESPONSE_TOO_LARGE : HTTP_ERR_TIMEOUT;
    if (total == buf_size - 1u && content_length == 0u)
        return HTTP_ERR_RESPONSE_TOO_LARGE;
    return HTTP_OK;
}

http_status_t m1_esp32_rpc_http_download_to_file(const char *url, const char *sd_path,
                                                 http_progress_cb_t progress_cb,
                                                 uint8_t timeout_sec)
{
    if (!url || !sd_path || sd_path[0] == '\0')
        return HTTP_ERR_INVALID_ARG;
    if (timeout_sec == 0u)
        timeout_sec = 30u;

    uint32_t content_length = 0u;
    http_status_t status = rpc_http_open(url, false, timeout_sec, &content_length);
    if (status != HTTP_OK)
        return status;

    FIL file;
    FRESULT fr = f_open(&file, sd_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        rpc_http_close();
        return HTTP_ERR_SD_OPEN_FAIL;
    }

    uint32_t downloaded = 0u;
    if (progress_cb && !progress_cb(0u, content_length)) {
        f_close(&file);
        f_unlink(sd_path);
        rpc_http_close();
        return HTTP_ERR_CANCELLED;
    }

    while (content_length == 0u || downloaded < content_length) {
        m1_wdt_reset();
        uint16_t want = RPC_HTTP_CHUNK_SIZE;
        if (content_length > 0u && want > content_length - downloaded)
            want = (uint16_t)(content_length - downloaded);

        uint8_t chunk[RPC_HTTP_CHUNK_SIZE];
        uint16_t received = 0u;
        status = rpc_http_read(chunk, want, &received, timeout_sec);
        if (status != HTTP_OK || received == 0u)
            break;

        UINT written = 0u;
        fr = f_write(&file, chunk, received, &written);
        if (fr != FR_OK || written != received) {
            status = HTTP_ERR_SD_WRITE_FAIL;
            break;
        }
        downloaded += received;

        if (progress_cb && !progress_cb(downloaded, content_length)) {
            status = HTTP_ERR_CANCELLED;
            break;
        }
    }

    f_close(&file);
    rpc_http_close();

    if (status == HTTP_OK && content_length > 0u && downloaded < content_length)
        status = HTTP_ERR_TIMEOUT;
    if (status != HTTP_OK)
        f_unlink(sd_path);
    return status;
}
