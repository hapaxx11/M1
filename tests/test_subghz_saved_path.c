/* See COPYING.txt for license details. */

#include "unity.h"
#include "subghz_saved_path.h"

#include <string.h>

void setUp(void) { }
void tearDown(void) { }

void test_build_root_and_nested_saved_paths(void)
{
    char path[64];

    TEST_ASSERT_TRUE(subghz_saved_path_build(path, sizeof(path), "0:/SUBGHZ",
                                             "Signal.sub"));
    TEST_ASSERT_EQUAL_STRING("/SUBGHZ/Signal.sub", path);
    TEST_ASSERT_TRUE(subghz_saved_path_build(
        path, sizeof(path), "0:/SUBGHZ/Pagers/Food Court", "Table_1.sub"));
    TEST_ASSERT_EQUAL_STRING(
        "/SUBGHZ/Pagers/Food Court/Table_1.sub", path);
}

void test_reject_paths_outside_saved_root_and_traversal(void)
{
    char path[64];

    TEST_ASSERT_FALSE(subghz_saved_path_build(path, sizeof(path),
                                              "0:/System", "Signal.sub"));
    TEST_ASSERT_FALSE(subghz_saved_path_build(
        path, sizeof(path), "/SUBGHZ/../System", "Signal.sub"));
    TEST_ASSERT_FALSE(subghz_saved_path_build(path, sizeof(path),
                                              "/SUBGHZ", "../Signal.sub"));
    TEST_ASSERT_FALSE(subghz_saved_path_build(path, sizeof(path),
                                              "/SUBGHZ", "nested/file.sub"));
    TEST_ASSERT_FALSE(subghz_saved_path_build(path, sizeof(path),
                                              "/SUBGHZ", ""));
    TEST_ASSERT_FALSE(subghz_saved_path_build(path, 4U, "/SUBGHZ",
                                              "Signal.sub"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_root_and_nested_saved_paths);
    RUN_TEST(test_reject_paths_outside_saved_root_and_traversal);
    return UNITY_END();
}
