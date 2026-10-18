/* See COPYING.txt for license details. */

#include "m1_http_client.h"
#include <stddef.h>

bool http_is_ready(void)
{
	return false;
}

http_status_t http_get(const char *url, char *response_buf, uint16_t buf_size,
                       uint8_t timeout_sec)
{
	(void)timeout_sec;
	if (!url || !response_buf || buf_size < 2u)
		return HTTP_ERR_INVALID_ARG;
	response_buf[0] = '\0';
	return HTTP_ERR_UNSUPPORTED_TRANSPORT;
}

http_status_t http_download_to_file(const char *url, const char *sd_path,
                                    http_progress_cb_t progress_cb,
                                    uint8_t timeout_sec)
{
	(void)progress_cb;
	(void)timeout_sec;
	if (!url || !sd_path || sd_path[0] == '\0')
		return HTTP_ERR_INVALID_ARG;
	return HTTP_ERR_UNSUPPORTED_TRANSPORT;
}

const char *http_status_str(http_status_t status)
{
	switch (status)
	{
		case HTTP_OK: return NULL;
		case HTTP_ERR_NO_WIFI: return "WiFi not connected";
		case HTTP_ERR_ESP_NOT_READY: return "ESP32 not ready";
		case HTTP_ERR_DNS_FAIL: return "DNS lookup failed";
		case HTTP_ERR_CONNECT_FAIL: return "Connection failed";
		case HTTP_ERR_SEND_FAIL: return "Request send failed";
		case HTTP_ERR_TIMEOUT: return "Server timeout";
		case HTTP_ERR_HTTP_ERROR: return "Server error (HTTP)";
		case HTTP_ERR_REDIRECT_LOOP: return "Too many redirects";
		case HTTP_ERR_RESPONSE_TOO_LARGE: return "Response too large";
		case HTTP_ERR_SD_WRITE_FAIL: return "SD card write error";
		case HTTP_ERR_SD_OPEN_FAIL: return "Cannot open file";
		case HTTP_ERR_CANCELLED: return "Cancelled";
		case HTTP_ERR_INVALID_ARG: return "Internal request error";
		case HTTP_ERR_PARSE_FAIL: return "Bad response";
		case HTTP_ERR_UNSUPPORTED_TRANSPORT:
			return "Network downloads unsupported";
		default: return NULL;
	}
}
