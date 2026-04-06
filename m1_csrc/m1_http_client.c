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

static char s_at_buf[HTTP_AT_BUF_SIZE];

bool http_is_ready(void)
{
#ifdef M1_APP_WIFI_CONNECT_ENABLE
	return wifi_is_connected() && get_esp32_main_init_status();
#else
	return false;
#endif
}

/*
 * http_get() — Small response HTTP GET using AT+HTTPCLIENT.
 *
 * AT+HTTPCLIENT=<opt>,<content-type>,"<url>"[,"<host>","<path>",<transport_type>]
 *   opt=2 (GET), content-type=0, transport_type=2 (HTTPS no cert verify)
 *
 * Response format:
 *   +HTTPCLIENT:<length>,<data>\r\n
 *   ...may repeat...
 *   \r\nOK\r\n
 */
http_status_t http_get(const char *url, char *response_buf, uint16_t buf_size, uint8_t timeout_sec)
{
	char cmd[512];
	uint8_t ret;
	char *p, *dst;
	int chunk_len;
	uint16_t total = 0;

	if (!url || !response_buf || buf_size < 2)
		return HTTP_ERR_INVALID_ARG;

	response_buf[0] = '\0';

	if (!http_is_ready())
		return HTTP_ERR_NO_WIFI;

	if (!timeout_sec)
		timeout_sec = HTTP_DEFAULT_TIMEOUT;

	/* Determine transport type: 1=HTTP, 2=HTTPS */
	int transport = 1;
	if (strncmp(url, "https://", 8) == 0)
		transport = 2;

	/* Build AT command: GET, content-type=0 (not used for GET) */
	snprintf(cmd, sizeof(cmd),
	         ESP32C6_AT_REQ_HTTPCLIENT "2,0,\"%s\",,,,%d" ESP32C6_AT_REQ_CRLF,
	         url, transport);

	memset(s_at_buf, 0, sizeof(s_at_buf));
	ret = spi_AT_send_recv(cmd, s_at_buf, sizeof(s_at_buf), timeout_sec);
	if (ret != 0)
		return HTTP_ERR_TIMEOUT;

	/* Check for ERROR response */
	if (strstr(s_at_buf, "ERROR"))
		return HTTP_ERR_HTTP_ERROR;

	/* Parse +HTTPCLIENT:<len>,<data> lines and concatenate data */
	dst = response_buf;
	p = s_at_buf;

	while ((p = strstr(p, ESP32C6_AT_RES_HTTPCLIENT_KEY)) != NULL)
	{
		p += strlen(ESP32C6_AT_RES_HTTPCLIENT_KEY);

		/* Parse chunk length */
		chunk_len = atoi(p);
		if (chunk_len <= 0)
			break;

		/* Find comma after length */
		p = strchr(p, ',');
		if (!p) break;
		p++; /* past comma */

		/* Copy data up to chunk_len or remaining buffer */
		int copy_len = chunk_len;
		if (total + copy_len >= buf_size - 1)
			copy_len = buf_size - 1 - total;
		if (copy_len <= 0)
			break;

		memcpy(dst + total, p, copy_len);
		total += copy_len;
		p += chunk_len;
	}

	response_buf[total] = '\0';

	return (total > 0) ? HTTP_OK : HTTP_ERR_PARSE_FAIL;
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
 * Open a TCP/SSL connection to host:port via ESP32 AT.
 */
static http_status_t tcp_connect(const char *host, uint16_t port, bool is_https, uint8_t timeout_sec)
{
	char cmd[256];

	/* Set passive receive mode first */
	snprintf(cmd, sizeof(cmd), ESP32C6_AT_REQ_CIPRECVMODE "1" ESP32C6_AT_REQ_CRLF);
	spi_AT_send_recv(cmd, s_at_buf, sizeof(s_at_buf), 5);

	/* Open connection */
	snprintf(cmd, sizeof(cmd), ESP32C6_AT_REQ_CIPSTART "\"%s\",\"%s\",%u" ESP32C6_AT_REQ_CRLF,
	         is_https ? "SSL" : "TCP", host, port);

	memset(s_at_buf, 0, sizeof(s_at_buf));
	uint8_t ret = spi_AT_send_recv(cmd, s_at_buf, sizeof(s_at_buf), timeout_sec);
	if (ret != 0)
		return HTTP_ERR_TIMEOUT;

	if (strstr(s_at_buf, ESP32C6_AT_RES_CONNECT) || strstr(s_at_buf, "OK"))
		return HTTP_OK;

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

	memset(s_at_buf, 0, sizeof(s_at_buf));
	uint8_t ret = spi_AT_send_recv(cmd, s_at_buf, sizeof(s_at_buf), timeout_sec);
	if (ret != 0)
		return -1;

	/* Parse +CIPRECVDATA:<actual_len>,<data> */
	p = strstr(s_at_buf, ESP32C6_AT_RES_CIPRECVDATA_KEY);
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
	memset(s_at_buf, 0, sizeof(s_at_buf));
	uint8_t ret = spi_AT_send_recv(ESP32C6_AT_REQ_CIPRECVLEN ESP32C6_AT_REQ_CRLF,
	                                s_at_buf, sizeof(s_at_buf), timeout_sec);
	if (ret != 0) return -1;

	p = strstr(s_at_buf, ESP32C6_AT_RES_CIPRECVLEN_KEY);
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
 * Parse HTTP response headers from raw data.
 * Extracts status code, Content-Length, and Location (for redirects).
 * Modifies the buffer to track consumed data.
 *
 * Returns HTTP status code (200, 301, 302, 404, etc.) or 0 on error.
 */
static int parse_http_headers(const char *data, int data_len __attribute__((unused)),
                               uint32_t *content_length, char *location, uint16_t loc_size,
                               int *header_end_offset)
{
	const char *hdr_end;
	const char *p;
	int status_code = 0;

	*content_length = 0;
	*header_end_offset = 0;
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

	if (!http_is_ready())
		return HTTP_ERR_NO_WIFI;

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

	/* Build and send HTTP GET request */
	snprintf(request, sizeof(request),
	         "GET %s HTTP/1.1\r\n"
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

	/* Read initial data (headers + possibly some body) */
	int initial_len = tcp_recv(s_at_buf, sizeof(s_at_buf) - 1, timeout_sec);
	if (initial_len <= 0)
	{
		tcp_close();
		return HTTP_ERR_TIMEOUT;
	}
	s_at_buf[initial_len] = '\0';

	/* Parse HTTP headers */
	status_code = parse_http_headers(s_at_buf, initial_len,
	                                  &content_length, location, sizeof(location),
	                                  &header_end_offset);

	if (status_code == 0)
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
