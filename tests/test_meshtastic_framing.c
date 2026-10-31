#include "unity.h"
#include "meshtastic_framing.h"

#include <string.h>

static meshtastic_framing_t framing;

void setUp(void) {
    memset(&framing, 0xA5, sizeof(framing));
    meshtastic_framing_reset(&framing);
}

void tearDown(void) { }

static meshtastic_frame_result_t feed(const uint8_t* data, size_t len) {
    meshtastic_frame_result_t result = MESHTASTIC_FRAME_PENDING;
    for(size_t i = 0; i < len; i++) {
        result = meshtastic_framing_feed(&framing, data[i]);
        if(result != MESHTASTIC_FRAME_PENDING) return result;
    }
    return result;
}

void test_frame_decodes_across_fragment_boundaries(void) {
    static const uint8_t first[] = {0x00, ZEROMESH_MAGIC0, ZEROMESH_MAGIC1, 0x00};
    static const uint8_t second[] = {0x03, 0x10, 0x20, 0x30};

    TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_BAD_MAGIC, feed(first, 1));
    TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_PENDING, feed(first + 1, 3));
    TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_COMPLETE, feed(second, sizeof(second)));
    TEST_ASSERT_EQUAL(3, framing.frame_len);
    TEST_ASSERT_EQUAL(3, framing.frame_pos);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(second + 1, framing.frame_buf, 3);
}

void test_magic_one_inside_bad_header_is_reused(void) {
    static const uint8_t input[] = {
        ZEROMESH_MAGIC0, ZEROMESH_MAGIC0, ZEROMESH_MAGIC1, 0x00, 0x01, 0x42,
    };

    TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_BAD_MAGIC, feed(input, 2));
    TEST_ASSERT_EQUAL(1, framing.hdr_pos);
    TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_COMPLETE, feed(input + 2, sizeof(input) - 2));
    TEST_ASSERT_EQUAL_HEX8(0x42, framing.frame_buf[0]);
}

void test_zero_length_is_rejected_and_next_frame_recovers(void) {
    static const uint8_t input[] = {
        ZEROMESH_MAGIC0, ZEROMESH_MAGIC1, 0x00, 0x00,
        ZEROMESH_MAGIC0, ZEROMESH_MAGIC1, 0x00, 0x01, 0x77,
    };

    TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_BAD_LENGTH, feed(input, 4));
    TEST_ASSERT_EQUAL(0, framing.hdr_pos);
    TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_COMPLETE, feed(input + 4, sizeof(input) - 4));
    TEST_ASSERT_EQUAL_HEX8(0x77, framing.frame_buf[0]);
}

void test_oversize_length_is_rejected_before_payload_write(void) {
    static const uint8_t input[] = {
        ZEROMESH_MAGIC0, ZEROMESH_MAGIC1, (MAX_FRAME_SIZE + 1) >> 8,
        (MAX_FRAME_SIZE + 1) & 0xFF,
    };

    TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_BAD_LENGTH, feed(input, sizeof(input)));
    TEST_ASSERT_EQUAL(0, framing.frame_pos);
    TEST_ASSERT_EQUAL(0, framing.frame_len);
}

void test_maximum_size_frame_is_bounded(void) {
    static const uint8_t header[] = {
        ZEROMESH_MAGIC0, ZEROMESH_MAGIC1, MAX_FRAME_SIZE >> 8, MAX_FRAME_SIZE & 0xFF,
    };

    TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_PENDING, feed(header, sizeof(header)));
    for(size_t i = 0; i < MAX_FRAME_SIZE; i++) {
        meshtastic_frame_result_t result = meshtastic_framing_feed(&framing, (uint8_t)i);
        if(i + 1 < MAX_FRAME_SIZE) {
            TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_PENDING, result);
        } else {
            TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_COMPLETE, result);
        }
    }
    TEST_ASSERT_EQUAL(MAX_FRAME_SIZE, framing.frame_pos);
    TEST_ASSERT_EQUAL_HEX8(0xFF, framing.frame_buf[MAX_FRAME_SIZE - 1]);
}

void test_reset_clears_partial_frame(void) {
    static const uint8_t partial[] = {ZEROMESH_MAGIC0, ZEROMESH_MAGIC1, 0x00, 0x02, 0x55};

    TEST_ASSERT_EQUAL(MESHTASTIC_FRAME_PENDING, feed(partial, sizeof(partial)));
    meshtastic_framing_reset(&framing);
    TEST_ASSERT_EQUAL(0, framing.hdr_pos);
    TEST_ASSERT_EQUAL(0, framing.frame_pos);
    TEST_ASSERT_EQUAL(0, framing.frame_len);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_frame_decodes_across_fragment_boundaries);
    RUN_TEST(test_magic_one_inside_bad_header_is_reused);
    RUN_TEST(test_zero_length_is_rejected_and_next_frame_recovers);
    RUN_TEST(test_oversize_length_is_rejected_before_payload_write);
    RUN_TEST(test_maximum_size_frame_is_bounded);
    RUN_TEST(test_reset_clears_partial_frame);
    return UNITY_END();
}
