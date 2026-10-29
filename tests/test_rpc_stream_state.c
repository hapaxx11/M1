#include "unity.h"
#include "m1_rpc_stream_state.h"
#include <stdio.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

void setUp(void) {}
void tearDown(void) {}

void test_pre_init_display_cannot_wait_for_route_mutex(void)
{
    char path[512];
    char source[131072];
    FILE *fp;
    size_t len;
    char *getter;
    char *guard;
    char *take;

    snprintf(path, sizeof(path), "%s/m1_csrc/m1_rpc.c", M1_ROOT);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    len = fread(source, 1, sizeof(source) - 1u, fp);
    fclose(fp);
    TEST_ASSERT_LESS_THAN(sizeof(source) - 1u, len);
    source[len] = '\0';

    getter = strstr(source, "static bool rpc_screen_stream_active_get(void)");
    TEST_ASSERT_NOT_NULL(getter);
    guard = strstr(getter, "m1_rpc_screen_stream_lock_ready(s_route_mutex)");
    take = strstr(getter, "xSemaphoreTake(s_route_mutex");
    TEST_ASSERT_NOT_NULL(guard);
    TEST_ASSERT_NOT_NULL(take);
    TEST_ASSERT_LESS_THAN(take, guard);
}

void test_route_mutex_readiness_is_explicit(void)
{
    int mutex;
    TEST_ASSERT_FALSE(m1_rpc_screen_stream_lock_ready(NULL));
    TEST_ASSERT_TRUE(m1_rpc_screen_stream_lock_ready(&mutex));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pre_init_display_cannot_wait_for_route_mutex);
    RUN_TEST(test_route_mutex_readiness_is_explicit);
    return UNITY_END();
}
