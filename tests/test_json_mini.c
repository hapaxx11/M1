/* See COPYING.txt for license details. */

/*
 * test_json_mini.c
 *
 * Regression tests for m1_json_mini.c — the minimal JSON parser used
 * for GitHub API release responses.
 *
 * Bug context:
 *   OTA firmware download always returned "No releases found" because:
 *   1. AT+HTTPCLIENT had a 2 KB buffer limit; responses were truncated.
 *   2. The HTTP format string placed the HTTPS transport-type in the wrong
 *      parameter slot, defaulting to plain HTTP (rejected by api.github.com).
 *   The JSON parser itself must correctly handle GitHub API payloads including
 *   nested objects, arrays, escaped strings, booleans, and integers.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "m1_json_mini.h"
#include <string.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * json_get_string
 * =================================================================== */

void test_json_get_string_simple(void)
{
	const char *json = "{\"name\":\"hello\"}";
	char buf[32];
	TEST_ASSERT_TRUE(json_get_string(json, "name", buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING("hello", buf);
}

void test_json_get_string_with_whitespace(void)
{
	const char *json = "{ \"tag_name\" : \"v0.9.0.5\" }";
	char buf[32];
	TEST_ASSERT_TRUE(json_get_string(json, "tag_name", buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING("v0.9.0.5", buf);
}

void test_json_get_string_escaped_quote(void)
{
	const char *json = "{\"desc\":\"line \\\"quoted\\\" text\"}";
	char buf[64];
	TEST_ASSERT_TRUE(json_get_string(json, "desc", buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING("line \"quoted\" text", buf);
}

void test_json_get_string_not_found(void)
{
	const char *json = "{\"name\":\"hello\"}";
	char buf[32];
	TEST_ASSERT_FALSE(json_get_string(json, "missing", buf, sizeof(buf)));
}

void test_json_get_string_null_inputs(void)
{
	char buf[32];
	TEST_ASSERT_FALSE(json_get_string(NULL, "key", buf, sizeof(buf)));
	TEST_ASSERT_FALSE(json_get_string("{}", NULL, buf, sizeof(buf)));
	TEST_ASSERT_FALSE(json_get_string("{}", "key", NULL, sizeof(buf)));
	TEST_ASSERT_FALSE(json_get_string("{}", "key", buf, 0));
	TEST_ASSERT_FALSE(json_get_string("{}", "key", buf, 1));
}

void test_json_get_string_truncation(void)
{
	const char *json = "{\"name\":\"abcdefghij\"}";
	char buf[6]; /* only room for 5 chars + NUL */
	TEST_ASSERT_TRUE(json_get_string(json, "name", buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING("abcde", buf);
}

void test_json_get_string_empty_value(void)
{
	const char *json = "{\"name\":\"\"}";
	char buf[32];
	/* Empty string value — json_get_string returns false since i==0 */
	TEST_ASSERT_FALSE(json_get_string(json, "name", buf, sizeof(buf)));
}

/* ===================================================================
 * json_get_int
 * =================================================================== */

void test_json_get_int_positive(void)
{
	const char *json = "{\"size\":12345}";
	int32_t val = 0;
	TEST_ASSERT_TRUE(json_get_int(json, "size", &val));
	TEST_ASSERT_EQUAL_INT32(12345, val);
}

void test_json_get_int_negative(void)
{
	const char *json = "{\"offset\":-42}";
	int32_t val = 0;
	TEST_ASSERT_TRUE(json_get_int(json, "offset", &val));
	TEST_ASSERT_EQUAL_INT32(-42, val);
}

void test_json_get_int_zero(void)
{
	const char *json = "{\"count\":0}";
	int32_t val = -1;
	TEST_ASSERT_TRUE(json_get_int(json, "count", &val));
	TEST_ASSERT_EQUAL_INT32(0, val);
}

void test_json_get_int_not_found(void)
{
	const char *json = "{\"size\":100}";
	int32_t val = 0;
	TEST_ASSERT_FALSE(json_get_int(json, "missing", &val));
}

void test_json_get_int_string_value(void)
{
	/* "size" is a string, not a number — should fail */
	const char *json = "{\"size\":\"big\"}";
	int32_t val = 0;
	TEST_ASSERT_FALSE(json_get_int(json, "size", &val));
}

void test_json_get_int_null_inputs(void)
{
	int32_t val;
	TEST_ASSERT_FALSE(json_get_int(NULL, "key", &val));
	TEST_ASSERT_FALSE(json_get_int("{}", NULL, &val));
	TEST_ASSERT_FALSE(json_get_int("{}", "key", NULL));
}

/* ===================================================================
 * json_get_bool
 * =================================================================== */

void test_json_get_bool_true(void)
{
	const char *json = "{\"prerelease\":true}";
	bool val = false;
	TEST_ASSERT_TRUE(json_get_bool(json, "prerelease", &val));
	TEST_ASSERT_TRUE(val);
}

void test_json_get_bool_false(void)
{
	const char *json = "{\"draft\":false}";
	bool val = true;
	TEST_ASSERT_TRUE(json_get_bool(json, "draft", &val));
	TEST_ASSERT_FALSE(val);
}

void test_json_get_bool_not_found(void)
{
	const char *json = "{\"draft\":false}";
	bool val;
	TEST_ASSERT_FALSE(json_get_bool(json, "missing", &val));
}

void test_json_get_bool_null_inputs(void)
{
	bool val;
	TEST_ASSERT_FALSE(json_get_bool(NULL, "key", &val));
	TEST_ASSERT_FALSE(json_get_bool("{}", NULL, &val));
	TEST_ASSERT_FALSE(json_get_bool("{}", "key", NULL));
}

/* ===================================================================
 * json_find_array / json_array_next
 * =================================================================== */

void test_json_find_array_basic(void)
{
	const char *json = "{\"assets\":[{\"name\":\"a\"},{\"name\":\"b\"}]}";
	const char *arr = json_find_array(json, "assets");
	TEST_ASSERT_NOT_NULL(arr);
	/* Should point at the first '{' inside the array */
	TEST_ASSERT_EQUAL_CHAR('{', *arr);
}

void test_json_find_array_empty(void)
{
	const char *json = "{\"items\":[]}";
	const char *arr = json_find_array(json, "items");
	TEST_ASSERT_NULL(arr); /* empty array returns NULL */
}

void test_json_find_array_not_found(void)
{
	const char *json = "{\"name\":\"test\"}";
	TEST_ASSERT_NULL(json_find_array(json, "missing"));
}

void test_json_array_next_iterates(void)
{
	const char *json = "{\"nums\":[1,2,3]}";
	const char *elem = json_find_array(json, "nums");
	TEST_ASSERT_NOT_NULL(elem);
	TEST_ASSERT_EQUAL_CHAR('1', *elem);

	elem = json_array_next(elem);
	TEST_ASSERT_NOT_NULL(elem);
	TEST_ASSERT_EQUAL_CHAR('2', *elem);

	elem = json_array_next(elem);
	TEST_ASSERT_NOT_NULL(elem);
	TEST_ASSERT_EQUAL_CHAR('3', *elem);

	elem = json_array_next(elem);
	TEST_ASSERT_NULL(elem); /* end of array */
}

/* ===================================================================
 * json_find_object
 * =================================================================== */

void test_json_find_object_basic(void)
{
	const char *json = "{\"author\":{\"login\":\"user1\"}}";
	const char *obj = json_find_object(json, "author");
	TEST_ASSERT_NOT_NULL(obj);
	TEST_ASSERT_EQUAL_CHAR('{', *obj);

	/* Can extract nested value */
	char buf[32];
	TEST_ASSERT_TRUE(json_get_string(obj, "login", buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING("user1", buf);
}

void test_json_find_object_not_found(void)
{
	const char *json = "{\"name\":\"test\"}";
	TEST_ASSERT_NULL(json_find_object(json, "missing"));
}

/* ===================================================================
 * json_object_end
 * =================================================================== */

void test_json_object_end_simple(void)
{
	const char *json = "{\"a\":1}rest";
	const char *end = json_object_end(json);
	TEST_ASSERT_NOT_NULL(end);
	TEST_ASSERT_EQUAL_CHAR('r', *end);
}

void test_json_object_end_nested(void)
{
	const char *json = "{\"a\":{\"b\":2}}after";
	const char *end = json_object_end(json);
	TEST_ASSERT_NOT_NULL(end);
	TEST_ASSERT_EQUAL_CHAR('a', *end);
}

void test_json_object_end_null(void)
{
	TEST_ASSERT_NULL(json_object_end(NULL));
	TEST_ASSERT_NULL(json_object_end("not_object"));
}

/* ===================================================================
 * GitHub API response format — integration-like test
 * Verifies the parser handles a realistic multi-release JSON array.
 * =================================================================== */

void test_json_github_release_payload(void)
{
	/* Simplified GitHub releases API response with 2 releases */
	const char *payload =
		"[{\"tag_name\":\"v0.9.0.5\",\"name\":\"M1 Hapax v0.9.0.5\","
		"\"prerelease\":true,\"draft\":false,"
		"\"assets\":[{\"name\":\"M1_Hapax_v0.9.0.5_wCRC.bin\","
		"\"browser_download_url\":\"https://github.com/hapaxx11/M1/releases/download/v0.9.0.5/M1_Hapax_v0.9.0.5_wCRC.bin\","
		"\"size\":1048576}]},"
		"{\"tag_name\":\"v0.9.0.4\",\"name\":\"M1 Hapax v0.9.0.4\","
		"\"prerelease\":false,\"draft\":false,"
		"\"assets\":[{\"name\":\"M1_Hapax_v0.9.0.4_wCRC.bin\","
		"\"browser_download_url\":\"https://example.com/v4.bin\","
		"\"size\":524288}]}]";

	/* Skip the opening '[' to get to first element */
	const char *elem = payload + 1;
	char buf[256];
	int32_t size_val;
	bool bval;

	/* First release */
	TEST_ASSERT_TRUE(json_get_string(elem, "tag_name", buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING("v0.9.0.5", buf);

	TEST_ASSERT_TRUE(json_get_string(elem, "name", buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING("M1 Hapax v0.9.0.5", buf);

	TEST_ASSERT_TRUE(json_get_bool(elem, "prerelease", &bval));
	TEST_ASSERT_TRUE(bval);

	TEST_ASSERT_TRUE(json_get_bool(elem, "draft", &bval));
	TEST_ASSERT_FALSE(bval);

	/* Navigate into assets array */
	const char *assets = json_find_array(elem, "assets");
	TEST_ASSERT_NOT_NULL(assets);

	TEST_ASSERT_TRUE(json_get_string(assets, "name", buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING("M1_Hapax_v0.9.0.5_wCRC.bin", buf);

	TEST_ASSERT_TRUE(json_get_int(assets, "size", &size_val));
	TEST_ASSERT_EQUAL_INT32(1048576, size_val);

	TEST_ASSERT_TRUE(json_get_string(assets, "browser_download_url", buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING(
		"https://github.com/hapaxx11/M1/releases/download/v0.9.0.5/M1_Hapax_v0.9.0.5_wCRC.bin",
		buf);
}

/* ===================================================================
 * Truncated response — buffer filled mid-release object
 * Verifies json_object_end returns NULL for incomplete objects,
 * preventing partial data from being misinterpreted as valid.
 * =================================================================== */

void test_json_truncated_release_object(void)
{
	/* Complete first release + truncated second release */
	const char *payload =
		"[{\"tag_name\":\"v0.9.0.5\","
		"\"assets\":[{\"name\":\"fw.bin\","
		"\"browser_download_url\":\"https://example.com/fw.bin\","
		"\"size\":100}]},"
		"{\"tag_name\":\"v0.9.0.4\","
		"\"assets\":[{\"name\":\"fw2.b";  /* truncated! */

	const char *p = payload;
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
	TEST_ASSERT_EQUAL_CHAR('[', *p);
	p++; /* past '[' */

	/* First object should be complete */
	TEST_ASSERT_EQUAL_CHAR('{', *p);
	const char *obj_end = json_object_end(p);
	TEST_ASSERT_NOT_NULL(obj_end);

	/* Can parse tag_name from first object */
	char buf[64];
	TEST_ASSERT_TRUE(json_get_string(p, "tag_name", buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING("v0.9.0.5", buf);

	/* Move past first object to second */
	p = obj_end;
	while (*p == ',' || *p == ' ' || *p == '\r' || *p == '\n') p++;

	/* Second object is truncated — json_object_end must return NULL */
	TEST_ASSERT_EQUAL_CHAR('{', *p);
	obj_end = json_object_end(p);
	TEST_ASSERT_NULL(obj_end); /* incomplete object — correctly detected */
}

/* ===================================================================
 * Chunked transfer encoding decoder
 *
 * Mirrors decode_chunked_body() in m1_http_client.c.
 * Extracted here for host-side testing (the full HTTP client has
 * hardware dependencies that prevent host compilation).
 * =================================================================== */

#include <stdlib.h>

static uint16_t decode_chunked_body(char *body, uint16_t body_len)
{
	char *src = body;
	char *dst = body;
	char *end = body + body_len;

	while (src < end)
	{
		while (src < end && (*src == '\r' || *src == '\n'))
			src++;
		if (src >= end)
			break;

		char *size_end = NULL;
		unsigned long chunk_size = strtoul(src, &size_end, 16);
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

		uint16_t avail = (uint16_t)(end - src);
		if (chunk_size > avail)
			chunk_size = avail;

		memmove(dst, src, chunk_size);
		dst += chunk_size;
		src += chunk_size;
	}

	*dst = '\0';
	return (uint16_t)(dst - body);
}

/* ===================================================================
 * Chunked decoder tests
 * =================================================================== */

void test_chunked_decode_single_chunk(void)
{
	/* "d\r\nHello, World!\r\n0\r\n\r\n" */
	char body[] = "d\r\nHello, World!\r\n0\r\n\r\n";
	uint16_t len = decode_chunked_body(body, (uint16_t)(sizeof(body) - 1));
	TEST_ASSERT_EQUAL_UINT16(13, len);
	TEST_ASSERT_EQUAL_STRING("Hello, World!", body);
}

void test_chunked_decode_multiple_chunks(void)
{
	/* Two chunks: "Hello" (5 bytes) + ", World!" (8 bytes) */
	char body[] = "5\r\nHello\r\n8\r\n, World!\r\n0\r\n\r\n";
	uint16_t len = decode_chunked_body(body, (uint16_t)(sizeof(body) - 1));
	TEST_ASSERT_EQUAL_UINT16(13, len);
	TEST_ASSERT_EQUAL_STRING("Hello, World!", body);
}

void test_chunked_decode_json_array(void)
{
	/* Simulates a chunked GitHub API response with JSON array */
	char body[512];
	const char *chunk_data = "[{\"tag_name\":\"v1.0\"}]";
	int chunk_len = (int)strlen(chunk_data);
	/* Build chunked body: hex_size\r\ndata\r\n0\r\n\r\n */
	snprintf(body, sizeof(body), "%x\r\n%s\r\n0\r\n\r\n", (unsigned)chunk_len, chunk_data);

	uint16_t decoded_len = decode_chunked_body(body, (uint16_t)strlen(body));
	TEST_ASSERT_EQUAL_UINT16((uint16_t)chunk_len, decoded_len);

	/* Verify the JSON is now parseable */
	TEST_ASSERT_EQUAL_CHAR('[', body[0]);
	char buf[32];
	TEST_ASSERT_TRUE(json_get_string(body + 1, "tag_name", buf, sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING("v1.0", buf);
}

void test_chunked_decode_truncated(void)
{
	/* Chunk header says 100 bytes but only 5 are present (buffer truncated) */
	char body[] = "64\r\nHello";
	uint16_t len = decode_chunked_body(body, (uint16_t)(sizeof(body) - 1));
	/* Should decode whatever is available (5 bytes) */
	TEST_ASSERT_EQUAL_UINT16(5, len);
	TEST_ASSERT_EQUAL_STRING("Hello", body);
}

void test_chunked_decode_empty(void)
{
	/* Just the terminal chunk */
	char body[] = "0\r\n\r\n";
	uint16_t len = decode_chunked_body(body, (uint16_t)(sizeof(body) - 1));
	TEST_ASSERT_EQUAL_UINT16(0, len);
	TEST_ASSERT_EQUAL_CHAR('\0', body[0]);
}

void test_chunked_decode_with_extensions(void)
{
	/* Chunk with extension: "d;ext=value\r\nHello, World!\r\n0\r\n\r\n" */
	char body[] = "d;ext=value\r\nHello, World!\r\n0\r\n\r\n";
	uint16_t len = decode_chunked_body(body, (uint16_t)(sizeof(body) - 1));
	TEST_ASSERT_EQUAL_UINT16(13, len);
	TEST_ASSERT_EQUAL_STRING("Hello, World!", body);
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* json_get_string */
	RUN_TEST(test_json_get_string_simple);
	RUN_TEST(test_json_get_string_with_whitespace);
	RUN_TEST(test_json_get_string_escaped_quote);
	RUN_TEST(test_json_get_string_not_found);
	RUN_TEST(test_json_get_string_null_inputs);
	RUN_TEST(test_json_get_string_truncation);
	RUN_TEST(test_json_get_string_empty_value);

	/* json_get_int */
	RUN_TEST(test_json_get_int_positive);
	RUN_TEST(test_json_get_int_negative);
	RUN_TEST(test_json_get_int_zero);
	RUN_TEST(test_json_get_int_not_found);
	RUN_TEST(test_json_get_int_string_value);
	RUN_TEST(test_json_get_int_null_inputs);

	/* json_get_bool */
	RUN_TEST(test_json_get_bool_true);
	RUN_TEST(test_json_get_bool_false);
	RUN_TEST(test_json_get_bool_not_found);
	RUN_TEST(test_json_get_bool_null_inputs);

	/* json_find_array / json_array_next */
	RUN_TEST(test_json_find_array_basic);
	RUN_TEST(test_json_find_array_empty);
	RUN_TEST(test_json_find_array_not_found);
	RUN_TEST(test_json_array_next_iterates);

	/* json_find_object */
	RUN_TEST(test_json_find_object_basic);
	RUN_TEST(test_json_find_object_not_found);

	/* json_object_end */
	RUN_TEST(test_json_object_end_simple);
	RUN_TEST(test_json_object_end_nested);
	RUN_TEST(test_json_object_end_null);

	/* Integration: GitHub API payload */
	RUN_TEST(test_json_github_release_payload);

	/* Truncated response handling */
	RUN_TEST(test_json_truncated_release_object);

	/* Chunked transfer encoding decoder */
	RUN_TEST(test_chunked_decode_single_chunk);
	RUN_TEST(test_chunked_decode_multiple_chunks);
	RUN_TEST(test_chunked_decode_json_array);
	RUN_TEST(test_chunked_decode_truncated);
	RUN_TEST(test_chunked_decode_empty);
	RUN_TEST(test_chunked_decode_with_extensions);

	return UNITY_END();
}
