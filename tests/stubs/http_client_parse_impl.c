/*
 * http_client_parse_impl.c — test-only copy of parse_url() from
 * m1_http_client.c.  The production file has heavy ESP32 SPI and
 * HAL dependencies that prevent direct host compilation; this copy
 * mirrors the pure-logic URL parser so it can be tested in isolation.
 *
 * Keep in sync with m1_csrc/m1_http_client.c::parse_url().
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
