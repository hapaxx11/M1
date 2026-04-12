/* Host-side regression tests for esp_queue (linked-list queue).
 *
 * Bug context: The original esp_queue_get() freed the dequeued node
 * BEFORE clearing q->rear, creating a window where a concurrent
 * esp_queue_put() could dereference freed memory (use-after-free).
 * The fix reorders operations so rear is updated inside a
 * vTaskSuspendAll() critical section before the node is freed.
 *
 * These tests verify correctness under ASan — any use-after-free or
 * memory leak will be caught by the sanitizer.
 */

#include "unity.h"
#include "esp_queue.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* Helper: create a heap-allocated int for queue data                   */
/* ------------------------------------------------------------------ */
static void *make_int(int val)
{
    int *p = malloc(sizeof(int));
    TEST_ASSERT_NOT_NULL(p);
    *p = val;
    return p;
}

/* ------------------------------------------------------------------ */
/* Basic: create, put one, get one, destroy                            */
/* ------------------------------------------------------------------ */
void test_put_get_single(void)
{
    esp_queue_t *q = create_esp_queue();
    TEST_ASSERT_NOT_NULL(q);

    TEST_ASSERT_FALSE(esp_queue_check(q));

    int *data = make_int(42);
    TEST_ASSERT_EQUAL_INT(ESP_QUEUE_SUCCESS, esp_queue_put(q, data));
    TEST_ASSERT_TRUE(esp_queue_check(q));

    void *out = esp_queue_get(q);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_INT(42, *(int *)out);
    free(out);

    TEST_ASSERT_FALSE(esp_queue_check(q));
    TEST_ASSERT_NULL(esp_queue_get(q));

    esp_queue_destroy(&q);
    TEST_ASSERT_NULL(q);
}

/* ------------------------------------------------------------------ */
/* FIFO ordering with multiple elements                                */
/* ------------------------------------------------------------------ */
void test_fifo_order(void)
{
    esp_queue_t *q = create_esp_queue();
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_EQUAL_INT(ESP_QUEUE_SUCCESS, esp_queue_put(q, make_int(i)));

    for (int i = 0; i < 5; i++) {
        void *out = esp_queue_get(q);
        TEST_ASSERT_NOT_NULL(out);
        TEST_ASSERT_EQUAL_INT(i, *(int *)out);
        free(out);
    }
    TEST_ASSERT_NULL(esp_queue_get(q));
    esp_queue_destroy(&q);
}

/* ------------------------------------------------------------------ */
/* Interleaved put/get — the pattern that triggers the old race         */
/* This exercises the single-element queue case where get() must        */
/* update both front and rear pointers atomically.                      */
/*                                                                      */
/* NOTE: This is a serial regression test — it cannot reproduce the     */
/* actual race (which requires task preemption between free() and       */
/* q->rear = NULL).  It validates the post-fix code path under ASan;   */
/* the real concurrency guarantee comes from scheduler suspension in    */
/* the production code.                                                 */
/* ------------------------------------------------------------------ */
void test_interleaved_put_get(void)
{
    esp_queue_t *q = create_esp_queue();

    for (int round = 0; round < 100; round++) {
        /* Put one element — queue has exactly one node */
        TEST_ASSERT_EQUAL_INT(ESP_QUEUE_SUCCESS, esp_queue_put(q, make_int(round)));
        TEST_ASSERT_TRUE(esp_queue_check(q));

        /* Get that element — this was the buggy path: rear pointed to
         * freed memory between free(temp) and q->rear = NULL. */
        void *out = esp_queue_get(q);
        TEST_ASSERT_NOT_NULL(out);
        TEST_ASSERT_EQUAL_INT(round, *(int *)out);
        free(out);

        /* Queue must be empty with both front and rear == NULL.
         * A subsequent put must succeed without corrupting memory. */
        TEST_ASSERT_FALSE(esp_queue_check(q));
    }

    esp_queue_destroy(&q);
}

/* ------------------------------------------------------------------ */
/* Reset clears all elements and frees their data                      */
/* ------------------------------------------------------------------ */
void test_reset(void)
{
    esp_queue_t *q = create_esp_queue();
    for (int i = 0; i < 10; i++)
        TEST_ASSERT_EQUAL_INT(ESP_QUEUE_SUCCESS, esp_queue_put(q, make_int(i)));
    TEST_ASSERT_TRUE(esp_queue_check(q));

    esp_queue_reset(q);
    TEST_ASSERT_FALSE(esp_queue_check(q));
    TEST_ASSERT_NULL(esp_queue_get(q));

    /* Queue must be reusable after reset */
    TEST_ASSERT_EQUAL_INT(ESP_QUEUE_SUCCESS, esp_queue_put(q, make_int(99)));
    void *out = esp_queue_get(q);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_INT(99, *(int *)out);
    free(out);

    esp_queue_destroy(&q);
}

/* ------------------------------------------------------------------ */
/* Rapid put-get cycles stress ASan for use-after-free detection        */
/* ------------------------------------------------------------------ */
void test_rapid_cycles(void)
{
    esp_queue_t *q = create_esp_queue();

    for (int i = 0; i < 1000; i++) {
        TEST_ASSERT_EQUAL_INT(ESP_QUEUE_SUCCESS, esp_queue_put(q, make_int(i)));
        void *out = esp_queue_get(q);
        TEST_ASSERT_NOT_NULL(out);
        TEST_ASSERT_EQUAL_INT(i, *(int *)out);
        free(out);
    }
    TEST_ASSERT_FALSE(esp_queue_check(q));

    esp_queue_destroy(&q);
}

/* ------------------------------------------------------------------ */
/* Edge: get/check/reset on NULL or empty queue                         */
/* ------------------------------------------------------------------ */
void test_null_and_empty(void)
{
    TEST_ASSERT_NULL(esp_queue_get(NULL));
    TEST_ASSERT_FALSE(esp_queue_check(NULL));

    esp_queue_t *q = create_esp_queue();
    TEST_ASSERT_NULL(esp_queue_get(q));
    TEST_ASSERT_FALSE(esp_queue_check(q));

    esp_queue_reset(q);   /* reset on empty — must not crash */
    TEST_ASSERT_FALSE(esp_queue_check(q));

    esp_queue_destroy(&q);
}

/* ------------------------------------------------------------------ */
/* Edge: put with NULL queue                                            */
/* ------------------------------------------------------------------ */
void test_put_null_queue(void)
{
    int *data = make_int(0);
    TEST_ASSERT_EQUAL_INT(ESP_QUEUE_ERR_UNINITALISED, esp_queue_put(NULL, data));
    /* The queue rejected the data, so we must free it ourselves. */
    free(data);
}

/* ------------------------------------------------------------------ */
/* Destroy sets pointer to NULL                                         */
/* ------------------------------------------------------------------ */
void test_destroy(void)
{
    esp_queue_t *q = create_esp_queue();
    TEST_ASSERT_EQUAL_INT(ESP_QUEUE_SUCCESS, esp_queue_put(q, make_int(1)));
    TEST_ASSERT_EQUAL_INT(ESP_QUEUE_SUCCESS, esp_queue_put(q, make_int(2)));
    esp_queue_destroy(&q);
    TEST_ASSERT_NULL(q);
}

/* ------------------------------------------------------------------ */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_put_get_single);
    RUN_TEST(test_fifo_order);
    RUN_TEST(test_interleaved_put_get);
    RUN_TEST(test_reset);
    RUN_TEST(test_rapid_cycles);
    RUN_TEST(test_null_and_empty);
    RUN_TEST(test_put_null_queue);
    RUN_TEST(test_destroy);
    return UNITY_END();
}
