/*
 * test_http_client_parse.c — Unit tests for HTTP client URL parsing and
 * error string mapping.
 *
 * Tests the parse_url() function (via http_parse_url stub) which determines
 * the connection type (SSL vs TCP), host, port, and path from a URL.
 * Getting is_https right is critical for OTA: HTTPS URLs must trigger the
 * SSL connection path, which requires SNTP + CIPSSLCCONF configuration.
 *
 * Also tests http_status_str() to verify all error codes map to the correct
 * user-facing strings.  The DNS_FAIL case is particularly important: before
 * the fix, tcp_connect() returned CONNECT_FAIL for all failures including
 * DNS; now DNS failures are distinguished via AT+CIPDOMAIN pre-resolution.
 *
 * Bug context: OTA firmware download always failed with "Connection failed"
 * because tcp_connect() opened AT+CIPSTART="SSL" without first configuring
 * SSL certificate verification mode (AT+CIPSSLCCONF=0) or SNTP time sync,
 * and without retrying, cleaning up stale connections, or checking DNS.
 */

#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

/* Stub implementation of parse_url in tests/stubs/http_client_parse_impl.c */
extern bool http_parse_url(const char *url, char *host, uint16_t host_size,
                           uint16_t *port, char *path, uint16_t path_size,
                           bool *is_https);

/* HTTP status codes — must match m1_http_client.h */
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

/* Stub implementation of http_status_str in tests/stubs/http_client_parse_impl.c */
extern const char *http_status_str(http_status_t status);

void setUp(void) {}
void tearDown(void) {}

/* ---- GitHub API URL (triggers OTA release fetch) ---- */

void test_github_api_url_is_https(void)
{
	char host[128], path[256];
	uint16_t port;
	bool is_https = false;

	bool ok = http_parse_url(
		"https://api.github.com/repos/hapaxx11/M1/releases?per_page=5",
		host, sizeof(host), &port, path, sizeof(path), &is_https);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_TRUE(is_https);
	TEST_ASSERT_EQUAL_STRING("api.github.com", host);
	TEST_ASSERT_EQUAL_UINT16(443, port);
	TEST_ASSERT_EQUAL_STRING("/repos/hapaxx11/M1/releases?per_page=5", path);
}

/* ---- GitHub release download URL (redirect target) ---- */

void test_github_download_url(void)
{
	char host[128], path[256];
	uint16_t port;
	bool is_https = false;

	bool ok = http_parse_url(
		"https://objects.githubusercontent.com/github-production-release-asset/123/file.bin",
		host, sizeof(host), &port, path, sizeof(path), &is_https);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_TRUE(is_https);
	TEST_ASSERT_EQUAL_STRING("objects.githubusercontent.com", host);
	TEST_ASSERT_EQUAL_UINT16(443, port);
}

/* ---- HTTP URL sets is_https=false ---- */

void test_http_url_not_https(void)
{
	char host[128], path[256];
	uint16_t port;
	bool is_https = true; /* should be set to false */

	bool ok = http_parse_url(
		"http://example.com/firmware.bin",
		host, sizeof(host), &port, path, sizeof(path), &is_https);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_FALSE(is_https);
	TEST_ASSERT_EQUAL_STRING("example.com", host);
	TEST_ASSERT_EQUAL_UINT16(80, port);
	TEST_ASSERT_EQUAL_STRING("/firmware.bin", path);
}

/* ---- Custom port ---- */

void test_custom_port(void)
{
	char host[128], path[256];
	uint16_t port;
	bool is_https;

	bool ok = http_parse_url(
		"https://myserver.local:8443/api/releases",
		host, sizeof(host), &port, path, sizeof(path), &is_https);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_TRUE(is_https);
	TEST_ASSERT_EQUAL_STRING("myserver.local", host);
	TEST_ASSERT_EQUAL_UINT16(8443, port);
	TEST_ASSERT_EQUAL_STRING("/api/releases", path);
}

/* ---- No path defaults to "/" ---- */

void test_no_path_defaults_to_root(void)
{
	char host[128], path[256];
	uint16_t port;
	bool is_https;

	bool ok = http_parse_url(
		"https://api.github.com",
		host, sizeof(host), &port, path, sizeof(path), &is_https);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_EQUAL_STRING("api.github.com", host);
	TEST_ASSERT_EQUAL_STRING("/", path);
}

/* ---- Invalid scheme rejected ---- */

void test_invalid_scheme_rejected(void)
{
	char host[128], path[256];
	uint16_t port;
	bool is_https;

	TEST_ASSERT_FALSE(http_parse_url(
		"ftp://example.com/file",
		host, sizeof(host), &port, path, sizeof(path), &is_https));
}

/* ---- Empty host rejected ---- */

void test_empty_host_rejected(void)
{
	char host[128], path[256];
	uint16_t port;
	bool is_https;

	TEST_ASSERT_FALSE(http_parse_url(
		"https:///path",
		host, sizeof(host), &port, path, sizeof(path), &is_https));
}

/* ---- Host too long for buffer ---- */

void test_host_buffer_overflow_rejected(void)
{
	char host[8]; /* too small for "api.github.com" */
	char path[256];
	uint16_t port;
	bool is_https;

	TEST_ASSERT_FALSE(http_parse_url(
		"https://api.github.com/repos/test",
		host, sizeof(host), &port, path, sizeof(path), &is_https));
}

/* ---- Monstatek Official release URL (real OTA scenario) ---- */

void test_monstatek_release_url(void)
{
	char host[128], path[256];
	uint16_t port;
	bool is_https = false;

	bool ok = http_parse_url(
		"https://api.github.com/repos/Monstatek/M1/releases?per_page=5",
		host, sizeof(host), &port, path, sizeof(path), &is_https);

	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_TRUE(is_https);
	TEST_ASSERT_EQUAL_STRING("api.github.com", host);
	TEST_ASSERT_EQUAL_UINT16(443, port);
}

/* ---- http_status_str: error string mapping ---- */

void test_status_str_ok_returns_null(void)
{
	TEST_ASSERT_NULL(http_status_str(HTTP_OK));
}

void test_status_str_dns_fail(void)
{
	/* DNS_FAIL is now returned by tcp_connect() when AT+CIPDOMAIN fails.
	 * Before the fix, DNS failures were reported as CONNECT_FAIL. */
	const char *s = http_status_str(HTTP_ERR_DNS_FAIL);
	TEST_ASSERT_NOT_NULL(s);
	TEST_ASSERT_EQUAL_STRING("DNS lookup failed", s);
}

void test_status_str_connect_fail(void)
{
	const char *s = http_status_str(HTTP_ERR_CONNECT_FAIL);
	TEST_ASSERT_NOT_NULL(s);
	TEST_ASSERT_EQUAL_STRING("Connection failed", s);
}

void test_status_str_all_errors_have_strings(void)
{
	/* Every defined error code must map to a non-NULL string.
	 * If a new error code is added without updating http_status_str(),
	 * this test will catch it. */
	http_status_t codes[] = {
		HTTP_ERR_NO_WIFI, HTTP_ERR_ESP_NOT_READY, HTTP_ERR_DNS_FAIL,
		HTTP_ERR_CONNECT_FAIL, HTTP_ERR_SEND_FAIL, HTTP_ERR_TIMEOUT,
		HTTP_ERR_HTTP_ERROR, HTTP_ERR_REDIRECT_LOOP,
		HTTP_ERR_RESPONSE_TOO_LARGE, HTTP_ERR_SD_WRITE_FAIL,
		HTTP_ERR_SD_OPEN_FAIL, HTTP_ERR_CANCELLED,
		HTTP_ERR_INVALID_ARG, HTTP_ERR_PARSE_FAIL,
	};
	for (int i = 0; i < (int)(sizeof(codes) / sizeof(codes[0])); i++)
	{
		const char *s = http_status_str(codes[i]);
		TEST_ASSERT_NOT_NULL_MESSAGE(s, "Error code missing from http_status_str()");
	}
}

void test_status_str_dns_differs_from_connect(void)
{
	/* DNS_FAIL and CONNECT_FAIL must produce different strings so the
	 * user can distinguish "no DNS" from "SSL handshake failed". */
	const char *dns_str = http_status_str(HTTP_ERR_DNS_FAIL);
	const char *conn_str = http_status_str(HTTP_ERR_CONNECT_FAIL);
	TEST_ASSERT_NOT_NULL(dns_str);
	TEST_ASSERT_NOT_NULL(conn_str);
	TEST_ASSERT_TRUE(strcmp(dns_str, conn_str) != 0);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_github_api_url_is_https);
	RUN_TEST(test_github_download_url);
	RUN_TEST(test_http_url_not_https);
	RUN_TEST(test_custom_port);
	RUN_TEST(test_no_path_defaults_to_root);
	RUN_TEST(test_invalid_scheme_rejected);
	RUN_TEST(test_empty_host_rejected);
	RUN_TEST(test_host_buffer_overflow_rejected);
	RUN_TEST(test_monstatek_release_url);
	RUN_TEST(test_status_str_ok_returns_null);
	RUN_TEST(test_status_str_dns_fail);
	RUN_TEST(test_status_str_connect_fail);
	RUN_TEST(test_status_str_all_errors_have_strings);
	RUN_TEST(test_status_str_dns_differs_from_connect);
	return UNITY_END();
}
