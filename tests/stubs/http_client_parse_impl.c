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

/*
 * parse_http_headers() — test-only copy of the pure-logic header parser
 * from m1_csrc/m1_http_client.c.
 *
 * Extracts status code, Content-Length, Location (for redirects), and
 * Transfer-Encoding: chunked flag from raw HTTP response data.
 * Returns HTTP status code (200, 301, etc.) or 0 if headers are
 * incomplete (no \r\n\r\n terminator found).
 *
 * Key regression scenario: when SSL fragments the response so the first
 * tcp_recv() doesn't include the full header block, parse_http_headers()
 * must return 0 so the caller can accumulate more data before retrying.
 *
 * Keep in sync with m1_csrc/m1_http_client.c::parse_http_headers().
 */
int http_parse_headers(const char *data, int data_len,
                       uint32_t *content_length, char *location, uint16_t loc_size,
                       int *header_end_offset, bool *is_chunked)
{
	const char *hdr_end;
	const char *p;
	int status_code = 0;

	(void)data_len;

	*content_length = 0;
	*header_end_offset = 0;
	if (is_chunked) *is_chunked = false;
	if (location) location[0] = '\0';

	/* Find end of headers */
	hdr_end = strstr(data, "\r\n\r\n");
	if (!hdr_end)
		return 0; /* headers incomplete */

	*header_end_offset = (int)((hdr_end - data) + 4);

	/* Parse status line: HTTP/1.x <status_code> ... */
	p = strstr(data, "HTTP/");
	if (!p) return 0;

	p = strchr(p, ' ');
	if (!p) return 0;
	p++;
	status_code = atoi(p);

	/* Parse Content-Length */
	p = strstr(data, "Content-Length:");
	if (!p) p = strstr(data, "content-length:");
	if (p)
	{
		p = strchr(p, ':');
		if (p)
		{
			p++;
			while (*p == ' ') p++;
			*content_length = (uint32_t)strtoul(p, NULL, 10);
		}
	}

	/* Detect Transfer-Encoding: chunked with case-insensitive
	 * field-name matching, per HTTP header rules. */
	if (is_chunked)
	{
		const char *line = data;
		*is_chunked = false;

		while (line < hdr_end)
		{
			const char *line_end = line;
			const char *colon;

			while (line_end < hdr_end && *line_end != '\r' && *line_end != '\n')
				line_end++;

			colon = memchr(line, ':', (size_t)(line_end - line));
			if (colon)
			{
				static const char te_name[] = "Transfer-Encoding";
				size_t name_len = (size_t)(colon - line);

				if (name_len == sizeof(te_name) - 1)
				{
					size_t i;
					bool name_match = true;

					for (i = 0; i < sizeof(te_name) - 1; i++)
					{
						char a = line[i];
						char b = te_name[i];

						if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
						if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');

						if (a != b)
						{
							name_match = false;
							break;
						}
					}

					if (name_match)
					{
						const char *value = colon + 1;

						while (value < line_end && (*value == ' ' || *value == '\t'))
							value++;

						while (value + 7 <= line_end)
						{
							char c0 = value[0];
							char c1 = value[1];
							char c2 = value[2];
							char c3 = value[3];
							char c4 = value[4];
							char c5 = value[5];
							char c6 = value[6];

							if (c0 >= 'A' && c0 <= 'Z') c0 = (char)(c0 - 'A' + 'a');
							if (c1 >= 'A' && c1 <= 'Z') c1 = (char)(c1 - 'A' + 'a');
							if (c2 >= 'A' && c2 <= 'Z') c2 = (char)(c2 - 'A' + 'a');
							if (c3 >= 'A' && c3 <= 'Z') c3 = (char)(c3 - 'A' + 'a');
							if (c4 >= 'A' && c4 <= 'Z') c4 = (char)(c4 - 'A' + 'a');
							if (c5 >= 'A' && c5 <= 'Z') c5 = (char)(c5 - 'A' + 'a');
							if (c6 >= 'A' && c6 <= 'Z') c6 = (char)(c6 - 'A' + 'a');

							if (c0 == 'c' && c1 == 'h' && c2 == 'u' &&
							    c3 == 'n' && c4 == 'k' && c5 == 'e' &&
							    c6 == 'd')
							{
								*is_chunked = true;
								break;
							}

							value++;
						}

						if (*is_chunked)
							break;
					}
				}
			}

			if (line_end < hdr_end && *line_end == '\r') line_end++;
			if (line_end < hdr_end && *line_end == '\n') line_end++;
			line = line_end;
		}
	}

	/* Parse Location header (for redirects) */
	if (location && loc_size > 0)
	{
		p = strstr(data, "Location:");
		if (!p) p = strstr(data, "location:");
		if (p)
		{
			p = strchr(p, ':');
			if (p)
			{
				p++;
				while (*p == ' ') p++;
				int i = 0;
				while (*p && *p != '\r' && *p != '\n' && i < loc_size - 1)
					location[i++] = *p++;
				location[i] = '\0';
			}
		}
	}

	return status_code;
}
