#include "unity.h"
#include <stdio.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

void setUp(void) {}
void tearDown(void) {}

void test_deep_init_task_has_four_kb_stack(void)
{
    char path[512];
    char source[32768];
    FILE *fp;
    size_t len;

    snprintf(path, sizeof(path), "%s/m1_csrc/m1_sys_init.c", M1_ROOT);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    len = fread(source, 1, sizeof(source) - 1u, fp);
    fclose(fp);
    TEST_ASSERT_LESS_THAN(sizeof(source) - 1u, len);
    source[len] = '\0';

    TEST_ASSERT_NOT_NULL(strstr(source,
        "xTaskCreate(m1_system_init_task, \"m1_system_init_task_n\", M1_TASK_STACK_SIZE_1024"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_deep_init_task_has_four_kb_stack);
    return UNITY_END();
}
