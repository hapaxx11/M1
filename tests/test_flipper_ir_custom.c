/* See COPYING.txt for license details. */

/**
 * @file   test_flipper_ir_custom.c
 * @brief  Host-side unit tests for Phase 3 flipper_ir extensions:
 *         flipper_ir_rename_signal(), flipper_ir_delete_signal(),
 *         flipper_ir_append_signal(), and flipper_ir_raw_feed_t.
 *
 * Tests use real temp files on the host via the ff.h stdio stub so that
 * the full streaming-rewrite path (read → temp → rename → replace) is
 * exercised, not just the pure-logic pieces.
 */

#include "unity.h"
#include "flipper_ir.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* ---- Helpers ------------------------------------------------------------ */

static char g_tmppath[128];   /* path of the test .ir file */
static char g_tmppath2[128];  /* second test path for multi-file tests */

/* Build a deterministic temp path using the test runner's PID. */
static void mk_tmppath(char *out, size_t sz, int id)
{
    snprintf(out, sz, "/tmp/test_flipper_ir_custom_%d_%d.ir", (int)getpid(), id);
}

/* Write a minimal .ir file with @p n signals named "Sig0".."Sig(n-1)". */
static void write_test_file(const char *path, int n)
{
    FILE *f = fopen(path, "w");
    if (!f) { TEST_FAIL_MESSAGE("Failed to create temp .ir file"); return; }
    fprintf(f, "Filetype: IR signals file\nVersion: 1\n");
    for (int i = 0; i < n; i++)
    {
        fprintf(f,
            "\n#\nname: Sig%d\ntype: parsed\nprotocol: NEC\n"
            "address: %02X 00 00 00\ncommand: %02X 00 00 00\n",
            i, i, i + 1);
    }
    fclose(f);
}

/* Read all signal names from @p path into @p names (max @p cap entries).
 * Returns the count. */
static uint16_t read_names(const char *path, char names[][FLIPPER_IR_NAME_MAX_LEN],
                             uint16_t cap)
{
    flipper_file_t ff;
    flipper_ir_signal_t sig;
    uint16_t n = 0;
    if (!flipper_ir_open(&ff, path)) return 0;
    while (n < cap && flipper_ir_read_signal(&ff, &sig))
        snprintf(names[n++], FLIPPER_IR_NAME_MAX_LEN, "%s", sig.name);
    ff_close(&ff);
    return n;
}

/* ---- setUp / tearDown --------------------------------------------------- */

void setUp(void)
{
    mk_tmppath(g_tmppath,  sizeof(g_tmppath),  0);
    mk_tmppath(g_tmppath2, sizeof(g_tmppath2), 1);
}

void tearDown(void)
{
    remove(g_tmppath);
    remove(g_tmppath2);
    /* Also clean up any leftover temp files from rewrite */
    char tmp[132];
    snprintf(tmp, sizeof(tmp), "%s~", g_tmppath);
    remove(tmp);
    snprintf(tmp, sizeof(tmp), "%s~", g_tmppath2);
    remove(tmp);
}

/* ======================================================================= */
/* flipper_ir_rename_signal                                                 */
/* ======================================================================= */

void test_rename_first_signal(void)
{
    char names[4][FLIPPER_IR_NAME_MAX_LEN];
    write_test_file(g_tmppath, 3);
    TEST_ASSERT_TRUE(flipper_ir_rename_signal(g_tmppath, 0, "NewFirst"));
    uint16_t n = read_names(g_tmppath, names, 4);
    TEST_ASSERT_EQUAL_UINT16(3, n);
    TEST_ASSERT_EQUAL_STRING("NewFirst", names[0]);
    TEST_ASSERT_EQUAL_STRING("Sig1",     names[1]);
    TEST_ASSERT_EQUAL_STRING("Sig2",     names[2]);
}

void test_rename_middle_signal(void)
{
    char names[4][FLIPPER_IR_NAME_MAX_LEN];
    write_test_file(g_tmppath, 3);
    TEST_ASSERT_TRUE(flipper_ir_rename_signal(g_tmppath, 1, "MiddleNew"));
    uint16_t n = read_names(g_tmppath, names, 4);
    TEST_ASSERT_EQUAL_UINT16(3, n);
    TEST_ASSERT_EQUAL_STRING("Sig0",      names[0]);
    TEST_ASSERT_EQUAL_STRING("MiddleNew", names[1]);
    TEST_ASSERT_EQUAL_STRING("Sig2",      names[2]);
}

void test_rename_last_signal(void)
{
    char names[4][FLIPPER_IR_NAME_MAX_LEN];
    write_test_file(g_tmppath, 3);
    TEST_ASSERT_TRUE(flipper_ir_rename_signal(g_tmppath, 2, "Last"));
    uint16_t n = read_names(g_tmppath, names, 4);
    TEST_ASSERT_EQUAL_UINT16(3, n);
    TEST_ASSERT_EQUAL_STRING("Sig0", names[0]);
    TEST_ASSERT_EQUAL_STRING("Sig1", names[1]);
    TEST_ASSERT_EQUAL_STRING("Last", names[2]);
}

void test_rename_oob_returns_false(void)
{
    write_test_file(g_tmppath, 3);
    TEST_ASSERT_FALSE(flipper_ir_rename_signal(g_tmppath, 5, "NoWay"));
    /* File content must be unchanged — 3 signals still there */
    TEST_ASSERT_EQUAL_UINT16(3, flipper_ir_count_signals(g_tmppath));
}

void test_rename_null_path_returns_false(void)
{
    TEST_ASSERT_FALSE(flipper_ir_rename_signal(NULL, 0, "Name"));
}

void test_rename_empty_new_name_returns_false(void)
{
    write_test_file(g_tmppath, 1);
    TEST_ASSERT_FALSE(flipper_ir_rename_signal(g_tmppath, 0, ""));
}

void test_rename_preserves_other_fields(void)
{
    /* Verify address/command survive the rename */
    flipper_file_t ff;
    flipper_ir_signal_t sig;

    write_test_file(g_tmppath, 2);
    TEST_ASSERT_TRUE(flipper_ir_rename_signal(g_tmppath, 0, "RenamedSig0"));

    TEST_ASSERT_TRUE(flipper_ir_open(&ff, g_tmppath));
    TEST_ASSERT_TRUE(flipper_ir_read_signal(&ff, &sig));
    ff_close(&ff);

    TEST_ASSERT_EQUAL_STRING("RenamedSig0", sig.name);
    TEST_ASSERT_EQUAL_UINT8(FLIPPER_IR_SIGNAL_PARSED, (uint8_t)sig.type);
    TEST_ASSERT_EQUAL_UINT16(0x00, sig.parsed.address); /* address from Sig0: 0x00 */
    TEST_ASSERT_EQUAL_UINT16(0x01, sig.parsed.command); /* command from Sig0: 0x01 */
}

void test_rename_single_signal_file(void)
{
    char names[2][FLIPPER_IR_NAME_MAX_LEN];
    write_test_file(g_tmppath, 1);
    TEST_ASSERT_TRUE(flipper_ir_rename_signal(g_tmppath, 0, "Solo"));
    uint16_t n = read_names(g_tmppath, names, 2);
    TEST_ASSERT_EQUAL_UINT16(1, n);
    TEST_ASSERT_EQUAL_STRING("Solo", names[0]);
}

/* ======================================================================= */
/* flipper_ir_delete_signal                                                 */
/* ======================================================================= */

void test_delete_first_signal(void)
{
    char names[4][FLIPPER_IR_NAME_MAX_LEN];
    write_test_file(g_tmppath, 3);
    TEST_ASSERT_TRUE(flipper_ir_delete_signal(g_tmppath, 0));
    uint16_t n = read_names(g_tmppath, names, 4);
    TEST_ASSERT_EQUAL_UINT16(2, n);
    TEST_ASSERT_EQUAL_STRING("Sig1", names[0]);
    TEST_ASSERT_EQUAL_STRING("Sig2", names[1]);
}

void test_delete_middle_signal(void)
{
    char names[4][FLIPPER_IR_NAME_MAX_LEN];
    write_test_file(g_tmppath, 3);
    TEST_ASSERT_TRUE(flipper_ir_delete_signal(g_tmppath, 1));
    uint16_t n = read_names(g_tmppath, names, 4);
    TEST_ASSERT_EQUAL_UINT16(2, n);
    TEST_ASSERT_EQUAL_STRING("Sig0", names[0]);
    TEST_ASSERT_EQUAL_STRING("Sig2", names[1]);
}

void test_delete_last_signal(void)
{
    char names[4][FLIPPER_IR_NAME_MAX_LEN];
    write_test_file(g_tmppath, 3);
    TEST_ASSERT_TRUE(flipper_ir_delete_signal(g_tmppath, 2));
    uint16_t n = read_names(g_tmppath, names, 4);
    TEST_ASSERT_EQUAL_UINT16(2, n);
    TEST_ASSERT_EQUAL_STRING("Sig0", names[0]);
    TEST_ASSERT_EQUAL_STRING("Sig1", names[1]);
}

void test_delete_oob_returns_false(void)
{
    write_test_file(g_tmppath, 3);
    TEST_ASSERT_FALSE(flipper_ir_delete_signal(g_tmppath, 99));
    TEST_ASSERT_EQUAL_UINT16(3, flipper_ir_count_signals(g_tmppath));
}

void test_delete_null_path_returns_false(void)
{
    TEST_ASSERT_FALSE(flipper_ir_delete_signal(NULL, 0));
}

void test_delete_only_signal_leaves_empty_file(void)
{
    write_test_file(g_tmppath, 1);
    TEST_ASSERT_TRUE(flipper_ir_delete_signal(g_tmppath, 0));
    TEST_ASSERT_EQUAL_UINT16(0, flipper_ir_count_signals(g_tmppath));
}

void test_delete_preserves_remaining_data(void)
{
    flipper_file_t ff;
    flipper_ir_signal_t sig;

    write_test_file(g_tmppath, 3);
    TEST_ASSERT_TRUE(flipper_ir_delete_signal(g_tmppath, 1));

    /* Read both remaining signals and verify address/command */
    TEST_ASSERT_TRUE(flipper_ir_open(&ff, g_tmppath));

    TEST_ASSERT_TRUE(flipper_ir_read_signal(&ff, &sig));
    TEST_ASSERT_EQUAL_STRING("Sig0", sig.name);
    TEST_ASSERT_EQUAL_UINT16(0x00, sig.parsed.address);
    TEST_ASSERT_EQUAL_UINT16(0x01, sig.parsed.command);

    TEST_ASSERT_TRUE(flipper_ir_read_signal(&ff, &sig));
    TEST_ASSERT_EQUAL_STRING("Sig2", sig.name);
    TEST_ASSERT_EQUAL_UINT16(0x02, sig.parsed.address);
    TEST_ASSERT_EQUAL_UINT16(0x03, sig.parsed.command);

    ff_close(&ff);
}

/* ======================================================================= */
/* flipper_ir_append_signal                                                 */
/* ======================================================================= */

static flipper_ir_signal_t make_parsed_sig(const char *name, uint16_t addr, uint16_t cmd)
{
    flipper_ir_signal_t s;
    memset(&s, 0, sizeof(s));
    snprintf(s.name, FLIPPER_IR_NAME_MAX_LEN, "%s", name);
    s.type             = FLIPPER_IR_SIGNAL_PARSED;
    s.parsed.protocol  = 2;   /* NEC */
    s.parsed.address   = addr;
    s.parsed.command   = cmd;
    s.valid            = true;
    return s;
}

void test_append_to_empty_file(void)
{
    /* Create file with header only (0 signals) */
    write_test_file(g_tmppath, 0);
    flipper_ir_signal_t sig = make_parsed_sig("First", 0x0A, 0x0B);
    TEST_ASSERT_TRUE(flipper_ir_append_signal(g_tmppath, &sig));
    TEST_ASSERT_EQUAL_UINT16(1, flipper_ir_count_signals(g_tmppath));

    char names[2][FLIPPER_IR_NAME_MAX_LEN];
    read_names(g_tmppath, names, 2);
    TEST_ASSERT_EQUAL_STRING("First", names[0]);
}

void test_append_to_existing_file(void)
{
    write_test_file(g_tmppath, 2);
    flipper_ir_signal_t sig = make_parsed_sig("AppendedNew", 0xFF, 0x10);
    TEST_ASSERT_TRUE(flipper_ir_append_signal(g_tmppath, &sig));

    char names[4][FLIPPER_IR_NAME_MAX_LEN];
    uint16_t n = read_names(g_tmppath, names, 4);
    TEST_ASSERT_EQUAL_UINT16(3, n);
    TEST_ASSERT_EQUAL_STRING("Sig0",        names[0]);
    TEST_ASSERT_EQUAL_STRING("Sig1",        names[1]);
    TEST_ASSERT_EQUAL_STRING("AppendedNew", names[2]);
}

void test_append_preserves_signal_data(void)
{
    write_test_file(g_tmppath, 1);
    flipper_ir_signal_t sig = make_parsed_sig("PowerBtn", 0x07, 0x02);
    TEST_ASSERT_TRUE(flipper_ir_append_signal(g_tmppath, &sig));

    flipper_file_t ff;
    flipper_ir_signal_t got;
    TEST_ASSERT_TRUE(flipper_ir_open(&ff, g_tmppath));
    flipper_ir_read_signal(&ff, &got); /* skip Sig0 */
    TEST_ASSERT_TRUE(flipper_ir_read_signal(&ff, &got));
    ff_close(&ff);

    TEST_ASSERT_EQUAL_STRING("PowerBtn", got.name);
    TEST_ASSERT_EQUAL_UINT16(0x07, got.parsed.address);
    TEST_ASSERT_EQUAL_UINT16(0x02, got.parsed.command);
}

void test_append_null_path_returns_false(void)
{
    flipper_ir_signal_t sig = make_parsed_sig("X", 0, 0);
    TEST_ASSERT_FALSE(flipper_ir_append_signal(NULL, &sig));
}

void test_append_null_sig_returns_false(void)
{
    write_test_file(g_tmppath, 1);
    TEST_ASSERT_FALSE(flipper_ir_append_signal(g_tmppath, NULL));
}

void test_append_invalid_sig_returns_false(void)
{
    write_test_file(g_tmppath, 1);
    flipper_ir_signal_t sig = make_parsed_sig("X", 0, 0);
    sig.valid = false;
    TEST_ASSERT_FALSE(flipper_ir_append_signal(g_tmppath, &sig));
}

/* ======================================================================= */
/* flipper_ir_raw_feed_t                                                    */
/* ======================================================================= */

void test_raw_feed_init(void)
{
    flipper_ir_raw_feed_t f;
    flipper_ir_raw_feed_init(&f, "MyRaw", 38000, 0.33f);
    TEST_ASSERT_EQUAL_STRING("MyRaw",  f.sig.name);
    TEST_ASSERT_EQUAL_UINT32(38000,    f.sig.raw.frequency);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.33f, f.sig.raw.duty_cycle);
    TEST_ASSERT_EQUAL_UINT16(0,        f.sig.raw.sample_count);
    TEST_ASSERT_FALSE(f.overflow);
    TEST_ASSERT_FALSE(f.sig.valid);
}

void test_raw_feed_init_null_name_uses_default(void)
{
    flipper_ir_raw_feed_t f;
    flipper_ir_raw_feed_init(&f, NULL, 38000, 0.33f);
    TEST_ASSERT_NOT_EQUAL(0, f.sig.name[0]);
}

void test_raw_feed_push_accepts_samples(void)
{
    flipper_ir_raw_feed_t f;
    flipper_ir_raw_feed_init(&f, "T", 38000, 0.33f);
    TEST_ASSERT_TRUE(flipper_ir_raw_feed_push(&f,  9000));
    TEST_ASSERT_TRUE(flipper_ir_raw_feed_push(&f, -4500));
    TEST_ASSERT_TRUE(flipper_ir_raw_feed_push(&f,   560));
    TEST_ASSERT_EQUAL_UINT16(3, f.sig.raw.sample_count);
    TEST_ASSERT_EQUAL_INT32(9000,  f.sig.raw.samples[0]);
    TEST_ASSERT_EQUAL_INT32(-4500, f.sig.raw.samples[1]);
    TEST_ASSERT_EQUAL_INT32(560,   f.sig.raw.samples[2]);
}

void test_raw_feed_finish_marks_valid(void)
{
    flipper_ir_raw_feed_t f;
    flipper_ir_raw_feed_init(&f, "T", 38000, 0.33f);
    flipper_ir_raw_feed_push(&f,  9000);
    flipper_ir_raw_feed_push(&f, -4500);
    TEST_ASSERT_TRUE(flipper_ir_raw_feed_finish(&f));
    TEST_ASSERT_TRUE(f.sig.valid);
}

void test_raw_feed_finish_adds_trailing_space_when_last_is_mark(void)
{
    flipper_ir_raw_feed_t f;
    flipper_ir_raw_feed_init(&f, "T", 38000, 0.33f);
    flipper_ir_raw_feed_push(&f, 560); /* mark — positive */
    uint16_t count_before = f.sig.raw.sample_count;
    TEST_ASSERT_TRUE(flipper_ir_raw_feed_finish(&f));
    /* A trailing space must have been added */
    TEST_ASSERT_GREATER_THAN(count_before, f.sig.raw.sample_count);
    TEST_ASSERT_LESS_THAN(0, f.sig.raw.samples[f.sig.raw.sample_count - 1]);
}

void test_raw_feed_finish_no_extra_space_when_last_is_space(void)
{
    flipper_ir_raw_feed_t f;
    flipper_ir_raw_feed_init(&f, "T", 38000, 0.33f);
    flipper_ir_raw_feed_push(&f,  9000);
    flipper_ir_raw_feed_push(&f, -4500); /* space — negative */
    uint16_t count_before = f.sig.raw.sample_count;
    TEST_ASSERT_TRUE(flipper_ir_raw_feed_finish(&f));
    TEST_ASSERT_EQUAL_UINT16(count_before, f.sig.raw.sample_count);
}

void test_raw_feed_empty_finish_returns_false(void)
{
    flipper_ir_raw_feed_t f;
    flipper_ir_raw_feed_init(&f, "T", 38000, 0.33f);
    TEST_ASSERT_FALSE(flipper_ir_raw_feed_finish(&f));
    TEST_ASSERT_FALSE(f.sig.valid);
}

void test_raw_feed_overflow_stops_push(void)
{
    flipper_ir_raw_feed_t f;
    flipper_ir_raw_feed_init(&f, "T", 38000, 0.33f);

    /* Fill to capacity */
    for (int i = 0; i < FLIPPER_IR_RAW_MAX_SAMPLES; i++)
        flipper_ir_raw_feed_push(&f, (i & 1) ? -500 : 500);

    TEST_ASSERT_EQUAL_UINT16(FLIPPER_IR_RAW_MAX_SAMPLES, f.sig.raw.sample_count);
    /* Next push must set overflow and return false */
    TEST_ASSERT_FALSE(flipper_ir_raw_feed_push(&f, 100));
    TEST_ASSERT_TRUE(f.overflow);
}

void test_raw_feed_finish_after_overflow_returns_false(void)
{
    flipper_ir_raw_feed_t f;
    flipper_ir_raw_feed_init(&f, "T", 38000, 0.33f);
    for (int i = 0; i <= FLIPPER_IR_RAW_MAX_SAMPLES; i++)
        flipper_ir_raw_feed_push(&f, 100);
    TEST_ASSERT_FALSE(flipper_ir_raw_feed_finish(&f));
}

void test_raw_feed_null_init_no_crash(void)
{
    flipper_ir_raw_feed_init(NULL, "T", 38000, 0.33f); /* should not crash */
}

void test_raw_feed_null_push_returns_false(void)
{
    TEST_ASSERT_FALSE(flipper_ir_raw_feed_push(NULL, 100));
}

/* ======================================================================= */
/* Combined: rename + delete + append sequence                              */
/* ======================================================================= */

void test_sequence_create_rename_delete_append(void)
{
    /* Start: 3 signals */
    write_test_file(g_tmppath, 3);

    /* Rename sig1 */
    TEST_ASSERT_TRUE(flipper_ir_rename_signal(g_tmppath, 1, "Vol_Up"));

    /* Delete sig0 */
    TEST_ASSERT_TRUE(flipper_ir_delete_signal(g_tmppath, 0));

    /* Append a new one */
    flipper_ir_signal_t mute = make_parsed_sig("Mute", 0x07, 0x0F);
    TEST_ASSERT_TRUE(flipper_ir_append_signal(g_tmppath, &mute));

    /* Expected: Vol_Up, Sig2, Mute */
    char names[4][FLIPPER_IR_NAME_MAX_LEN];
    uint16_t n = read_names(g_tmppath, names, 4);
    TEST_ASSERT_EQUAL_UINT16(3, n);
    TEST_ASSERT_EQUAL_STRING("Vol_Up", names[0]);
    TEST_ASSERT_EQUAL_STRING("Sig2",   names[1]);
    TEST_ASSERT_EQUAL_STRING("Mute",   names[2]);
}

/* ======================================================================= */
/* main                                                                     */
/* ======================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* rename */
    RUN_TEST(test_rename_first_signal);
    RUN_TEST(test_rename_middle_signal);
    RUN_TEST(test_rename_last_signal);
    RUN_TEST(test_rename_oob_returns_false);
    RUN_TEST(test_rename_null_path_returns_false);
    RUN_TEST(test_rename_empty_new_name_returns_false);
    RUN_TEST(test_rename_preserves_other_fields);
    RUN_TEST(test_rename_single_signal_file);

    /* delete */
    RUN_TEST(test_delete_first_signal);
    RUN_TEST(test_delete_middle_signal);
    RUN_TEST(test_delete_last_signal);
    RUN_TEST(test_delete_oob_returns_false);
    RUN_TEST(test_delete_null_path_returns_false);
    RUN_TEST(test_delete_only_signal_leaves_empty_file);
    RUN_TEST(test_delete_preserves_remaining_data);

    /* append */
    RUN_TEST(test_append_to_empty_file);
    RUN_TEST(test_append_to_existing_file);
    RUN_TEST(test_append_preserves_signal_data);
    RUN_TEST(test_append_null_path_returns_false);
    RUN_TEST(test_append_null_sig_returns_false);
    RUN_TEST(test_append_invalid_sig_returns_false);

    /* raw_feed */
    RUN_TEST(test_raw_feed_init);
    RUN_TEST(test_raw_feed_init_null_name_uses_default);
    RUN_TEST(test_raw_feed_push_accepts_samples);
    RUN_TEST(test_raw_feed_finish_marks_valid);
    RUN_TEST(test_raw_feed_finish_adds_trailing_space_when_last_is_mark);
    RUN_TEST(test_raw_feed_finish_no_extra_space_when_last_is_space);
    RUN_TEST(test_raw_feed_empty_finish_returns_false);
    RUN_TEST(test_raw_feed_overflow_stops_push);
    RUN_TEST(test_raw_feed_finish_after_overflow_returns_false);
    RUN_TEST(test_raw_feed_null_init_no_crash);
    RUN_TEST(test_raw_feed_null_push_returns_false);

    /* combined sequence */
    RUN_TEST(test_sequence_create_rename_delete_append);

    return UNITY_END();
}
