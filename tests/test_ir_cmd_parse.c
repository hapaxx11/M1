/* See COPYING.txt for license details. */

/*
 * test_ir_cmd_parse.c
 *
 * Unit tests for ir_cmd_parse() and ir_parse_hex_bytes() in ir_signal_record.c
 * (Phase G extraction — decouples parse_ir_signal_block from FatFS).
 *
 * A minimal string-backed ir_block_reader_t replaces flipper_file_t so the
 * parser logic can run on the host without any FatFS or HAL dependency.
 *
 * Coverage:
 *   ir_parse_hex_bytes()  — all branches: NULL, empty, single/multi byte, overflow
 *   ir_cmd_parse()        — parsed signals, raw signals, separator termination,
 *                           unknown protocol, missing fields, null guard, multi-
 *                           signal stream (sequential calls), byte-order encoding
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "ir_signal_record.h"

#include <string.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * String-backed ir_block_reader_t
 *
 * Simulates a sequence of text lines for ir_cmd_parse() without any
 * FatFS dependency.  Lines must be plain C strings stored in a
 * null-terminated array; the reader walks through them one at a time.
 * A line starting with '#' is treated as a separator.
 * Any line containing ": " is parsed as "key: value".
 * =================================================================== */

typedef struct {
	const char * const *lines; /* null-terminated array of line strings */
	size_t idx;
	char   key_buf[64];
	char   val_buf[448];
	bool   is_sep_flag;
	bool   is_kv_flag;
} str_reader_ctx_t;

static bool str_next(void *ctx)
{
	str_reader_ctx_t *r = (str_reader_ctx_t *)ctx;
	const char *line;
	const char *colon;
	size_t klen;
	const char *v;

	r->is_sep_flag = false;
	r->is_kv_flag  = false;

	if (r->lines[r->idx] == NULL)
		return false; /* EOF */

	line = r->lines[r->idx++];

	if (line[0] == '#')
	{
		r->is_sep_flag = true;
		return true;
	}

	colon = strchr(line, ':');
	if (colon != NULL)
	{
		klen = (size_t)(colon - line);
		if (klen < sizeof(r->key_buf))
		{
			strncpy(r->key_buf, line, klen);
			r->key_buf[klen] = '\0';

			v = colon + 1;
			while (*v == ' ')
				v++;
			strncpy(r->val_buf, v, sizeof(r->val_buf) - 1);
			r->val_buf[sizeof(r->val_buf) - 1] = '\0';

			r->is_kv_flag = true;
		}
	}

	return true;
}

static bool str_is_sep(void *ctx)
{
	return ((str_reader_ctx_t *)ctx)->is_sep_flag;
}

static bool str_parse_kv(void *ctx)
{
	return ((str_reader_ctx_t *)ctx)->is_kv_flag;
}

static const char *str_get_key(void *ctx)
{
	return ((str_reader_ctx_t *)ctx)->key_buf;
}

static const char *str_get_value(void *ctx)
{
	return ((str_reader_ctx_t *)ctx)->val_buf;
}

static const ir_block_reader_t s_str_reader = {
	.next      = str_next,
	.is_sep    = str_is_sep,
	.parse_kv  = str_parse_kv,
	.get_key   = str_get_key,
	.get_value = str_get_value,
};

/* Convenience macro: build a ctx from a literal line array and call ir_cmd_parse */
#define PARSE_LINES(lines_arr, cmd_ptr) \
	do { \
		str_reader_ctx_t ctx__ = { .lines = (lines_arr), .idx = 0 }; \
		ir_cmd_parse(&s_str_reader, &ctx__, (cmd_ptr)); \
	} while (0)

#define PARSE_LINES_RET(lines_arr, cmd_ptr) \
	({ \
		str_reader_ctx_t ctx__ = { .lines = (lines_arr), .idx = 0 }; \
		ir_cmd_parse(&s_str_reader, &ctx__, (cmd_ptr)); \
	})

/* ===================================================================
 * ir_parse_hex_bytes() tests
 * =================================================================== */

void test_hex_null_str(void)
{
	uint8_t out[4];
	TEST_ASSERT_EQUAL_UINT8(0, ir_parse_hex_bytes(NULL, out, 4));
}

void test_hex_null_out(void)
{
	TEST_ASSERT_EQUAL_UINT8(0, ir_parse_hex_bytes("AA BB", NULL, 4));
}

void test_hex_zero_maxlen(void)
{
	uint8_t out[4];
	TEST_ASSERT_EQUAL_UINT8(0, ir_parse_hex_bytes("AA BB", out, 0));
}

void test_hex_empty_str(void)
{
	uint8_t out[4] = {0};
	TEST_ASSERT_EQUAL_UINT8(0, ir_parse_hex_bytes("", out, 4));
}

void test_hex_single_byte(void)
{
	uint8_t out[4] = {0};
	TEST_ASSERT_EQUAL_UINT8(1, ir_parse_hex_bytes("AB", out, 4));
	TEST_ASSERT_EQUAL_UINT8(0xAB, out[0]);
}

void test_hex_four_bytes_space_separated(void)
{
	uint8_t out[4] = {0};
	TEST_ASSERT_EQUAL_UINT8(4, ir_parse_hex_bytes("07 00 00 00", out, 4));
	TEST_ASSERT_EQUAL_UINT8(0x07, out[0]);
	TEST_ASSERT_EQUAL_UINT8(0x00, out[1]);
	TEST_ASSERT_EQUAL_UINT8(0x00, out[2]);
	TEST_ASSERT_EQUAL_UINT8(0x00, out[3]);
}

void test_hex_overflow_clamps_to_max_len(void)
{
	uint8_t out[2] = {0};
	/* Input has 4 bytes but max_len is 2 — should stop at 2 */
	TEST_ASSERT_EQUAL_UINT8(2, ir_parse_hex_bytes("01 02 03 04", out, 2));
	TEST_ASSERT_EQUAL_UINT8(0x01, out[0]);
	TEST_ASSERT_EQUAL_UINT8(0x02, out[1]);
}

void test_hex_upper_and_lower_case(void)
{
	uint8_t out[4] = {0};
	TEST_ASSERT_EQUAL_UINT8(3, ir_parse_hex_bytes("ab CD eF", out, 4));
	TEST_ASSERT_EQUAL_UINT8(0xAB, out[0]);
	TEST_ASSERT_EQUAL_UINT8(0xCD, out[1]);
	TEST_ASSERT_EQUAL_UINT8(0xEF, out[2]);
}

void test_hex_leading_whitespace(void)
{
	uint8_t out[4] = {0};
	TEST_ASSERT_EQUAL_UINT8(1, ir_parse_hex_bytes("  FF", out, 4));
	TEST_ASSERT_EQUAL_UINT8(0xFF, out[0]);
}

/* ===================================================================
 * ir_cmd_parse() — null / invalid input guards
 * =================================================================== */

void test_parse_null_ops(void)
{
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = NULL, .idx = 0 };
	TEST_ASSERT_FALSE(ir_cmd_parse(NULL, &ctx, &cmd));
}

void test_parse_null_ctx(void)
{
	ir_universal_cmd_t cmd;
	TEST_ASSERT_FALSE(ir_cmd_parse(&s_str_reader, NULL, &cmd));
}

void test_parse_null_cmd(void)
{
	const char *lines[] = { "name: Power", NULL };
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };
	TEST_ASSERT_FALSE(ir_cmd_parse(&s_str_reader, &ctx, NULL));
}

void test_parse_empty_stream(void)
{
	const char *lines[] = { NULL };
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };
	TEST_ASSERT_FALSE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
}

/* ===================================================================
 * ir_cmd_parse() — valid parsed (protocol) signal
 * =================================================================== */

void test_parse_nec_signal(void)
{
	const char *lines[] = {
		"name: Power",
		"type: parsed",
		"protocol: NEC",
		"address: 07 00 00 00",
		"command: 02 00 00 00",
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	TEST_ASSERT_TRUE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
	TEST_ASSERT_TRUE(cmd.valid);
	TEST_ASSERT_FALSE(cmd.is_raw);
	TEST_ASSERT_EQUAL_STRING("Power", cmd.name);
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL, cmd.protocol);
	TEST_ASSERT_EQUAL_UINT16(0x0007, cmd.address);
	TEST_ASSERT_EQUAL_UINT16(0x0002, cmd.command);
}

void test_parse_rc5_signal(void)
{
	const char *lines[] = {
		"name: VolUp",
		"type: parsed",
		"protocol: RC5",
		"address: 00 00 00 00",
		"command: 10 00 00 00",
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	TEST_ASSERT_TRUE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC5_PROTOCOL, cmd.protocol);
	TEST_ASSERT_EQUAL_UINT16(0x0010, cmd.command);
}

void test_parse_samsung32_signal(void)
{
	const char *lines[] = {
		"name: Mute",
		"type: parsed",
		"protocol: Samsung32",
		"address: E0 07 00 00",
		"command: 0B 00 00 00",
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	TEST_ASSERT_TRUE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG32_PROTOCOL, cmd.protocol);
	/* address: 0xE0 | (0x07 << 8) = 0x07E0 */
	TEST_ASSERT_EQUAL_UINT16(0x07E0, cmd.address);
	TEST_ASSERT_EQUAL_UINT16(0x000B, cmd.command);
}

/* ===================================================================
 * ir_cmd_parse() — address/command byte-order encoding
 * =================================================================== */

void test_parse_address_little_endian_two_bytes(void)
{
	const char *lines[] = {
		"name: Ch+",
		"type: parsed",
		"protocol: NEC",
		"address: 12 34 00 00",
		"command: AB CD 00 00",
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	TEST_ASSERT_TRUE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
	/* Little-endian: 0x12 | (0x34 << 8) = 0x3412 */
	TEST_ASSERT_EQUAL_UINT16(0x3412, cmd.address);
	TEST_ASSERT_EQUAL_UINT16(0xCDAB, cmd.command);
}

void test_parse_address_single_byte(void)
{
	const char *lines[] = {
		"name: Input",
		"type: parsed",
		"protocol: NEC",
		"address: 20",
		"command: 01",
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	TEST_ASSERT_TRUE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
	TEST_ASSERT_EQUAL_UINT16(0x0020, cmd.address);
	TEST_ASSERT_EQUAL_UINT16(0x0001, cmd.command);
}

/* ===================================================================
 * ir_cmd_parse() — valid raw signal
 * =================================================================== */

void test_parse_raw_signal(void)
{
	const char *lines[] = {
		"name: Learn_01",
		"type: raw",
		"frequency: 38000",
		"duty_cycle: 0.330000",
		"data: 9024 4512 579 552 579 552",
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	TEST_ASSERT_TRUE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
	TEST_ASSERT_TRUE(cmd.valid);
	TEST_ASSERT_TRUE(cmd.is_raw);
	TEST_ASSERT_EQUAL_STRING("Learn_01", cmd.name);
	TEST_ASSERT_EQUAL_UINT32(38000U, cmd.raw_freq);
	TEST_ASSERT_EQUAL_UINT16(6U, cmd.raw_count);
}

void test_parse_raw_single_sample(void)
{
	const char *lines[] = {
		"name: Pulse",
		"type: raw",
		"frequency: 36000",
		"duty_cycle: 0.5",
		"data: 500",
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	TEST_ASSERT_TRUE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
	TEST_ASSERT_EQUAL_UINT16(1U, cmd.raw_count);
}

/* ===================================================================
 * ir_cmd_parse() — separator termination
 * =================================================================== */

void test_parse_separator_ends_block(void)
{
	/* Separator (#) terminates the current block */
	const char *lines[] = {
		"name: Stop",
		"type: parsed",
		"protocol: NEC",
		"address: 01 00 00 00",
		"command: 08 00 00 00",
		"#",                      /* separator */
		"name: Play",             /* belongs to next block — must not be read */
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	TEST_ASSERT_TRUE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
	/* Must have parsed "Stop", not "Play" */
	TEST_ASSERT_EQUAL_STRING("Stop", cmd.name);
	/* Reader index must be 6 (past the separator, not past "Play") */
	TEST_ASSERT_EQUAL(6U, ctx.idx);
}

void test_parse_two_blocks_sequential(void)
{
	/* Two signal blocks separated by "#" — each call should parse one block */
	const char *lines[] = {
		"name: On",
		"type: parsed",
		"protocol: NEC",
		"address: 00 00 00 00",
		"command: 01 00 00 00",
		"#",
		"name: Off",
		"type: parsed",
		"protocol: NEC",
		"address: 00 00 00 00",
		"command: 02 00 00 00",
		NULL
	};
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };
	ir_universal_cmd_t cmd1, cmd2;

	TEST_ASSERT_TRUE(ir_cmd_parse(&s_str_reader, &ctx, &cmd1));
	TEST_ASSERT_EQUAL_STRING("On", cmd1.name);
	TEST_ASSERT_EQUAL_UINT16(0x0001, cmd1.command);

	TEST_ASSERT_TRUE(ir_cmd_parse(&s_str_reader, &ctx, &cmd2));
	TEST_ASSERT_EQUAL_STRING("Off", cmd2.name);
	TEST_ASSERT_EQUAL_UINT16(0x0002, cmd2.command);
}

/* ===================================================================
 * ir_cmd_parse() — failure cases
 * =================================================================== */

void test_parse_unknown_protocol_returns_false(void)
{
	const char *lines[] = {
		"name: Foo",
		"type: parsed",
		"protocol: UnknownXYZ",
		"address: 00 00 00 00",
		"command: 00 00 00 00",
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	/* Unknown protocol maps to 0 → ir_cmd_parse returns false */
	TEST_ASSERT_FALSE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
}

void test_parse_missing_type_returns_false(void)
{
	const char *lines[] = {
		"name: Bar",
		"protocol: NEC",
		"address: 00 00 00 00",
		"command: 00 00 00 00",
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	TEST_ASSERT_FALSE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
}

void test_parse_missing_name_returns_false(void)
{
	const char *lines[] = {
		"type: parsed",
		"protocol: NEC",
		"address: 00 00 00 00",
		"command: 00 00 00 00",
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	/* got_name never set → returns false */
	TEST_ASSERT_FALSE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
}

void test_parse_raw_zero_frequency_returns_false(void)
{
	const char *lines[] = {
		"name: Bad",
		"type: raw",
		"frequency: 0",
		"duty_cycle: 0.33",
		"data: 100 200",
		NULL
	};
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	TEST_ASSERT_FALSE(ir_cmd_parse(&s_str_reader, &ctx, &cmd));
}

void test_parse_cmd_zeroed_on_entry(void)
{
	/* Ensure ir_cmd_parse() zeroes cmd before filling */
	ir_universal_cmd_t cmd;
	memset(&cmd, 0xFF, sizeof(cmd)); /* poison */

	const char *lines[] = { NULL };
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };

	ir_cmd_parse(&s_str_reader, &ctx, &cmd);

	/* valid must be false (we couldn't parse anything) */
	TEST_ASSERT_FALSE(cmd.valid);
}

/* ===================================================================
 * ir_cmd_parse() — protocol coverage spot-checks
 * =================================================================== */

static bool parse_protocol_test(const char *proto_name, uint8_t *out_proto)
{
	const char *lines[6];
	lines[0] = "name: T";
	lines[1] = "type: parsed";
	lines[2] = proto_name; /* "protocol: XYZ" — caller must format it */
	lines[3] = "address: 00 00 00 00";
	lines[4] = "command: 01 00 00 00";
	lines[5] = NULL;
	ir_universal_cmd_t cmd;
	str_reader_ctx_t ctx = { .lines = lines, .idx = 0 };
	bool r = ir_cmd_parse(&s_str_reader, &ctx, &cmd);
	if (r) *out_proto = cmd.protocol;
	return r;
}

void test_parse_protocol_samsung48(void)
{
	uint8_t p = 0;
	TEST_ASSERT_TRUE(parse_protocol_test("protocol: Samsung48", &p));
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG48_PROTOCOL, p);
}

void test_parse_protocol_rc6(void)
{
	uint8_t p = 0;
	TEST_ASSERT_TRUE(parse_protocol_test("protocol: RC6", &p));
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC6_PROTOCOL, p);
}

void test_parse_protocol_denon(void)
{
	uint8_t p = 0;
	TEST_ASSERT_TRUE(parse_protocol_test("protocol: Denon", &p));
	TEST_ASSERT_EQUAL_UINT8(IRMP_DENON_PROTOCOL, p);
}

/* ===================================================================
 * Test runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* ir_parse_hex_bytes() */
	RUN_TEST(test_hex_null_str);
	RUN_TEST(test_hex_null_out);
	RUN_TEST(test_hex_zero_maxlen);
	RUN_TEST(test_hex_empty_str);
	RUN_TEST(test_hex_single_byte);
	RUN_TEST(test_hex_four_bytes_space_separated);
	RUN_TEST(test_hex_overflow_clamps_to_max_len);
	RUN_TEST(test_hex_upper_and_lower_case);
	RUN_TEST(test_hex_leading_whitespace);

	/* ir_cmd_parse() — null/invalid guards */
	RUN_TEST(test_parse_null_ops);
	RUN_TEST(test_parse_null_ctx);
	RUN_TEST(test_parse_null_cmd);
	RUN_TEST(test_parse_empty_stream);

	/* ir_cmd_parse() — valid parsed signals */
	RUN_TEST(test_parse_nec_signal);
	RUN_TEST(test_parse_rc5_signal);
	RUN_TEST(test_parse_samsung32_signal);

	/* ir_cmd_parse() — byte-order encoding */
	RUN_TEST(test_parse_address_little_endian_two_bytes);
	RUN_TEST(test_parse_address_single_byte);

	/* ir_cmd_parse() — raw signals */
	RUN_TEST(test_parse_raw_signal);
	RUN_TEST(test_parse_raw_single_sample);

	/* ir_cmd_parse() — separator / sequential blocks */
	RUN_TEST(test_parse_separator_ends_block);
	RUN_TEST(test_parse_two_blocks_sequential);

	/* ir_cmd_parse() — failure cases */
	RUN_TEST(test_parse_unknown_protocol_returns_false);
	RUN_TEST(test_parse_missing_type_returns_false);
	RUN_TEST(test_parse_missing_name_returns_false);
	RUN_TEST(test_parse_raw_zero_frequency_returns_false);
	RUN_TEST(test_parse_cmd_zeroed_on_entry);

	/* ir_cmd_parse() — protocol spot-checks */
	RUN_TEST(test_parse_protocol_samsung48);
	RUN_TEST(test_parse_protocol_rc6);
	RUN_TEST(test_parse_protocol_denon);

	return UNITY_END();
}
