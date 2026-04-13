/*
 * test_http_client_parse.c — Unit tests for HTTP client URL parsing.
 *
 * Tests the parse_url() function (via http_parse_url stub) which determines
 * the connection type (SSL vs TCP), host, port, and path from a URL.
 * Getting is_https right is critical for OTA: HTTPS URLs must trigger the
 * SSL connection path, which requires SNTP + CIPSSLCCONF configuration.
 *
 * Bug context: OTA firmware download always failed with "Connection failed"
 * because tcp_connect() opened AT+CIPSTART="SSL" without first configuring
 * SSL certificate verification mode (AT+CIPSSLCCONF=0) or SNTP time sync.
 */

#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Stub implementation of parse_url in tests/stubs/http_client_parse_impl.c */
extern bool http_parse_url(const char *url, char *host, uint16_t host_size,
                           uint16_t *port, char *path, uint16_t path_size,
                           bool *is_https);

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
	return UNITY_END();
}
