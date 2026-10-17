/* See COPYING.txt for license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "subghz_raw_pairs_writer.h"

static bool write_line(void *context, const char *line)
{
    return fputs(line, (FILE *)context) >= 0;
}

void setUp(void) {}
void tearDown(void) {}

void test_all_pairs_are_written_including_unterminated_final_repetition(void)
{
    const SubGhzRawPair pairs[] = {
        {100u, 200u}, {101u, 201u}, {102u, 202u}, {103u, 203u},
        {104u, 204u}, {105u, 205u}, {106u, 206u},
    };
    FILE *file = tmpfile();
    TEST_ASSERT_NOT_NULL(file);
    char line[64];

    TEST_ASSERT_TRUE(subghz_raw_pairs_write_data(
        pairs, 7u, 3u, line, sizeof(line), write_line, file));
    rewind(file);

    uint32_t actual[14];
    uint32_t count = 0;
    char read_line[64];
    while (fgets(read_line, sizeof(read_line), file) != NULL)
    {
        char *p = strchr(read_line, ':');
        TEST_ASSERT_NOT_NULL(p);
        p++;
        while (*p != '\0')
        {
            while (*p == ' ' || *p == '\r' || *p == '\n')
                p++;
            if (*p == '\0')
                break;
            TEST_ASSERT_TRUE(count < 14u);
            actual[count++] = (uint32_t)strtoul(p, &p, 10);
        }
    }
    fclose(file);

    TEST_ASSERT_EQUAL_UINT32(14u, count);
    for (uint32_t i = 0; i < 7u; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(pairs[i].high_us, actual[2u * i]);
        TEST_ASSERT_EQUAL_UINT32(pairs[i].low_us, actual[2u * i + 1u]);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_all_pairs_are_written_including_unterminated_final_repetition);
    return UNITY_END();
}
