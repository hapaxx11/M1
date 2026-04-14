/*
 * http_status_defs.h — Shared http_status_t enum for host-side tests.
 *
 * Single source of truth for the enum used by both the test stub
 * (http_client_parse_impl.c) and the test file (test_http_client_parse.c).
 * Must be kept in sync with m1_csrc/m1_http_client.h.
 */

#ifndef HTTP_STATUS_DEFS_H_
#define HTTP_STATUS_DEFS_H_

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

#endif /* HTTP_STATUS_DEFS_H_ */
