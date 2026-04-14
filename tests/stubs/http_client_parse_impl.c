/*
 * http_client_parse_impl.c — test-only copy of parse_url() and
 * http_status_str() from m1_http_client.c.  The production file has
 * heavy ESP32 SPI and HAL dependencies that prevent direct host
 * compilation; this copy mirrors the pure-logic functions so they
 * can be tested in isolation.
 *
 * Keep in sync with m1_csrc/m1_http_client.c::parse_url() and
 * m1_csrc/m1_http_client.c::http_status_str().
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "http_status_defs.h"

const char *http_status_str(http_status_t status)
{
	switch (status)
	{
		case HTTP_OK:                     return NULL;
		case HTTP_ERR_NO_WIFI:            return "WiFi not connected";
		case HTTP_ERR_ESP_NOT_READY:      return "ESP32 not ready";
		case HTTP_ERR_DNS_FAIL:           return "DNS lookup failed";
		case HTTP_ERR_CONNECT_FAIL:       return "Connection failed";
		case HTTP_ERR_SEND_FAIL:          return "Request send failed";
		case HTTP_ERR_TIMEOUT:            return "Server timeout";
		case HTTP_ERR_HTTP_ERROR:         return "Server error (HTTP)";
		case HTTP_ERR_REDIRECT_LOOP:      return "Too many redirects";
		case HTTP_ERR_RESPONSE_TOO_LARGE: return "Response too large";
		case HTTP_ERR_SD_WRITE_FAIL:      return "SD card write error";
		case HTTP_ERR_SD_OPEN_FAIL:       return "Cannot open file";
		case HTTP_ERR_CANCELLED:          return "Cancelled";
		case HTTP_ERR_INVALID_ARG:        return "Internal request error";
		case HTTP_ERR_PARSE_FAIL:         return "Bad response";
		default:                          return NULL;
	}
}

bool http_parse_url(const char *url, char *host, uint16_t host_size,
                    uint16_t *port, char *path, uint16_t path_size, bool *is_https)
{
	const char *p = url;
	const char *host_start;
	uint16_t host_len;

	if (strncmp(p, "https://", 8) == 0)
	{
		*is_https = true;
		*port = 443;
		p += 8;
	}
	else if (strncmp(p, "http://", 7) == 0)
	{
		*is_https = false;
		*port = 80;
		p += 7;
	}
	else
	{
		return false;
	}

	host_start = p;

	/* Find end of host (: for port, / for path, or end of string) */
	while (*p && *p != ':' && *p != '/')
		p++;

	host_len = p - host_start;
	if (host_len >= host_size || host_len == 0)
		return false;

	memcpy(host, host_start, host_len);
	host[host_len] = '\0';

	/* Optional port */
	if (*p == ':')
	{
		p++;
		*port = (uint16_t)atoi(p);
		while (*p >= '0' && *p <= '9')
			p++;
	}

	/* Path (default to "/") */
	if (*p == '/')
	{
		strncpy(path, p, path_size - 1);
		path[path_size - 1] = '\0';
	}
	else
	{
		strncpy(path, "/", path_size - 1);
		path[path_size - 1] = '\0';
	}

	return true;
}

/*
 * http_is_ready_check() — test-only copy of the http_is_ready() logic.
 *
 * Takes the three flag values as parameters (wifi connected, ESP32 HAL
 * initialized, ESP32 SPI task initialized) instead of reading hardware state.
 *
 * Mirrors the production logic in m1_csrc/m1_http_client.c::http_is_ready():
 *   return wifi_is_connected() && m1_esp32_get_init_status() && get_esp32_main_init_status();
 *
 * Regression: before the fix, http_is_ready() only checked wifi_connected and
 * task_init, missing the hal_init check.  After wifi_scan_ap() exits it calls
 * m1_esp32_deinit() which clears esp32_init_done (the HAL flag) but leaves
 * esp32_main_init_done (the task flag) true.  The old check returned true even
 * with the ESP32 SPI disabled, so fw_download_start() proceeded with AT commands
 * that all timed out — manifesting as a DNS lookup failure (HTTP_ERR_DNS_FAIL).
 */
bool http_is_ready_check(bool wifi_connected, bool hal_init, bool task_init)
{
	return wifi_connected && hal_init && task_init;
}

/*
 * http_readiness_status() — test-only copy of the layered readiness check used
 * inside http_get() and http_download_to_file().
 *
 * Mirrors the split check introduced to return the correct error code:
 *   if (!wifi_is_connected())                                   → HTTP_ERR_NO_WIFI
 *   if (!m1_esp32_get_init_status() || !get_esp32_main_init_status()) → HTTP_ERR_ESP_NOT_READY
 *   otherwise                                                    → HTTP_OK
 */
http_status_t http_readiness_status(bool wifi_connected, bool hal_init, bool task_init)
{
	if (!wifi_connected)
		return HTTP_ERR_NO_WIFI;
	if (!hal_init || !task_init)
		return HTTP_ERR_ESP_NOT_READY;
	return HTTP_OK;
}
