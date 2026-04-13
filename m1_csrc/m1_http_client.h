/* See COPYING.txt for license details. */

/*
 * m1_http_client.h
 *
 * Reusable HTTP client module for M1.
 * Uses ESP32-C6 AT commands for HTTP/HTTPS communication.
 *
 * Both http_get and http_download_to_file use the raw TCP/SSL path
 * (AT+CIPSTART / AT+CIPSEND / AT+CIPRECVDATA) — not AT+HTTPCLIENT — so
 * there is no 2 KB receive-buffer limit from the ESP32 AT HTTP layer.
 *
 * M1 Project
 */

#ifndef M1_HTTP_CLIENT_H_
#define M1_HTTP_CLIENT_H_

#include <stdint.h>
#include <stdbool.h>
#include "ff.h"

/* HTTP client status codes */
typedef enum {
	HTTP_OK = 0,
	HTTP_ERR_NO_WIFI,           /* WiFi not connected */
	HTTP_ERR_ESP_NOT_READY,     /* ESP32 not initialized */
	HTTP_ERR_DNS_FAIL,          /* DNS resolution failed */
	HTTP_ERR_CONNECT_FAIL,      /* TCP/SSL connection failed */
	HTTP_ERR_SEND_FAIL,         /* Failed to send request */
	HTTP_ERR_TIMEOUT,           /* Response timeout */
	HTTP_ERR_HTTP_ERROR,        /* HTTP status 4xx/5xx */
	HTTP_ERR_REDIRECT_LOOP,     /* Too many redirects */
	HTTP_ERR_RESPONSE_TOO_LARGE,/* Response exceeds buffer */
	HTTP_ERR_SD_WRITE_FAIL,     /* SD card write error */
	HTTP_ERR_SD_OPEN_FAIL,      /* Cannot open/create file */
	HTTP_ERR_CANCELLED,         /* Operation cancelled by user */
	HTTP_ERR_INVALID_ARG,       /* Invalid argument */
	HTTP_ERR_PARSE_FAIL,        /* Failed to parse response */
} http_status_t;

/* Progress callback for download operations.
 * downloaded: bytes downloaded so far
 * total: total bytes expected (0 if unknown)
 * Returns: true to continue, false to cancel download */
typedef bool (*http_progress_cb_t)(uint32_t downloaded, uint32_t total);

/*
 * Perform an HTTP GET for API-style responses.
 * Uses raw TCP/SSL (same path as http_download_to_file) to avoid the 2 KB
 * AT+HTTPCLIENT receive-buffer limit in the ESP32 AT firmware.
 * Response body is stored in response_buf, null-terminated.
 * Includes User-Agent and Accept: application/json headers automatically.
 *
 * url:          Full URL (http:// or https://)
 * response_buf: Buffer for response body
 * buf_size:     Size of response buffer
 * timeout_sec:  Connection/response timeout in seconds (0 for default 30s)
 *
 * Returns: HTTP_OK on success, error code otherwise
 */
http_status_t http_get(const char *url, char *response_buf, uint16_t buf_size, uint8_t timeout_sec);

/*
 * Download a file via HTTP(S) and save to SD card.
 * Uses AT+CIPSTART for streaming large files.
 * Follows up to 3 HTTP redirects.
 *
 * url:          Full URL to download
 * sd_path:      Destination path on SD card (e.g., "0:/Firmware/file.bin")
 * progress_cb:  Optional progress callback (may be NULL)
 * timeout_sec:  Per-chunk timeout in seconds (0 for default 30s)
 *
 * Returns: HTTP_OK on success, error code otherwise
 */
http_status_t http_download_to_file(const char *url, const char *sd_path,
                                     http_progress_cb_t progress_cb, uint8_t timeout_sec);

/*
 * Check if WiFi is connected and ESP32 is ready for HTTP operations.
 */
bool http_is_ready(void);

/*
 * Reset cached SSL configuration state.
 * Must be called when the ESP32 is deinitialized or reset, because a
 * hardware reset wipes all ESP32-side AT config (SNTP, CIPSSLCCONF).
 * The next SSL connection will re-send the configuration commands.
 */
void http_ssl_reset(void);

/*
 * Return a human-readable error string for an http_status_t value.
 * Returns NULL for HTTP_OK (no error) and unknown values.
 * Useful for displaying error reasons in the UI.
 */
const char *http_status_str(http_status_t status);

#endif /* M1_HTTP_CLIENT_H_ */
