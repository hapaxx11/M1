/* See COPYING.txt for license details. */

/*
 * m1_http_client.c
 *
 * Reusable HTTP client module for M1.
 * Uses ESP32-C6 AT commands for HTTP/HTTPS communication.
 *
 * M1 Project
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_http_client.h"
#include "m1_wifi.h"
#include "m1_esp32_hal.h"
#include "m1_compile_cfg.h"
#include "m1_file_browser.h"
#include "m1_watchdog.h"
#include "m1_log_debug.h"
#include "esp_app_main.h"
#include "esp_at_list.h"

#define HTTP_TAG "HTTP"

/* AT response buffer — shared for all operations.
 * 4KB matches the SPI transfer max size. */
#define HTTP_AT_BUF_SIZE    4096

/* Maximum number of HTTP redirects to follow */
#define HTTP_MAX_REDIRECTS  3

/* Default timeout if caller passes 0 */
#define HTTP_DEFAULT_TIMEOUT  30

/* Download chunk size for CIPRECVDATA */
#define HTTP_DOWNLOAD_CHUNK  2048

/* Maximum retries per second for download polling (200ms intervals) */
#define HTTP_RETRY_PER_SEC  5

/* DNS pre-resolution retry parameters.
 * Transient DNS failures are common on WiFi (DHCP not settled,
 * ESP32 state after closing a prior connection, ISP DNS hiccup).
 * Retry a few times before giving up.  The DNS pre-resolution phase
 * is budgeted against the caller's timeout_sec; subsequent SSL
 * connect retries are separate and may add additional time. */
#define DNS_MAX_ATTEMPTS       3
#define DNS_RETRY_DELAY_MS  1000
#define DNS_PER_ATTEMPT_SEC   10   /* AT+CIPDOMAIN timeout per try */

static char s_at_buf[HTTP_AT_BUF_SIZE];

/* Separate SPI working buffer for tcp_recv / tcp_recv_available.
 * These functions issue AT commands via spi_AT_send_recv() which writes
 * into the provided buffer.  If they shared s_at_buf, each call would
 * clobber any partially-accumulated HTTP header data that the caller is
 * building up in s_at_buf.  Using a dedicated buffer lets the caller
 * safely accumulate data across multiple tcp_recv calls. */
static char s_at_recv_buf[HTTP_AT_BUF_SIZE];

/* Forward declarations for internal helpers used by both http_get and http_download_to_file */
static bool parse_url(const char *url, char *host, uint16_t host_size,
                      uint16_t *port, char *path, uint16_t path_size, bool *is_https);
static http_status_t tcp_connect(const char *host, uint16_t port, bool is_https, uint8_t timeout_sec);
static void tcp_close(void);
static http_status_t tcp_send(const char *data, uint16_t len, uint8_t timeout_sec);
static int tcp_recv(char *buf, uint16_t max_len, uint8_t timeout_sec);
static int tcp_recv_available(uint8_t timeout_sec);
static int tcp_wait_data(uint8_t timeout_sec);
static int tcp_recv_headers(int initial_len, uint8_t timeout_sec);
static int parse_http_headers(const char *data, int data_len,
                               uint32_t *content_length, char *location, uint16_t loc_size,
                               int *header_end_offset, bool *is_chunked);

bool http_is_ready(void)
{
#ifdef M1_APP_WIFI_CONNECT_ENABLE
	return wifi_is_connected() && m1_esp32_get_init_status() && get_esp32_main_init_status();
#else
	return false;
#endif
}

const char *http_status_str(http_status_t status)
{
	switch (status)
	{
		case HTTP_OK:                     return NULL; /* no error */
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

/*
 * http_get() — HTTP GET for API calls (small-to-medium responses).
 *
 * Uses the same raw TCP/SSL path as http_download_to_file so there is no
 * AT+HTTPCLIENT receive-buffer limit (was capped at 2 KB in the ESP32 AT
 * firmware build).  The response body is written into response_buf up to
 * buf_size-1 bytes, null-terminated.
 *
 * GitHub API requires a User-Agent header; it is included automatically.
 *
 * Termination heuristic for chunked responses (no Content-Length):
 *   The loop exits as soon as the content-length is satisfied, OR when no
 *   new bytes arrive for HTTP_GET_IDLE_TIMEOUT_MS ms (connection closed
 *   by server after sending complete response).
 */

/* Maximum idle time after last received byte before declaring API response done */
#define HTTP_GET_IDLE_TIMEOUT_MS  3000

/*
 * Decode chunked transfer encoding in-place.
 *
 * Chunked bodies look like:
 *   <hex-size>\r\n<data>\r\n<hex-size>\r\n<data>\r\n0\r\n\r\n
 *
 * Returns the decoded body length (always <= original length since chunk
 * markers are stripped).  The buffer is null-terminated after decoding.
 */
static uint16_t decode_chunked_body(char *body, uint16_t body_len)
{
	char *src = body;
	char *dst = body;
	char *end = body + body_len;

	while (src < end)
	{
		/* Skip leading \r\n (trailing from previous chunk) */
		while (src < end && (*src == '\r' || *src == '\n'))
			src++;
		if (src >= end)
			break;

		/* Read hex chunk size */
		char *size_end = NULL;
		unsigned long chunk_size = strtoul(src, &size_end, 16);

		/* If strtoul didn't consume any characters, data is malformed */
		if (size_end == src)
			break;

		/* Skip any optional chunk extensions up to the line ending */
		src = size_end;
		while (src < end && *src != '\r' && *src != '\n')
			src++;
		if (src >= end)
			break;

		/* Consume the chunk-size line ending */
		if (*src == '\r')
		{
			src++;
			if (src < end && *src == '\n')
				src++;
		}
		else if (*src == '\n')
		{
			src++;
		}
		if (src >= end)
			break;

		/* Terminal chunk (size 0) — we're done */
		if (chunk_size == 0)
			break;

		/* Copy chunk data — clamp to what's available */
		uint16_t avail = (uint16_t)(end - src);
		if (chunk_size > avail)
			chunk_size = avail;

		/* In-place safe: dst always <= src (we stripped markers) */
		memmove(dst, src, chunk_size);
		dst += chunk_size;
		src += chunk_size;
	}

	*dst = '\0';
	return (uint16_t)(dst - body);
}

http_status_t http_get(const char *url, char *response_buf, uint16_t buf_size, uint8_t timeout_sec)
{
	char host[128];
	char path[256];
	char request[512];
	uint16_t port;
	bool is_https;
	http_status_t status;
	uint32_t content_length = 0;
	bool chunked = false;
	int header_end_offset = 0;
	int status_code;
	uint16_t total = 0;

	if (!url || !response_buf || buf_size < 2)
		return HTTP_ERR_INVALID_ARG;

	response_buf[0] = '\0';

	if (!wifi_is_connected())
		return HTTP_ERR_NO_WIFI;
	if (!m1_esp32_get_init_status() || !get_esp32_main_init_status())
		return HTTP_ERR_ESP_NOT_READY;

	if (!timeout_sec)
		timeout_sec = HTTP_DEFAULT_TIMEOUT;

	/* Parse URL */
	if (!parse_url(url, host, sizeof(host), &port, path, sizeof(path), &is_https))
		return HTTP_ERR_INVALID_ARG;

	/* Open TCP/SSL connection */
	status = tcp_connect(host, port, is_https, timeout_sec);
	if (status != HTTP_OK)
		return status;

	/* Build and send HTTP GET request with required headers.
	 * Use HTTP/1.0 to prevent chunked transfer encoding — the response
	 * must arrive with Content-Length or be terminated by connection close.
	 * This avoids chunk-size markers corrupting the JSON body. */
	snprintf(request, sizeof(request),
	         "GET %s HTTP/1.0\r\n"
	         "Host: %s\r\n"
	         "User-Agent: M1-Hapax/1.0\r\n"
	         "Accept: application/json\r\n"
	         "Connection: close\r\n"
	         "\r\n",
	         path, host);

	status = tcp_send(request, strlen(request), timeout_sec);
	if (status != HTTP_OK)
	{
		tcp_close();
		return status;
	}

	/* Wait for the first bytes of the response */
	if (tcp_wait_data(timeout_sec) <= 0)
	{
		tcp_close();
		return HTTP_ERR_TIMEOUT;
	}

	m1_wdt_reset();

	/* Read initial chunk — large enough to capture headers + start of body.
	 * Cap to HTTP_DOWNLOAD_CHUNK so the AT response (+CIPRECVDATA header +
	 * data + \r\nOK\r\n) fits within the 4KB s_at_recv_buf. */
	int initial_len = tcp_recv(s_at_buf, HTTP_DOWNLOAD_CHUNK, timeout_sec);
	if (initial_len <= 0)
	{
		tcp_close();
		return HTTP_ERR_TIMEOUT;
	}
	s_at_buf[initial_len] = '\0';

	/* Accumulate more data if headers are incomplete (SSL fragmentation). */
	initial_len = tcp_recv_headers(initial_len, timeout_sec);

	/* Parse HTTP response headers */
	status_code = parse_http_headers(s_at_buf, initial_len,
	                                  &content_length, NULL, 0,
	                                  &header_end_offset, &chunked);
	if (status_code == 0)
	{
		tcp_close();
		return HTTP_ERR_PARSE_FAIL;
	}

	if (status_code < 200 || status_code >= 300)
	{
		tcp_close();
		return HTTP_ERR_HTTP_ERROR;
	}

	/* Copy body bytes that arrived with the headers */
	int body_in_first = initial_len - header_end_offset;
	if (header_end_offset > initial_len)
		body_in_first = 0;
	if (body_in_first > 0)
	{
		int copy = body_in_first;
		if (copy > (int)(buf_size - 1))
			copy = buf_size - 1;
		memmove(response_buf, s_at_buf + header_end_offset, copy);
		total = (uint16_t)copy;
	}

	/*
	 * Stream remaining body data until:
	 *   a) content_length is known and we have it all, OR
	 *   b) no new data arrives for HTTP_GET_IDLE_TIMEOUT_MS ms after the last
	 *      received byte (server closed connection after complete response), OR
	 *   c) the overall operation deadline (timeout_sec) expires — tracked with
	 *      a separate start_tick that is NEVER reset, so a slow but steady
	 *      stream cannot push the deadline past timeout_sec, OR
	 *   d) response_buf is full — returns HTTP_ERR_RESPONSE_TOO_LARGE so the
	 *      caller knows the JSON is truncated and cannot be parsed.
	 *
	 * idle_start is reset only when new bytes arrive (idle heuristic).
	 * start_tick is captured once and never changed (hard deadline).
	 */
	uint32_t start_tick = HAL_GetTick();  /* hard deadline — never reset */
	uint32_t idle_start = HAL_GetTick();  /* idle-completion heuristic */
	bool hard_deadline_hit = false;

	while (total < buf_size - 1)
	{
		m1_wdt_reset();

		/* Done if we have all bytes indicated by Content-Length */
		if (content_length > 0 && (uint32_t)total >= content_length)
			break;

		/* Check timeouts once per iteration */
		uint32_t now = HAL_GetTick();
		if (total > 0 && (now - idle_start) >= HTTP_GET_IDLE_TIMEOUT_MS)
			break; /* server closed connection after sending complete response */
		if ((now - start_tick) >= (uint32_t)timeout_sec * 1000U)
		{
			hard_deadline_hit = true;
			break; /* hard deadline expired — regardless of ongoing data flow */
		}

		int avail = tcp_recv_available(2);
		if (avail <= 0)
		{
			vTaskDelay(pdMS_TO_TICKS(200));
			continue;
		}

		/* Data is available — reset idle timer only (not the hard deadline) */
		idle_start = HAL_GetTick();

		int to_read = avail;
		if (to_read > HTTP_DOWNLOAD_CHUNK)
			to_read = HTTP_DOWNLOAD_CHUNK;
		if (total + to_read > (int)(buf_size - 1))
			to_read = (int)(buf_size - 1) - total;
		if (to_read <= 0)
		{
			/* Buffer full — response is truncated; closing before returning */
			tcp_close();
			return HTTP_ERR_RESPONSE_TOO_LARGE;
		}

		char recv_buf[HTTP_DOWNLOAD_CHUNK];
		int received = tcp_recv(recv_buf, to_read, timeout_sec);
		if (received <= 0)
		{
			vTaskDelay(pdMS_TO_TICKS(200));
			continue;
		}

		memcpy(response_buf + total, recv_buf, received);
		total += (uint16_t)received;
	}

	response_buf[total] = '\0';
	tcp_close();

	/*
	 * If the server sent chunked transfer encoding (unexpected for HTTP/1.0
	 * but possible with non-compliant servers), decode the body in-place to
	 * strip chunk-size markers that would otherwise corrupt JSON parsing.
	 */
	if (chunked && total > 0)
		total = decode_chunked_body(response_buf, total);

	if (total == 0)
		return HTTP_ERR_PARSE_FAIL;

	/* If the hard deadline expired before the response completed, report
	 * timeout so the caller knows the body may be truncated/incomplete. */
	if (hard_deadline_hit)
	{
		bool complete = false;
		if (content_length > 0 && (uint32_t)total >= content_length)
			complete = true;
		if (!complete)
			return HTTP_ERR_TIMEOUT;
	}

	return HTTP_OK;
}

/*
 * Parse URL into host, port, and path components.
 * Returns true on success.
 */
static bool parse_url(const char *url, char *host, uint16_t host_size,
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
 * SSL/SNTP setup state.  Cleared by http_ssl_reset() when the ESP32 is
 * deinitialized, since a hardware reset wipes all ESP32-side AT config.
 */
static bool s_ssl_configured = false;

void http_ssl_reset(void)
{
	s_ssl_configured = false;
}

/* Maximum SSL connection attempts (original + retries) */
#define SSL_CONNECT_MAX_ATTEMPTS  3

/* Delay between SSL connection retries (milliseconds) */
#define SSL_CONNECT_RETRY_DELAY_MS  2000

/* Maximum time to wait for SNTP time sync (milliseconds) */
#define SSL_SNTP_SYNC_WAIT_MS  3000

/*
 * SSL/SNTP setup.  Called before each SSL connection.
 * Configures SNTP so the ESP32 has valid system time (required by
 * some TLS implementations even without certificate verification),
 * and disables SSL certificate verification (the M1 does not ship
 * a CA certificate store).
 *
 * After enabling SNTP, waits briefly for the time to sync.  Some
 * ESP32 AT firmware builds reject SSL handshakes when system time
 * is at epoch (1970-01-01), even with auth_mode=0.
 *
 * Only caches success after AT+CIPSSLCCONF returns OK — if either
 * command fails (timeout, ESP32 busy, etc.) we retry on the next
 * SSL connection attempt.
 */
static void ssl_ensure_configured(void)
{
	if (s_ssl_configured)
		return;

	/* Enable SNTP so the ESP32 has valid time for TLS. */
	spi_AT_send_recv(
		ESP32C6_AT_REQ_CIPSNTPCFG "1,0,\"pool.ntp.org\",\"time.google.com\"" ESP32C6_AT_REQ_CRLF,
		s_at_buf, sizeof(s_at_buf), 3);
	if (!strstr(s_at_buf, "OK"))
		M1_LOG_W(HTTP_TAG, "SNTP config failed: %.64s\n\r", s_at_buf);

	/* Wait for SNTP time sync.  Poll AT+CIPSNTPTIME? until the
	 * response no longer contains "1970" (epoch = no sync yet).
	 * Query immediately once so an already-synced clock does not
	 * incur a fixed initial delay, then wait 500 ms between retries.
	 * Give up after SSL_SNTP_SYNC_WAIT_MS — auth_mode=0 should
	 * still allow SSL without valid time on most firmware builds. */
	uint32_t sntp_start = HAL_GetTick();
	bool first_poll = true;
	while ((HAL_GetTick() - sntp_start) < SSL_SNTP_SYNC_WAIT_MS)
	{
		if (!first_poll)
			vTaskDelay(pdMS_TO_TICKS(500));
		first_poll = false;
		spi_AT_send_recv(
			ESP32C6_AT_REQ_CIPSNTPTIME ESP32C6_AT_REQ_CRLF,
			s_at_buf, sizeof(s_at_buf), 2);
		if (strstr(s_at_buf, "+CIPSNTPTIME:") && !strstr(s_at_buf, "1970"))
		{
			M1_LOG_I(HTTP_TAG, "SNTP synced\n\r");
			break;
		}
	}

	/* Disable SSL certificate verification (auth_mode=0).
	 * Without this the ESP32 attempts to verify the server certificate
	 * against a CA store that is either empty or doesn't contain the
	 * required root CA, causing every HTTPS connection to fail. */
	spi_AT_send_recv(
		ESP32C6_AT_REQ_CIPSSLCCONF "0" ESP32C6_AT_REQ_CRLF,
		s_at_buf, sizeof(s_at_buf), 3);
	if (strstr(s_at_buf, "OK"))
		s_ssl_configured = true;
	else
		M1_LOG_W(HTTP_TAG, "SSL config failed: %.64s\n\r", s_at_buf);
}

/*
 * Open a TCP/SSL connection to host:port via ESP32 AT.
 *
 * For SSL connections (is_https=true):
 *   - Closes any stale connection that might prevent AT+CIPSTART
 *   - Resolves the hostname via AT+CIPDOMAIN first (catches DNS failures)
 *   - Retries up to SSL_CONNECT_MAX_ATTEMPTS times with back-off delay
 *   - Forces SSL reconfiguration (SNTP + CIPSSLCCONF) on each retry
 *   - Logs the AT response on failure for diagnostics
 */
static http_status_t tcp_connect(const char *host, uint16_t port, bool is_https, uint8_t timeout_sec)
{
	char cmd[256];
	int max_attempts = is_https ? SSL_CONNECT_MAX_ATTEMPTS : 1;

	/* Set passive receive mode first */
	snprintf(cmd, sizeof(cmd), ESP32C6_AT_REQ_CIPRECVMODE "1" ESP32C6_AT_REQ_CRLF);
	spi_AT_send_recv(cmd, s_at_buf, sizeof(s_at_buf), 5);

	/* For SSL: do DNS pre-resolution to catch network issues early.
	 * AT+CIPDOMAIN is much faster than AT+CIPSTART="SSL" and gives
	 * a clear signal when DNS is broken (vs a generic SSL timeout).
	 * Retry up to DNS_MAX_ATTEMPTS times — transient failures are
	 * common right after WiFi connect or when following redirects
	 * (GitHub → objects.githubusercontent.com).
	 * The DNS pre-resolution phase uses timeout_sec to limit attempts;
	 * note that spi_AT_send_recv() enforces the timeout per response
	 * chunk, so actual wall time per attempt is approximate. */
	if (is_https)
	{
		bool dns_ok = false;
		uint8_t dns_at_timeout = DNS_PER_ATTEMPT_SEC;
		if (dns_at_timeout > timeout_sec)
			dns_at_timeout = timeout_sec;

		/* How many attempts fit within the caller's timeout?
		 * Each attempt costs dns_at_timeout + back-off delay
		 * (except the first which has no delay). */
		int dns_per_attempt_cost = dns_at_timeout + (DNS_RETRY_DELAY_MS / 1000);
		int dns_attempts = DNS_MAX_ATTEMPTS;
		if (dns_per_attempt_cost > 0 && (int)timeout_sec < dns_attempts * dns_per_attempt_cost)
		{
			dns_attempts = 1 + ((int)timeout_sec - dns_at_timeout) / dns_per_attempt_cost;
			if (dns_attempts < 1)
				dns_attempts = 1;
			if (dns_attempts > DNS_MAX_ATTEMPTS)
				dns_attempts = DNS_MAX_ATTEMPTS;
		}

		snprintf(cmd, sizeof(cmd), ESP32C6_AT_REQ_CIPDOMAIN "\"%s\"" ESP32C6_AT_REQ_CRLF, host);

		for (int dns_try = 0; dns_try < dns_attempts; dns_try++)
		{
			if (dns_try > 0)
			{
				M1_LOG_W(HTTP_TAG, "DNS retry %d/%d for %s\n\r",
				         dns_try + 1, dns_attempts, host);
				vTaskDelay(pdMS_TO_TICKS(DNS_RETRY_DELAY_MS));
				m1_wdt_reset();
			}

			spi_AT_send_recv(cmd, s_at_buf, sizeof(s_at_buf), dns_at_timeout);
			if (strstr(s_at_buf, ESP32C6_AT_RES_CIPDOMAIN_KEY))
			{
				dns_ok = true;
				M1_LOG_D(HTTP_TAG, "DNS OK: %.64s\n\r", s_at_buf);
				break;
			}

			M1_LOG_W(HTTP_TAG, "DNS attempt %d failed for %s: %.64s\n\r",
			         dns_try + 1, host, s_at_buf);
		}

		if (!dns_ok)
		{
			M1_LOG_E(HTTP_TAG, "DNS failed for %s after %d attempts\n\r",
			         host, dns_attempts);
			return HTTP_ERR_DNS_FAIL;
		}
	}

	for (int attempt = 0; attempt < max_attempts; attempt++)
	{
		if (attempt > 0)
		{
			M1_LOG_W(HTTP_TAG, "SSL retry %d/%d\n\r", attempt + 1, max_attempts);
			vTaskDelay(pdMS_TO_TICKS(SSL_CONNECT_RETRY_DELAY_MS));
			m1_wdt_reset();
			/* Force SSL reconfiguration on retry — the previous attempt
			 * may have left the ESP32 in an inconsistent SSL state. */
			s_ssl_configured = false;
		}

		/* For SSL: configure SNTP + disable cert verification */
		if (is_https)
			ssl_ensure_configured();

		/* Close any existing connection before opening a new one.
		 * In single-connection mode (CIPMUX=0), a stale connection
		 * causes AT+CIPSTART to fail with "ALREADY CONNECTED". */
		tcp_close();

		/* Open connection */
		snprintf(cmd, sizeof(cmd), ESP32C6_AT_REQ_CIPSTART "\"%s\",\"%s\",%u" ESP32C6_AT_REQ_CRLF,
		         is_https ? "SSL" : "TCP", host, port);

		memset(s_at_buf, 0, sizeof(s_at_buf));
		uint8_t ret = spi_AT_send_recv(cmd, s_at_buf, sizeof(s_at_buf), timeout_sec);
		if (ret != 0)
		{
			M1_LOG_W(HTTP_TAG, "CIPSTART send error (attempt %d): ret=%d\n\r",
			         attempt + 1, ret);
			continue;
		}

		if (strstr(s_at_buf, ESP32C6_AT_RES_CONNECT) || strstr(s_at_buf, "OK"))
			return HTTP_OK;

		/* Detect response timeout — spi_AT_send_recv returns SUCCESS
		 * but places "TIMEOUT(...)" in the buffer when the ESP32 does
		 * not respond within the deadline.  Report as HTTP_ERR_TIMEOUT
		 * so the caller gets an accurate error instead of CONNECT_FAIL. */
		if (strstr(s_at_buf, "TIMEOUT"))
		{
			M1_LOG_W(HTTP_TAG, "CIPSTART timed out (attempt %d)\n\r", attempt + 1);
			if (attempt == max_attempts - 1)
				return HTTP_ERR_TIMEOUT;
			continue;
		}

		M1_LOG_W(HTTP_TAG, "CIPSTART failed (attempt %d): %.80s\n\r",
		         attempt + 1, s_at_buf);
	}

	return HTTP_ERR_CONNECT_FAIL;
}

/*
 * Close the current TCP/SSL connection.
 */
static void tcp_close(void)
{
	char cmd[32];
	snprintf(cmd, sizeof(cmd), ESP32C6_AT_REQ_CIPCLOSE ESP32C6_AT_REQ_CRLF);
	spi_AT_send_recv(cmd, s_at_buf, sizeof(s_at_buf), 5);
}

/*
 * Send data on the current TCP connection.
 * Uses AT+CIPSEND=<len> then sends data after ">" prompt.
 */
static http_status_t tcp_send(const char *data, uint16_t len, uint8_t timeout_sec)
{
	char cmd[32];

	/* Tell ESP32 how much data we'll send */
	snprintf(cmd, sizeof(cmd), ESP32C6_AT_REQ_CIPSEND "%u" ESP32C6_AT_REQ_CRLF, len);

	memset(s_at_buf, 0, sizeof(s_at_buf));
	uint8_t ret = spi_AT_send_recv(cmd, s_at_buf, sizeof(s_at_buf), timeout_sec);
	if (ret != 0)
		return HTTP_ERR_TIMEOUT;

	/* ESP32 should respond with ">" prompt — now send the actual data */
	memset(s_at_buf, 0, sizeof(s_at_buf));
	ret = spi_AT_send_recv(data, s_at_buf, sizeof(s_at_buf), timeout_sec);
	if (ret != 0)
		return HTTP_ERR_TIMEOUT;

	if (strstr(s_at_buf, ESP32C6_AT_RES_SEND_OK))
		return HTTP_OK;

	return HTTP_ERR_SEND_FAIL;
}

/*
 * Read data from TCP connection using passive receive.
 * Returns bytes actually read into buf.
 */
static int tcp_recv(char *buf, uint16_t max_len, uint8_t timeout_sec)
{
	char cmd[32];
	char *p;
	int actual_len;

	snprintf(cmd, sizeof(cmd), ESP32C6_AT_REQ_CIPRECVDATA "%u" ESP32C6_AT_REQ_CRLF, max_len);

	memset(s_at_recv_buf, 0, sizeof(s_at_recv_buf));
	uint8_t ret = spi_AT_send_recv(cmd, s_at_recv_buf, sizeof(s_at_recv_buf), timeout_sec);
	if (ret != 0)
		return -1;

	/* Parse +CIPRECVDATA:<actual_len>,<data> */
	p = strstr(s_at_recv_buf, ESP32C6_AT_RES_CIPRECVDATA_KEY);
	if (!p)
		return 0; /* no data available */

	p += strlen(ESP32C6_AT_RES_CIPRECVDATA_KEY);
	actual_len = atoi(p);
	if (actual_len <= 0)
		return 0;

	/* Find comma separator */
	p = strchr(p, ',');
	if (!p) return 0;
	p++; /* past comma */

	if (actual_len > max_len)
		actual_len = max_len;

	memcpy(buf, p, actual_len);
	return actual_len;
}

/*
 * Query how many bytes are available to read in the ESP32 receive buffer.
 */
static int tcp_recv_available(uint8_t timeout_sec)
{
	char *p;
	memset(s_at_recv_buf, 0, sizeof(s_at_recv_buf));
	uint8_t ret = spi_AT_send_recv(ESP32C6_AT_REQ_CIPRECVLEN ESP32C6_AT_REQ_CRLF,
	                                s_at_recv_buf, sizeof(s_at_recv_buf), timeout_sec);
	if (ret != 0) return -1;

	p = strstr(s_at_recv_buf, ESP32C6_AT_RES_CIPRECVLEN_KEY);
	if (!p) return -1;

	p += strlen(ESP32C6_AT_RES_CIPRECVLEN_KEY);
	return atoi(p);
}

/*
 * Wait for data to become available, with timeout.
 * Returns bytes available, or 0 on timeout.
 */
static int tcp_wait_data(uint8_t timeout_sec)
{
	int avail;
	for (int i = 0; i < timeout_sec * 2; i++)
	{
		m1_wdt_reset();
		avail = tcp_recv_available(2);
		if (avail > 0)
			return avail;
		vTaskDelay(pdMS_TO_TICKS(500));
	}
	return 0;
}

/*
 * Accumulate HTTP response headers in s_at_buf.
 *
 * SSL may fragment the HTTP response across multiple records, so the
 * first tcp_recv may return only a partial header.  This helper reads
 * additional data until the header terminator \r\n\r\n is found or
 * timeout_sec expires.
 *
 * tcp_recv uses s_at_recv_buf internally, so s_at_buf is preserved
 * across calls and safe to accumulate into.
 *
 * initial_len: bytes already in s_at_buf from the first tcp_recv.
 * timeout_sec: maximum time to spend accumulating.
 * Returns: updated total length in s_at_buf (>= initial_len).
 */
static int tcp_recv_headers(int initial_len, uint8_t timeout_sec)
{
	if (strstr(s_at_buf, "\r\n\r\n"))
		return initial_len; /* headers already complete */

	M1_LOG_D(HTTP_TAG, "Headers incomplete after %d bytes, accumulating\n\r", initial_len);
	uint32_t start_tick = HAL_GetTick();
	uint32_t timeout_ms = (uint32_t)timeout_sec * 1000U;

	while (!strstr(s_at_buf, "\r\n\r\n") &&
	       initial_len < (int)(sizeof(s_at_buf) - 1))
	{
		if ((HAL_GetTick() - start_tick) >= timeout_ms)
			break;
		m1_wdt_reset();
		vTaskDelay(pdMS_TO_TICKS(100));

		int space = (int)(sizeof(s_at_buf) - 1) - initial_len;
		int to_read = space > HTTP_DOWNLOAD_CHUNK ? HTTP_DOWNLOAD_CHUNK : space;
		int received = tcp_recv(s_at_buf + initial_len, to_read, 2);
		if (received > 0)
		{
			initial_len += received;
			s_at_buf[initial_len] = '\0';
		}
	}

	return initial_len;
}

/*
 * Parse HTTP response headers from raw data.
 * Extracts status code, Content-Length, and Location (for redirects).
 * Modifies the buffer to track consumed data.
 *
 * Returns HTTP status code (200, 301, 302, 404, etc.) or 0 on error.
 */
static int parse_http_headers(const char *data, int data_len __attribute__((unused)),
                               uint32_t *content_length, char *location, uint16_t loc_size,
                               int *header_end_offset, bool *is_chunked)
{
	const char *hdr_end;
	const char *p;
	int status_code = 0;

	*content_length = 0;
	*header_end_offset = 0;
	if (is_chunked) *is_chunked = false;
	if (location) location[0] = '\0';

	/* Find end of headers */
	hdr_end = strstr(data, "\r\n\r\n");
	if (!hdr_end)
		return 0; /* headers incomplete */

	*header_end_offset = (hdr_end - data) + 4;

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

http_status_t http_download_to_file(const char *url, const char *sd_path,
                                     http_progress_cb_t progress_cb, uint8_t timeout_sec)
{
	char host[128];
	char path[256];
	char request[512];
	char location[256];
	char recv_buf[HTTP_DOWNLOAD_CHUNK];
	uint16_t port;
	bool is_https;
	http_status_t status;
	FIL file;
	UINT bw;
	uint32_t content_length = 0;
	uint32_t downloaded = 0;
	int header_end_offset = 0;
	int status_code;
	int redirects = 0;
	const char *current_url = url;
	/* Writable copy for redirect URLs */
	char redirect_url[256];

	if (!url || !sd_path)
		return HTTP_ERR_INVALID_ARG;

	if (!wifi_is_connected())
		return HTTP_ERR_NO_WIFI;
	if (!m1_esp32_get_init_status() || !get_esp32_main_init_status())
		return HTTP_ERR_ESP_NOT_READY;

	if (!timeout_sec)
		timeout_sec = HTTP_DEFAULT_TIMEOUT;

retry_with_redirect:
	if (redirects > HTTP_MAX_REDIRECTS)
		return HTTP_ERR_REDIRECT_LOOP;

	/* Parse the URL */
	if (!parse_url(current_url, host, sizeof(host), &port, path, sizeof(path), &is_https))
		return HTTP_ERR_INVALID_ARG;

	/* Open TCP/SSL connection */
	status = tcp_connect(host, port, is_https, timeout_sec);
	if (status != HTTP_OK)
		return status;

	/* Build and send HTTP GET request.
	 * Use HTTP/1.0 to prevent chunked transfer encoding — binary data
	 * mixed with chunk-size markers would corrupt the downloaded file. */
	snprintf(request, sizeof(request),
	         "GET %s HTTP/1.0\r\n"
	         "Host: %s\r\n"
	         "User-Agent: M1-Hapax/1.0\r\n"
	         "Accept: */*\r\n"
	         "Connection: close\r\n"
	         "\r\n",
	         path, host);

	status = tcp_send(request, strlen(request), timeout_sec);
	if (status != HTTP_OK)
	{
		tcp_close();
		return status;
	}

	/* Wait for response headers */
	if (tcp_wait_data(timeout_sec) <= 0)
	{
		tcp_close();
		return HTTP_ERR_TIMEOUT;
	}

	m1_wdt_reset();

	/* Read initial data (headers + possibly some body).
	 * Cap to HTTP_DOWNLOAD_CHUNK to leave room for AT response overhead. */
	int initial_len = tcp_recv(s_at_buf, HTTP_DOWNLOAD_CHUNK, timeout_sec);
	if (initial_len <= 0)
	{
		tcp_close();
		return HTTP_ERR_TIMEOUT;
	}
	s_at_buf[initial_len] = '\0';

	/* Accumulate more data if headers are incomplete (SSL fragmentation). */
	initial_len = tcp_recv_headers(initial_len, timeout_sec);

	/* Parse HTTP headers */
	bool dl_chunked = false;
	status_code = parse_http_headers(s_at_buf, initial_len,
	                                  &content_length, location, sizeof(location),
	                                  &header_end_offset, &dl_chunked);

	if (status_code == 0)
	{
		tcp_close();
		return HTTP_ERR_PARSE_FAIL;
	}

	/* Chunked transfer encoding is not supported for file downloads.
	 * HTTP/1.0 should prevent it, but if a non-compliant server sends
	 * chunked anyway, fail fast rather than writing corrupt data. */
	if (dl_chunked)
	{
		tcp_close();
		return HTTP_ERR_PARSE_FAIL;
	}

	/* Handle redirects */
	if (status_code >= 300 && status_code < 400 && location[0])
	{
		tcp_close();
		redirects++;
		strncpy(redirect_url, location, sizeof(redirect_url) - 1);
		redirect_url[sizeof(redirect_url) - 1] = '\0';
		current_url = redirect_url;
		goto retry_with_redirect;
	}

	/* Check for HTTP errors */
	if (status_code < 200 || status_code >= 300)
	{
		tcp_close();
		return HTTP_ERR_HTTP_ERROR;
	}

	/* Open output file on SD card */
	FRESULT fr = f_open(&file, sd_path, FA_CREATE_ALWAYS | FA_WRITE);
	if (fr != FR_OK)
	{
		tcp_close();
		return HTTP_ERR_SD_OPEN_FAIL;
	}

	/* Write any body data that arrived with the headers */
	int body_in_first = initial_len - header_end_offset;
	if (header_end_offset > initial_len)
		body_in_first = 0; /* malformed headers — no body data in first chunk */
	if (body_in_first > 0)
	{
		fr = f_write(&file, s_at_buf + header_end_offset, body_in_first, &bw);
		if (fr != FR_OK || bw != (UINT)body_in_first)
		{
			f_close(&file);
			tcp_close();
			f_unlink(sd_path); /* delete partial file */
			return HTTP_ERR_SD_WRITE_FAIL;
		}
		downloaded += body_in_first;
	}

	/* Report initial progress */
	if (progress_cb)
	{
		if (!progress_cb(downloaded, content_length))
		{
			f_close(&file);
			tcp_close();
			f_unlink(sd_path);
			return HTTP_ERR_CANCELLED;
		}
	}

	/* Stream remaining body data */
	int retry_count = 0;
	while (content_length == 0 || downloaded < content_length)
	{
		m1_wdt_reset();

		int avail = tcp_recv_available(2);
		if (avail <= 0)
		{
			/* If connection closed and we have content_length, check if done */
			if (content_length > 0 && downloaded >= content_length)
				break;

			/* Wait a bit and retry */
			vTaskDelay(pdMS_TO_TICKS(200));
			retry_count++;
			if (retry_count > timeout_sec * HTTP_RETRY_PER_SEC)
				break; /* timeout */
			continue;
		}

		retry_count = 0;
		int to_read = avail;
		if (to_read > HTTP_DOWNLOAD_CHUNK)
			to_read = HTTP_DOWNLOAD_CHUNK;

		int received = tcp_recv(recv_buf, to_read, timeout_sec);
		if (received <= 0)
			break;

		fr = f_write(&file, recv_buf, received, &bw);
		if (fr != FR_OK || bw != (UINT)received)
		{
			f_close(&file);
			tcp_close();
			f_unlink(sd_path);
			return HTTP_ERR_SD_WRITE_FAIL;
		}

		downloaded += received;

		if (progress_cb)
		{
			if (!progress_cb(downloaded, content_length))
			{
				f_close(&file);
				tcp_close();
				f_unlink(sd_path);
				return HTTP_ERR_CANCELLED;
			}
		}
	}

	f_close(&file);
	tcp_close();

	/* Verify we got all the data */
	if (content_length > 0 && downloaded < content_length)
	{
		f_unlink(sd_path); /* delete partial file */
		return HTTP_ERR_TIMEOUT;
	}

	return HTTP_OK;
}
