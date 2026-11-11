/* See COPYING.txt for license details. */

#include "unity.h"
#include "lfrfid.h"

LFRFID_TAG_INFO lfrfid_tag_info;
LFRFIDProgram *lfrfid_program;
EncodedTx_Data_t lfrfid_encoded_data;
uint8_t rfid_rxtx_is_taking_this_irq;
static Encoded_Data_t test_encoded_steps[ENCODED_DATA_MAX];

void lfrfid_emul_hw_init(void) {}
void bytes_to_u32_array(BitOrder order, const uint8_t in_data[], uint32_t out_data[], int size)
{
    for (int i = 0; i < size; i++) {
        if (order == BIT_ORDER_LSB_FIRST) {
            out_data[i] = (uint32_t)in_data[i * 4] |
                ((uint32_t)in_data[i * 4 + 1] << 8) |
                ((uint32_t)in_data[i * 4 + 2] << 16) |
                ((uint32_t)in_data[i * 4 + 3] << 24);
        } else {
            out_data[i] = ((uint32_t)in_data[i * 4] << 24) |
                ((uint32_t)in_data[i * 4 + 1] << 16) |
                ((uint32_t)in_data[i * 4 + 2] << 8) |
                (uint32_t)in_data[i * 4 + 3];
        }
    }
}

void t5577_execute_write(LFRFIDProgram *data, int block)
{
    (void)data;
    (void)block;
}

void setUp(void)
{
    memset(&lfrfid_tag_info, 0, sizeof(lfrfid_tag_info));
    memset(test_encoded_steps, 0, sizeof(test_encoded_steps));
    lfrfid_encoded_data.data = test_encoded_steps;
    lfrfid_encoded_data.length = 0;
    lfrfid_encoded_data.index = 0;
}

void tearDown(void) {}

#include "lfrfid_protocol_fdx_b.c"

static uint8_t test_get_bit(const uint8_t *data, size_t pos)
{
    return (data[pos / 8] >> (7 - pos % 8)) & 1U;
}

static void test_set_bit(uint8_t *data, size_t pos, uint8_t bit)
{
    if (bit) data[pos / 8] |= 1U << (7 - pos % 8);
    else data[pos / 8] &= ~(1U << (7 - pos % 8));
}

static void test_copy_bits(uint8_t *dst, size_t dpos, size_t len,
                           const uint8_t *src, size_t spos)
{
    for (size_t i = 0; i < len; i++)
        test_set_bit(dst, dpos + i, test_get_bit(src, spos + i));
}

static uint16_t test_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        for (size_t j = 0; j < 8; j++) {
            uint8_t c15 = (crc >> 15) & 1U;
            uint8_t bit = (data[i] >> (7 - j)) & 1U;
            crc <<= 1;
            if (c15 ^ bit) crc ^= 0x1021;
        }
    }
    return crc;
}

static void build_fdxb_frame(const uint8_t payload[FDX_B_DECODED_SIZE],
                             uint8_t frame[FDX_B_ENCODED_SIZE])
{
    memset(frame, 0, FDX_B_ENCODED_SIZE);
    test_set_bit(frame, 0, 1);
    for (size_t i = 0; i < 13; i++) {
        test_set_bit(frame, 11 + 9 * i, 1);
        if (i == 8 || i == 9) continue;
        if (i < 8)
            test_copy_bits(frame, 12 + 9 * i, 8, payload, i * 8);
        else
            test_copy_bits(frame, 12 + 9 * i, 8, payload, (i - 2) * 8);
    }
    uint16_t crc = test_crc16(payload, 8);
    test_copy_bits(frame, 84, 8, (const uint8_t *)&crc, 8);
    test_copy_bits(frame, 93, 8, (const uint8_t *)&crc, 0);
    test_set_bit(frame, 128, 1);
}

static size_t fdxb_events(const uint8_t *frame, lfrfid_evt_t *events)
{
    size_t count = 0;
    for (size_t bit = 0; bit < 128; bit++) {
        if (test_get_bit(frame, bit)) {
            events[count++].t_us = 256;
        } else {
            events[count++].t_us = 128;
            events[count++].t_us = 128;
        }
    }
    events[count++].t_us = 256;
    for (size_t i = 0; i < 15; i++) {
        events[count++].t_us = 128;
        events[count++].t_us = 128;
    }
    return count;
}

static const uint8_t test_payload[FDX_B_DECODED_SIZE] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x55, 0xAA, 0x01,
};

void test_fdxb_decodes_stock_frame(void)
{
    uint8_t frame[FDX_B_ENCODED_SIZE];
    lfrfid_evt_t events[260];
    build_fdxb_frame(test_payload, frame);
    size_t count = fdxb_events(frame, events);

    protocol_fdx_b.decoder.begin(NULL);
    TEST_ASSERT_TRUE(protocol_fdx_b.decoder.execute(events, count));
    TEST_ASSERT_EQUAL_UINT16(32, lfrfid_tag_info.bitrate);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(test_payload, lfrfid_tag_info.uid,
                                  FDX_B_DECODED_SIZE);

    const uint8_t *decoded = protocol_fdx_b.get_data(NULL);
    TEST_ASSERT_NOT_NULL(decoded);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(test_payload, decoded, FDX_B_DECODED_SIZE);
}

void test_fdxb_encoder_preserves_payload_and_repeat_edges(void)
{
    uint8_t expected_frame[FDX_B_ENCODED_SIZE];
    Encoded_Data_t expected_steps[ENCODED_DATA_MAX];
    build_fdxb_frame(test_payload, expected_frame);
    memcpy(lfrfid_tag_info.uid, test_payload,
           min(sizeof(lfrfid_tag_info.uid), (size_t)FDX_B_DECODED_SIZE));

    TEST_ASSERT_TRUE(protocol_fdx_b.encoder.begin(NULL));
    uint16_t expected_count = fdxb_build_wave(expected_frame, FDXB_ENCODED_BITS,
                                              expected_steps, ENCODED_DATA_MAX,
                                              FDXB_EMUL_GPIO_PIN);
    TEST_ASSERT_EQUAL_UINT16(193, expected_count);
    TEST_ASSERT_EQUAL_UINT16(expected_count * 2U, lfrfid_encoded_data.length);

    for(uint16_t i = 0; i < expected_count; i++) {
        TEST_ASSERT_EQUAL_UINT32(expected_steps[i].bsrr,
                                 lfrfid_encoded_data.data[i].bsrr);
        TEST_ASSERT_EQUAL_UINT16(expected_steps[i].time_us,
                                 lfrfid_encoded_data.data[i].time_us);
    }
    for(uint16_t i = 0; i < lfrfid_encoded_data.length; i++) {
        uint16_t next = (i + 1U) % lfrfid_encoded_data.length;
        TEST_ASSERT_NOT_EQUAL(lfrfid_encoded_data.data[i].bsrr,
                              lfrfid_encoded_data.data[next].bsrr);
    }
}

void test_fdxb_write_preserves_all_payload_blocks(void)
{
    uint8_t expected_frame[FDX_B_ENCODED_SIZE];
    uint32_t expected_blocks[4];
    LFRFIDProgram program;
    build_fdxb_frame(test_payload, expected_frame);
    bytes_to_u32_array(BIT_ORDER_MSB_FIRST, expected_frame, expected_blocks, 4);
    memcpy(lfrfid_tag_info.uid, test_payload,
           min(sizeof(lfrfid_tag_info.uid), (size_t)FDX_B_DECODED_SIZE));
    memset(&program, 0, sizeof(program));
    program.type = LFRFIDProgramTypeT5577;

    protocol_fdx_b.write.begin(&lfrfid_tag_info, &program);

    TEST_ASSERT_EQUAL_UINT32(5, program.t5577.max_blocks);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(expected_blocks, &program.t5577.block_data[1], 4);
}

void test_fdxb_rejects_wrong_header(void)
{
    uint8_t frame[FDX_B_ENCODED_SIZE];
    lfrfid_evt_t events[260];
    build_fdxb_frame(test_payload, frame);
    test_set_bit(frame, 0, 0);
    size_t count = fdxb_events(frame, events);

    protocol_fdx_b.decoder.begin(NULL);
    TEST_ASSERT_FALSE(protocol_fdx_b.decoder.execute(events, count));
}

void test_em4100_rejects_normalization_overflow(void)
{
    lfrfid_evt_t events[64];
    for (size_t i = 0; i < 64; i++) {
        events[i].t_us = 96;
        events[i].edge = (i & 1U) ? LFRFID_EDGE_RISE : LFRFID_EDGE_FALL;
    }

    protocol_em4100_16.decoder.begin(NULL);
    TEST_ASSERT_FALSE(protocol_em4100_16.decoder.execute(events, 64));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fdxb_decodes_stock_frame);
    RUN_TEST(test_fdxb_encoder_preserves_payload_and_repeat_edges);
    RUN_TEST(test_fdxb_write_preserves_all_payload_blocks);
    RUN_TEST(test_fdxb_rejects_wrong_header);
    RUN_TEST(test_em4100_rejects_normalization_overflow);
    return UNITY_END();
}
