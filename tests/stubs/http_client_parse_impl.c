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

/* HTTP client status codes — must match m1_http_client.h */
typedef enum {
	HTTP_OK = 0,
	HTTP_ERR_NO_WIFI,
	HTTP_ERR_ESP_NOT_READY,
	HTTP_ERR_DNS_FAIL,
	HTTP_ERR_CONNECT_FAIL,
	HTTP_ERR_SEND_FAIL,
	HTTP_ERR_TIMEOUT,
	HTTP_ERR_HTTP_ERROR,
	HTTP_ERR_REDIRECT_LOOP,
	HTTP_ERR_RESPONSE_TOO_LARGE,
	HTTP_ERR_SD_WRITE_FAIL,
	HTTP_ERR_SD_OPEN_FAIL,
	HTTP_ERR_CANCELLED,
	HTTP_ERR_INVALID_ARG,
	HTTP_ERR_PARSE_FAIL,
} http_status_t;

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
