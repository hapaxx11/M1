/* See COPYING.txt for license details. */

#include "unity.h"
#include "m1_http_client.h"

void setUp(void) {}
void tearDown(void) {}

void test_http_get_fails_closed_for_unsupported_transport(void)
{
	char response[8] = "stale";

	TEST_ASSERT_EQUAL(HTTP_ERR_UNSUPPORTED_TRANSPORT,
	                  http_get("https://example.test/", response,
	                           sizeof(response), 1u));
	TEST_ASSERT_EQUAL_CHAR('\0', response[0]);
}

void test_http_download_fails_closed_for_unsupported_transport(void)
{
	TEST_ASSERT_EQUAL(HTTP_ERR_UNSUPPORTED_TRANSPORT,
	                  http_download_to_file("https://example.test/file.bin",
	                                        "0:/file.bin", NULL, 1u));
}

void test_http_api_validates_arguments(void)
{
	char response[8];

	TEST_ASSERT_EQUAL(HTTP_ERR_INVALID_ARG,
	                  http_get(NULL, response, sizeof(response), 1u));
	TEST_ASSERT_EQUAL(HTTP_ERR_INVALID_ARG,
	                  http_download_to_file(NULL, "0:/file.bin", NULL, 1u));
}

void test_http_is_not_ready_without_supported_transport(void)
{
	TEST_ASSERT_FALSE(http_is_ready());
	TEST_ASSERT_EQUAL_STRING("Network downloads unsupported",
	                         http_status_str(HTTP_ERR_UNSUPPORTED_TRANSPORT));
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_http_get_fails_closed_for_unsupported_transport);
	RUN_TEST(test_http_download_fails_closed_for_unsupported_transport);
	RUN_TEST(test_http_api_validates_arguments);
	RUN_TEST(test_http_is_not_ready_without_supported_transport);
	return UNITY_END();
}
