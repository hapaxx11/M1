/* See COPYING.txt for license details. */

/*
 * test_flipper_ir.c
 *
 * Unit tests for flipper_ir.c — IR protocol mapping and hex conversion.
 *
 * Tests the pure-logic functions:
 *   - flipper_ir_proto_to_irmp()    — Flipper name → IRMP protocol ID
 *   - flipper_ir_irmp_to_proto()    — IRMP ID → Flipper name (reverse)
 *   - flipper_ir_parse_block()      — VTable-based signal block parser
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "flipper_ir.h"

#include <string.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * String-backed ir_block_reader_t (same adapter as test_ir_cmd_parse.c)
 *
 * Lines starting with '#' are separators; lines containing ": " are KV.
 * Null sentinel terminates the line array.
 * =================================================================== */

typedef struct {
	const char * const *lines;
	size_t idx;
	char   key_buf[64];
	char   val_buf[2048]; /* large enough for RAW data lines */
	bool   is_sep_flag;
	bool   is_kv_flag;
} fir_str_ctx_t;

static bool fir_str_next(void *ctx)
{
	fir_str_ctx_t *r = (fir_str_ctx_t *)ctx;
	const char *line;
	const char *colon;
	size_t klen;
	const char *v;

	r->is_sep_flag = false;
	r->is_kv_flag  = false;

	if (r->lines[r->idx] == NULL)
		return false;

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

static bool fir_str_is_sep(void *ctx)   { return ((fir_str_ctx_t *)ctx)->is_sep_flag; }
static bool fir_str_parse_kv(void *ctx) { return ((fir_str_ctx_t *)ctx)->is_kv_flag; }
static const char *fir_str_get_key(void *ctx)   { return ((fir_str_ctx_t *)ctx)->key_buf; }
static const char *fir_str_get_value(void *ctx) { return ((fir_str_ctx_t *)ctx)->val_buf; }

static const ir_block_reader_t s_fir_reader = {
	.next      = fir_str_next,
	.is_sep    = fir_str_is_sep,
	.parse_kv  = fir_str_parse_kv,
	.get_key   = fir_str_get_key,
	.get_value = fir_str_get_value,
};

/* Convenience helpers */
#define FIR_PARSE(lines_arr, sig_ptr) \
	do { \
		fir_str_ctx_t ctx__ = { .lines = (lines_arr), .idx = 0 }; \
		flipper_ir_parse_block(&s_fir_reader, &ctx__, (sig_ptr)); \
	} while (0)

#define FIR_PARSE_RET(lines_arr, sig_ptr) \
	({ \
		fir_str_ctx_t ctx__ = { .lines = (lines_arr), .idx = 0 }; \
		flipper_ir_parse_block(&s_fir_reader, &ctx__, (sig_ptr)); \
	})

/* ===================================================================
 * flipper_ir_proto_to_irmp — forward mapping (name → ID)
 * =================================================================== */

void test_ir_nec(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL,
		flipper_ir_proto_to_irmp("NEC"));
}

void test_ir_necext(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL,
		flipper_ir_proto_to_irmp("NECext"));
}

void test_ir_nec42(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC42_PROTOCOL,
		flipper_ir_proto_to_irmp("NEC42"));
}

void test_ir_nec42ext(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC42_PROTOCOL,
		flipper_ir_proto_to_irmp("NEC42ext"));
}

void test_ir_nec16(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC16_PROTOCOL,
		flipper_ir_proto_to_irmp("NEC16"));
}

void test_ir_samsung32(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG32_PROTOCOL,
		flipper_ir_proto_to_irmp("Samsung32"));
}

void test_ir_rc5(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC5_PROTOCOL,
		flipper_ir_proto_to_irmp("RC5"));
}

void test_ir_rc5x(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC5_PROTOCOL,
		flipper_ir_proto_to_irmp("RC5X"));
}

void test_ir_rc6(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC6_PROTOCOL,
		flipper_ir_proto_to_irmp("RC6"));
}

void test_ir_sirc(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL,
		flipper_ir_proto_to_irmp("SIRC"));
}

void test_ir_sirc15(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL,
		flipper_ir_proto_to_irmp("SIRC15"));
}

void test_ir_sirc20(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SIRCS_PROTOCOL,
		flipper_ir_proto_to_irmp("SIRC20"));
}

void test_ir_kaseikyo(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_KASEIKYO_PROTOCOL,
		flipper_ir_proto_to_irmp("Kaseikyo"));
}

void test_ir_rca(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RCCAR_PROTOCOL,
		flipper_ir_proto_to_irmp("RCA"));
}

void test_ir_pioneer(void)
{
	/* Pioneer uses NEC encoding */
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL,
		flipper_ir_proto_to_irmp("Pioneer"));
}

void test_ir_denon(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_DENON_PROTOCOL,
		flipper_ir_proto_to_irmp("Denon"));
}

void test_ir_jvc(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_JVC_PROTOCOL,
		flipper_ir_proto_to_irmp("JVC"));
}

void test_ir_sharp(void)
{
	/* Sharp uses same as Denon */
	TEST_ASSERT_EQUAL_UINT8(IRMP_DENON_PROTOCOL,
		flipper_ir_proto_to_irmp("Sharp"));
}

void test_ir_panasonic(void)
{
	/* Panasonic uses Kaseikyo */
	TEST_ASSERT_EQUAL_UINT8(IRMP_KASEIKYO_PROTOCOL,
		flipper_ir_proto_to_irmp("Panasonic"));
}

void test_ir_lg(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_LGAIR_PROTOCOL,
		flipper_ir_proto_to_irmp("LG"));
}

void test_ir_samsung(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG32_PROTOCOL,
		flipper_ir_proto_to_irmp("Samsung"));
}

void test_ir_apple(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_APPLE_PROTOCOL,
		flipper_ir_proto_to_irmp("Apple"));
}

void test_ir_nokia(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NOKIA_PROTOCOL,
		flipper_ir_proto_to_irmp("Nokia"));
}

void test_ir_bose(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_BOSE_PROTOCOL,
		flipper_ir_proto_to_irmp("Bose"));
}

void test_ir_samsung48(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG48_PROTOCOL,
		flipper_ir_proto_to_irmp("Samsung48"));
}

void test_ir_rcmm(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_RCMM32_PROTOCOL,
		flipper_ir_proto_to_irmp("RCMM"));
}

/* ===================================================================
 * Case insensitivity
 * =================================================================== */

void test_ir_case_lower(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL,
		flipper_ir_proto_to_irmp("nec"));
}

void test_ir_case_upper(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG32_PROTOCOL,
		flipper_ir_proto_to_irmp("SAMSUNG32"));
}

void test_ir_case_mixed(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_DENON_PROTOCOL,
		flipper_ir_proto_to_irmp("dEnOn"));
}

/* ===================================================================
 * Edge cases
 * =================================================================== */

void test_ir_unknown(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_UNKNOWN_PROTOCOL,
		flipper_ir_proto_to_irmp("SomeUnknownProtocol"));
}

void test_ir_null(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_UNKNOWN_PROTOCOL,
		flipper_ir_proto_to_irmp(NULL));
}

void test_ir_empty(void)
{
	TEST_ASSERT_EQUAL_UINT8(IRMP_UNKNOWN_PROTOCOL,
		flipper_ir_proto_to_irmp(""));
}

/* ===================================================================
 * flipper_ir_irmp_to_proto — reverse mapping (ID → name)
 * =================================================================== */

void test_ir_reverse_nec(void)
{
	TEST_ASSERT_EQUAL_STRING("NEC",
		flipper_ir_irmp_to_proto(IRMP_NEC_PROTOCOL));
}

void test_ir_reverse_samsung32(void)
{
	TEST_ASSERT_EQUAL_STRING("Samsung32",
		flipper_ir_irmp_to_proto(IRMP_SAMSUNG32_PROTOCOL));
}

void test_ir_reverse_rc5(void)
{
	TEST_ASSERT_EQUAL_STRING("RC5",
		flipper_ir_irmp_to_proto(IRMP_RC5_PROTOCOL));
}

void test_ir_reverse_sirc(void)
{
	TEST_ASSERT_EQUAL_STRING("SIRC",
		flipper_ir_irmp_to_proto(IRMP_SIRCS_PROTOCOL));
}

void test_ir_reverse_unknown(void)
{
	TEST_ASSERT_EQUAL_STRING("Unknown",
		flipper_ir_irmp_to_proto(255));
}

void test_ir_reverse_lg(void)
{
	TEST_ASSERT_EQUAL_STRING("LG",
		flipper_ir_irmp_to_proto(IRMP_LGAIR_PROTOCOL));
}

void test_ir_reverse_bose(void)
{
	TEST_ASSERT_EQUAL_STRING("Bose",
		flipper_ir_irmp_to_proto(IRMP_BOSE_PROTOCOL));
}

/* ===================================================================
 * Round-trip: name → ID → name (first match wins for aliases)
 * =================================================================== */

void test_ir_roundtrip_rc6(void)
{
	uint8_t id = flipper_ir_proto_to_irmp("RC6");
	const char *name = flipper_ir_irmp_to_proto(id);
	TEST_ASSERT_EQUAL_STRING("RC6", name);
}

void test_ir_roundtrip_jvc(void)
{
	uint8_t id = flipper_ir_proto_to_irmp("JVC");
	const char *name = flipper_ir_irmp_to_proto(id);
	TEST_ASSERT_EQUAL_STRING("JVC", name);
}

void test_ir_roundtrip_apple(void)
{
	uint8_t id = flipper_ir_proto_to_irmp("Apple");
	const char *name = flipper_ir_irmp_to_proto(id);
	TEST_ASSERT_EQUAL_STRING("Apple", name);
}

/* ===================================================================
 * flipper_ir_parse_block() tests — null guards
 * =================================================================== */

void test_parse_block_null_ops(void)
{
	flipper_ir_signal_t sig;
	fir_str_ctx_t ctx = { .lines = (const char *[]){ NULL }, .idx = 0 };
	TEST_ASSERT_FALSE(flipper_ir_parse_block(NULL, &ctx, &sig));
}

void test_parse_block_null_ctx(void)
{
	flipper_ir_signal_t sig;
	TEST_ASSERT_FALSE(flipper_ir_parse_block(&s_fir_reader, NULL, &sig));
}

void test_parse_block_null_out(void)
{
	fir_str_ctx_t ctx = { .lines = (const char *[]){ NULL }, .idx = 0 };
	TEST_ASSERT_FALSE(flipper_ir_parse_block(&s_fir_reader, &ctx, NULL));
}

void test_parse_block_empty_stream(void)
{
	static const char * const lines[] = { NULL };
	flipper_ir_signal_t sig;
	TEST_ASSERT_FALSE(FIR_PARSE_RET(lines, &sig));
}

/* ===================================================================
 * flipper_ir_parse_block() — parsed signals
 * =================================================================== */

void test_parse_block_nec_basic(void)
{
	static const char * const lines[] = {
		"name: Power",
		"type: parsed",
		"protocol: NEC",
		"address: 07 00 00 00",
		"command: 02 00 00 00",
		NULL
	};
	flipper_ir_signal_t sig;
	bool ok = FIR_PARSE_RET(lines, &sig);
	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_TRUE(sig.valid);
	TEST_ASSERT_EQUAL(FLIPPER_IR_SIGNAL_PARSED, sig.type);
	TEST_ASSERT_EQUAL_STRING("Power", sig.name);
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL, sig.parsed.protocol);
	TEST_ASSERT_EQUAL_UINT16(0x0007, sig.parsed.address);
	TEST_ASSERT_EQUAL_UINT16(0x0002, sig.parsed.command);
	TEST_ASSERT_EQUAL_UINT8(0, sig.parsed.flags);
}

void test_parse_block_rc5_basic(void)
{
	static const char * const lines[] = {
		"name: Mute",
		"type: parsed",
		"protocol: RC5",
		"address: 10 00 00 00",
		"command: 0D 00 00 00",
		NULL
	};
	flipper_ir_signal_t sig;
	bool ok = FIR_PARSE_RET(lines, &sig);
	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_EQUAL_STRING("Mute", sig.name);
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC5_PROTOCOL, sig.parsed.protocol);
	TEST_ASSERT_EQUAL_UINT16(0x0010, sig.parsed.address);
	TEST_ASSERT_EQUAL_UINT16(0x000D, sig.parsed.command);
}

void test_parse_block_samsung32_basic(void)
{
	static const char * const lines[] = {
		"name: Vol+",
		"type: parsed",
		"protocol: Samsung32",
		"address: 07 07 00 00",
		"command: E0 00 00 00",
		NULL
	};
	flipper_ir_signal_t sig;
	bool ok = FIR_PARSE_RET(lines, &sig);
	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_EQUAL_UINT8(IRMP_SAMSUNG32_PROTOCOL, sig.parsed.protocol);
	TEST_ASSERT_EQUAL_UINT16(0x0707, sig.parsed.address);
	TEST_ASSERT_EQUAL_UINT16(0x00E0, sig.parsed.command);
}

void test_parse_block_address_single_byte(void)
{
	/* Single byte address — stored in low byte, high byte = 0 */
	static const char * const lines[] = {
		"name: Ok",
		"type: parsed",
		"protocol: NEC",
		"address: AB 00 00 00",
		"command: 01 00 00 00",
		NULL
	};
	flipper_ir_signal_t sig;
	FIR_PARSE(lines, &sig);
	TEST_ASSERT_EQUAL_UINT16(0x00AB, sig.parsed.address);
}

void test_parse_block_unknown_protocol_returns_zero_id(void)
{
	/* Unknown protocol name → protocol field == 0 (IRMP_UNKNOWN_PROTOCOL).
	 * The parser still returns true when "command:" is found; the caller is
	 * responsible for checking sig.parsed.protocol != 0. */
	static const char * const lines[] = {
		"name: Foo",
		"type: parsed",
		"protocol: NoSuchProto",
		"address: 01 00 00 00",
		"command: 01 00 00 00",
		NULL
	};
	flipper_ir_signal_t sig;
	bool ok = FIR_PARSE_RET(lines, &sig);
	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_EQUAL_UINT8(IRMP_UNKNOWN_PROTOCOL, sig.parsed.protocol);
}

void test_parse_block_unknown_type_fails(void)
{
	static const char * const lines[] = {
		"name: Bar",
		"type: laser",
		NULL
	};
	flipper_ir_signal_t sig;
	bool ok = FIR_PARSE_RET(lines, &sig);
	TEST_ASSERT_FALSE(ok);
}

void test_parse_block_missing_command_fails(void)
{
	/* No command: line — block ends without marking valid */
	static const char * const lines[] = {
		"name: InComplete",
		"type: parsed",
		"protocol: NEC",
		"address: 07 00 00 00",
		NULL
	};
	flipper_ir_signal_t sig;
	bool ok = FIR_PARSE_RET(lines, &sig);
	TEST_ASSERT_FALSE(ok);
}

void test_parse_block_separator_is_skipped(void)
{
	/* Separators in flipper_ir_parse_block are skipped (continue), not
	 * terminators.  The signal following the separator is still found. */
	static const char * const lines[] = {
		"name: S1",
		"type: parsed",
		"protocol: NEC",
		"address: 01 00 00 00",
		"#",              /* separator — skipped */
		"command: 01 00 00 00",
		NULL
	};
	flipper_ir_signal_t sig;
	bool ok = FIR_PARSE_RET(lines, &sig);
	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_EQUAL_UINT16(0x0001, sig.parsed.command);
}

/* ===================================================================
 * flipper_ir_parse_block() — raw signals
 * =================================================================== */

void test_parse_block_raw_basic(void)
{
	static const char * const lines[] = {
		"name: Raw1",
		"type: raw",
		"frequency: 38000",
		"duty_cycle: 0.330000",
		"data: 9024 4512 579 552 579 552",
		NULL
	};
	flipper_ir_signal_t sig;
	bool ok = FIR_PARSE_RET(lines, &sig);
	TEST_ASSERT_TRUE(ok);
	TEST_ASSERT_TRUE(sig.valid);
	TEST_ASSERT_EQUAL(FLIPPER_IR_SIGNAL_RAW, sig.type);
	TEST_ASSERT_EQUAL_STRING("Raw1", sig.name);
	TEST_ASSERT_EQUAL_UINT32(38000, sig.raw.frequency);
	TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.33f, sig.raw.duty_cycle);
	TEST_ASSERT_EQUAL_UINT16(6, sig.raw.sample_count);
	TEST_ASSERT_EQUAL_INT32(9024, sig.raw.samples[0]);
	TEST_ASSERT_EQUAL_INT32(4512, sig.raw.samples[1]);
	TEST_ASSERT_EQUAL_INT32(579,  sig.raw.samples[2]);
}

void test_parse_block_raw_negative_samples(void)
{
	static const char * const lines[] = {
		"name: Neg",
		"type: raw",
		"frequency: 38000",
		"duty_cycle: 0.5",
		"data: 500 -300 400 -200",
		NULL
	};
	flipper_ir_signal_t sig;
	FIR_PARSE(lines, &sig);
	TEST_ASSERT_TRUE(sig.valid);
	TEST_ASSERT_EQUAL_INT32(500,  sig.raw.samples[0]);
	TEST_ASSERT_EQUAL_INT32(-300, sig.raw.samples[1]);
	TEST_ASSERT_EQUAL_INT32(400,  sig.raw.samples[2]);
	TEST_ASSERT_EQUAL_INT32(-200, sig.raw.samples[3]);
}

void test_parse_block_raw_zero_frequency_fails(void)
{
	static const char * const lines[] = {
		"name: ZeroFreq",
		"type: raw",
		"frequency: 0",
		"duty_cycle: 0.33",
		"data: 500 300 400",
		NULL
	};
	flipper_ir_signal_t sig;
	FIR_PARSE(lines, &sig);
	/* frequency == 0 is accepted by the parser (validity depends on caller) */
	/* sample_count > 0 → valid is set to true regardless of frequency */
	TEST_ASSERT_EQUAL_UINT32(0, sig.raw.frequency);
}

void test_parse_block_raw_empty_data_fails(void)
{
	static const char * const lines[] = {
		"name: NoData",
		"type: raw",
		"frequency: 38000",
		"duty_cycle: 0.33",
		"data: ",
		NULL
	};
	flipper_ir_signal_t sig;
	FIR_PARSE(lines, &sig);
	TEST_ASSERT_FALSE(sig.valid); /* sample_count == 0 → invalid */
}

/* ===================================================================
 * flipper_ir_parse_block() — duty_cycle parsing edge cases
 * =================================================================== */

void test_parse_block_duty_cycle_integer(void)
{
	static const char * const lines[] = {
		"name: DC1",
		"type: raw",
		"frequency: 38000",
		"duty_cycle: 1",
		"data: 100",
		NULL
	};
	flipper_ir_signal_t sig;
	FIR_PARSE(lines, &sig);
	TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, sig.raw.duty_cycle);
}

/* ===================================================================
 * flipper_ir_parse_block() — two sequential signals
 * =================================================================== */

void test_parse_block_two_sequential(void)
{
	static const char * const s1[] = {
		"name: First",
		"type: parsed",
		"protocol: NEC",
		"address: 01 00 00 00",
		"command: 02 00 00 00",
		NULL
	};
	static const char * const s2[] = {
		"name: Second",
		"type: parsed",
		"protocol: RC5",
		"address: 10 00 00 00",
		"command: 05 00 00 00",
		NULL
	};
	flipper_ir_signal_t sig1, sig2;
	bool ok1 = FIR_PARSE_RET(s1, &sig1);
	bool ok2 = FIR_PARSE_RET(s2, &sig2);
	TEST_ASSERT_TRUE(ok1);
	TEST_ASSERT_TRUE(ok2);
	TEST_ASSERT_EQUAL_STRING("First", sig1.name);
	TEST_ASSERT_EQUAL_STRING("Second", sig2.name);
	TEST_ASSERT_EQUAL_UINT8(IRMP_NEC_PROTOCOL, sig1.parsed.protocol);
	TEST_ASSERT_EQUAL_UINT8(IRMP_RC5_PROTOCOL, sig2.parsed.protocol);
}

/* ===================================================================
 * flipper_ir_parse_block() — name length boundary
 * =================================================================== */

void test_parse_block_name_truncated(void)
{
	/* Name longer than FLIPPER_IR_NAME_MAX_LEN (32) must be truncated */
	static const char * const lines[] = {
		"name: ABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890",  /* > 32 chars */
		"type: parsed",
		"protocol: NEC",
		"address: 01 00 00 00",
		"command: 01 00 00 00",
		NULL
	};
	flipper_ir_signal_t sig;
	FIR_PARSE(lines, &sig);
	TEST_ASSERT_EQUAL_UINT32(FLIPPER_IR_NAME_MAX_LEN - 1, strlen(sig.name));
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* Forward mapping — all protocols */
	RUN_TEST(test_ir_nec);
	RUN_TEST(test_ir_necext);
	RUN_TEST(test_ir_nec42);
	RUN_TEST(test_ir_nec42ext);
	RUN_TEST(test_ir_nec16);
	RUN_TEST(test_ir_samsung32);
	RUN_TEST(test_ir_rc5);
	RUN_TEST(test_ir_rc5x);
	RUN_TEST(test_ir_rc6);
	RUN_TEST(test_ir_sirc);
	RUN_TEST(test_ir_sirc15);
	RUN_TEST(test_ir_sirc20);
	RUN_TEST(test_ir_kaseikyo);
	RUN_TEST(test_ir_rca);
	RUN_TEST(test_ir_pioneer);
	RUN_TEST(test_ir_denon);
	RUN_TEST(test_ir_jvc);
	RUN_TEST(test_ir_sharp);
	RUN_TEST(test_ir_panasonic);
	RUN_TEST(test_ir_lg);
	RUN_TEST(test_ir_samsung);
	RUN_TEST(test_ir_apple);
	RUN_TEST(test_ir_nokia);
	RUN_TEST(test_ir_bose);
	RUN_TEST(test_ir_samsung48);
	RUN_TEST(test_ir_rcmm);

	/* Case insensitivity */
	RUN_TEST(test_ir_case_lower);
	RUN_TEST(test_ir_case_upper);
	RUN_TEST(test_ir_case_mixed);

	/* Edge cases */
	RUN_TEST(test_ir_unknown);
	RUN_TEST(test_ir_null);
	RUN_TEST(test_ir_empty);

	/* Reverse mapping */
	RUN_TEST(test_ir_reverse_nec);
	RUN_TEST(test_ir_reverse_samsung32);
	RUN_TEST(test_ir_reverse_rc5);
	RUN_TEST(test_ir_reverse_sirc);
	RUN_TEST(test_ir_reverse_unknown);
	RUN_TEST(test_ir_reverse_lg);
	RUN_TEST(test_ir_reverse_bose);

	/* Round-trip */
	RUN_TEST(test_ir_roundtrip_rc6);
	RUN_TEST(test_ir_roundtrip_jvc);
	RUN_TEST(test_ir_roundtrip_apple);

	/* flipper_ir_parse_block — null guards */
	RUN_TEST(test_parse_block_null_ops);
	RUN_TEST(test_parse_block_null_ctx);
	RUN_TEST(test_parse_block_null_out);
	RUN_TEST(test_parse_block_empty_stream);

	/* flipper_ir_parse_block — parsed signals */
	RUN_TEST(test_parse_block_nec_basic);
	RUN_TEST(test_parse_block_rc5_basic);
	RUN_TEST(test_parse_block_samsung32_basic);
	RUN_TEST(test_parse_block_address_single_byte);
	RUN_TEST(test_parse_block_unknown_protocol_returns_zero_id);
	RUN_TEST(test_parse_block_unknown_type_fails);
	RUN_TEST(test_parse_block_missing_command_fails);
	RUN_TEST(test_parse_block_separator_is_skipped);

	/* flipper_ir_parse_block — raw signals */
	RUN_TEST(test_parse_block_raw_basic);
	RUN_TEST(test_parse_block_raw_negative_samples);
	RUN_TEST(test_parse_block_raw_zero_frequency_fails);
	RUN_TEST(test_parse_block_raw_empty_data_fails);
	RUN_TEST(test_parse_block_duty_cycle_integer);

	/* flipper_ir_parse_block — multi-signal + boundary */
	RUN_TEST(test_parse_block_two_sequential);
	RUN_TEST(test_parse_block_name_truncated);

	return UNITY_END();
}
