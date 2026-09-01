/* See COPYING.txt for license details. */

/**
 * @file   test_espnow_trigger.c
 * @brief  Host-side unit tests for the ESP-NOW danger-gated remote trigger.
 */

#include "unity.h"
#include "espnow_trigger.h"

#include <string.h>

void setUp(void)  {}
void tearDown(void) {}

/* =========================================================================
 * Wire framing
 * =========================================================================*/

void test_request_build_parse_roundtrip(void)
{
    uint8_t frame[64];
    size_t  flen = 0;
    TEST_ASSERT_TRUE(espnow_trig_build_request(ESPNOW_SHARE_KIND_SUBGHZ,
                                               "garage.sub", frame, sizeof(frame), &flen));
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_TRIG_MSG_REQUEST, frame[0]);

    espnow_share_kind_t kind = ESPNOW_SHARE_KIND_UNKNOWN;
    char name[ESPNOW_TRIG_NAME_MAX + 1];
    TEST_ASSERT_TRUE(espnow_trig_parse_request(frame, flen, &kind, name, sizeof(name)));
    TEST_ASSERT_EQUAL_INT(ESPNOW_SHARE_KIND_SUBGHZ, kind);
    TEST_ASSERT_EQUAL_STRING("garage.sub", name);
}

void test_request_build_rejects_unsafe_name(void)
{
    uint8_t frame[64];
    size_t flen;
    TEST_ASSERT_FALSE(espnow_trig_build_request(ESPNOW_SHARE_KIND_SUBGHZ,
                                                "../evil", frame, sizeof(frame), &flen));
}

void test_request_parse_rejects_unsafe_name(void)
{
    /* Hand-craft a REQUEST carrying a traversal name. */
    uint8_t frame[16] = { ESPNOW_TRIG_MSG_REQUEST, ESPNOW_SHARE_KIND_SUBGHZ,
                          '.', '.', '/', 'x' };
    espnow_share_kind_t kind;
    char name[ESPNOW_TRIG_NAME_MAX + 1];
    TEST_ASSERT_FALSE(espnow_trig_parse_request(frame, 6, &kind, name, sizeof(name)));
}

void test_request_parse_rejects_out_of_range_kind(void)
{
    uint8_t frame[16] = { ESPNOW_TRIG_MSG_REQUEST, 0xFFu, 'x', '.', 's', 'u', 'b' };
    espnow_share_kind_t kind = ESPNOW_SHARE_KIND_UNKNOWN;
    char name[ESPNOW_TRIG_NAME_MAX + 1];
    TEST_ASSERT_FALSE(espnow_trig_parse_request(frame, 7, &kind, name, sizeof(name)));
}

void test_request_parse_rejects_embedded_nul_name(void)
{
    uint8_t frame[16] = { ESPNOW_TRIG_MSG_REQUEST, ESPNOW_SHARE_KIND_SUBGHZ,
                          'o', 'k', '\0', 'x' };
    espnow_share_kind_t kind;
    char name[ESPNOW_TRIG_NAME_MAX + 1];
    TEST_ASSERT_FALSE(espnow_trig_parse_request(frame, 6, &kind, name, sizeof(name)));
}

void test_request_parse_rejects_wrong_type(void)
{
    uint8_t frame[8] = { ESPNOW_TRIG_MSG_ACCEPT, 0, 'x' };
    espnow_share_kind_t kind;
    char name[ESPNOW_TRIG_NAME_MAX + 1];
    TEST_ASSERT_FALSE(espnow_trig_parse_request(frame, 3, &kind, name, sizeof(name)));
}

void test_status_build_parse(void)
{
    uint8_t frame[4];
    size_t  flen = 0;
    TEST_ASSERT_TRUE(espnow_trig_build_status(ESPNOW_TRIG_MSG_REJECT,
                                              ESPNOW_TRIG_REJECT_DENIED,
                                              frame, sizeof(frame), &flen));
    TEST_ASSERT_EQUAL_UINT(2, flen);

    espnow_trig_msg_t type;
    uint8_t code = 0;
    TEST_ASSERT_TRUE(espnow_trig_parse_status(frame, flen, &type, &code));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_MSG_REJECT, type);
    TEST_ASSERT_EQUAL_UINT8(ESPNOW_TRIG_REJECT_DENIED, code);
}

void test_status_build_rejects_bad_type(void)
{
    uint8_t frame[4];
    size_t flen;
    TEST_ASSERT_FALSE(espnow_trig_build_status(ESPNOW_TRIG_MSG_REQUEST, 0,
                                               frame, sizeof(frame), &flen));
}

/* =========================================================================
 * Happy path — full exchange
 * =========================================================================*/

void test_full_exchange_success(void)
{
    espnow_trigger_ctx_t init, resp;
    espnow_trigger_init(&init, ESPNOW_TRIG_ROLE_INITIATOR, false);
    espnow_trigger_init(&resp, ESPNOW_TRIG_ROLE_RESPONDER, true /* opted in */);

    /* Initiator sends request. */
    TEST_ASSERT_TRUE(espnow_trigger_request_sent(&init, ESPNOW_SHARE_KIND_IR, "tv.ir"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_REQ_SENT, init.state);

    /* Responder receives it → awaits consent. */
    TEST_ASSERT_TRUE(espnow_trigger_on_request(&resp, ESPNOW_SHARE_KIND_IR, "tv.ir"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_REQ_RECEIVED, resp.state);

    /* User grants → executing. */
    TEST_ASSERT_TRUE(espnow_trigger_grant(&resp));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_EXECUTING, resp.state);

    /* Initiator sees ACCEPT. */
    TEST_ASSERT_TRUE(espnow_trigger_on_accept(&init));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_AWAIT_RESULT, init.state);

    /* Responder finishes replay OK, sends RESULT. */
    TEST_ASSERT_TRUE(espnow_trigger_execution_done(&resp, true));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_DONE, resp.state);

    /* Initiator sees RESULT OK. */
    TEST_ASSERT_TRUE(espnow_trigger_on_result(&init, ESPNOW_TRIG_RESULT_OK));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_DONE, init.state);
}

/* =========================================================================
 * Safety gates
 * =========================================================================*/

void test_responder_auto_rejects_when_disabled(void)
{
    espnow_trigger_ctx_t resp;
    espnow_trigger_init(&resp, ESPNOW_TRIG_ROLE_RESPONDER, false /* NOT opted in */);
    TEST_ASSERT_FALSE(espnow_trigger_on_request(&resp, ESPNOW_SHARE_KIND_SUBGHZ, "x.sub"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_REJECTED, resp.state);
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_REJECT_DISABLED, resp.reject_reason);
}

void test_responder_rejects_bad_name(void)
{
    espnow_trigger_ctx_t resp;
    espnow_trigger_init(&resp, ESPNOW_TRIG_ROLE_RESPONDER, true);
    TEST_ASSERT_FALSE(espnow_trigger_on_request(&resp, ESPNOW_SHARE_KIND_SUBGHZ, "../x"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_REJECTED, resp.state);
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_REJECT_BAD_NAME, resp.reject_reason);
}

void test_responder_rejects_unknown_kind(void)
{
    espnow_trigger_ctx_t resp;
    espnow_trigger_init(&resp, ESPNOW_TRIG_ROLE_RESPONDER, true);
    TEST_ASSERT_FALSE(espnow_trigger_on_request(&resp, ESPNOW_SHARE_KIND_UNKNOWN, "file.sub"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_REJECTED, resp.state);
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_REJECT_BAD_NAME, resp.reject_reason);
}

void test_responder_rejects_out_of_range_kind(void)
{
    espnow_trigger_ctx_t resp;
    espnow_trigger_init(&resp, ESPNOW_TRIG_ROLE_RESPONDER, true);
    TEST_ASSERT_FALSE(espnow_trigger_on_request(&resp, (espnow_share_kind_t)0xFFu, "file.sub"));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_REJECTED, resp.state);
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_REJECT_BAD_NAME, resp.reject_reason);
}

void test_responder_deny_path(void)
{
    espnow_trigger_ctx_t resp;
    espnow_trigger_init(&resp, ESPNOW_TRIG_ROLE_RESPONDER, true);
    espnow_trigger_on_request(&resp, ESPNOW_SHARE_KIND_SUBGHZ, "x.sub");
    TEST_ASSERT_TRUE(espnow_trigger_deny(&resp));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_REJECTED, resp.state);
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_REJECT_DENIED, resp.reject_reason);
}

void test_grant_requires_pending_request(void)
{
    espnow_trigger_ctx_t resp;
    espnow_trigger_init(&resp, ESPNOW_TRIG_ROLE_RESPONDER, true);
    /* No request received yet — cannot grant. */
    TEST_ASSERT_FALSE(espnow_trigger_grant(&resp));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_IDLE, resp.state);
}

void test_execution_failure_marks_rejected(void)
{
    espnow_trigger_ctx_t resp;
    espnow_trigger_init(&resp, ESPNOW_TRIG_ROLE_RESPONDER, true);
    espnow_trigger_on_request(&resp, ESPNOW_SHARE_KIND_IR, "a.ir");
    espnow_trigger_grant(&resp);
    TEST_ASSERT_TRUE(espnow_trigger_execution_done(&resp, false));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_REJECTED, resp.state);
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_RESULT_FAIL, resp.result);
}

/* =========================================================================
 * Role/state guards
 * =========================================================================*/

void test_initiator_cannot_grant(void)
{
    espnow_trigger_ctx_t init;
    espnow_trigger_init(&init, ESPNOW_TRIG_ROLE_INITIATOR, false);
    espnow_trigger_request_sent(&init, ESPNOW_SHARE_KIND_SUBGHZ, "x.sub");
    TEST_ASSERT_FALSE(espnow_trigger_grant(&init));
}

void test_initiator_reject_records_reason(void)
{
    espnow_trigger_ctx_t init;
    espnow_trigger_init(&init, ESPNOW_TRIG_ROLE_INITIATOR, false);
    espnow_trigger_request_sent(&init, ESPNOW_SHARE_KIND_SUBGHZ, "x.sub");
    TEST_ASSERT_TRUE(espnow_trigger_on_reject(&init, ESPNOW_TRIG_REJECT_NOT_FOUND));
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_STATE_REJECTED, init.state);
    TEST_ASSERT_EQUAL_INT(ESPNOW_TRIG_REJECT_NOT_FOUND, init.reject_reason);
}

void test_result_before_accept_is_invalid(void)
{
    espnow_trigger_ctx_t init;
    espnow_trigger_init(&init, ESPNOW_TRIG_ROLE_INITIATOR, false);
    espnow_trigger_request_sent(&init, ESPNOW_SHARE_KIND_SUBGHZ, "x.sub");
    /* Result cannot arrive before ACCEPT. */
    TEST_ASSERT_FALSE(espnow_trigger_on_result(&init, ESPNOW_TRIG_RESULT_OK));
}

/* =========================================================================
 * Runner
 * =========================================================================*/

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_request_build_parse_roundtrip);
    RUN_TEST(test_request_build_rejects_unsafe_name);
    RUN_TEST(test_request_parse_rejects_unsafe_name);
    RUN_TEST(test_request_parse_rejects_out_of_range_kind);
    RUN_TEST(test_request_parse_rejects_embedded_nul_name);
    RUN_TEST(test_request_parse_rejects_wrong_type);
    RUN_TEST(test_status_build_parse);
    RUN_TEST(test_status_build_rejects_bad_type);
    RUN_TEST(test_full_exchange_success);
    RUN_TEST(test_responder_auto_rejects_when_disabled);
    RUN_TEST(test_responder_rejects_bad_name);
    RUN_TEST(test_responder_rejects_unknown_kind);
    RUN_TEST(test_responder_rejects_out_of_range_kind);
    RUN_TEST(test_responder_deny_path);
    RUN_TEST(test_grant_requires_pending_request);
    RUN_TEST(test_execution_failure_marks_rejected);
    RUN_TEST(test_initiator_cannot_grant);
    RUN_TEST(test_initiator_reject_records_reason);
    RUN_TEST(test_result_before_accept_is_invalid);
    return UNITY_END();
}
