/* See COPYING.txt for license details. */

/**
 * @file   espnow_trigger.h
 * @brief  ESP-NOW danger-gated remote trigger — pure logic.
 *
 * Phase 3 of the peer link lets a paired M1 ask the peer to **replay a named
 * saved capture the peer already holds** (Sub-GHz / IR / …).  It deliberately
 * does NOT allow arbitrary remote actions — only "replay saved item X".
 *
 * Safety is layered:
 *   - the responder must have opted in (`allow_remote` per session),
 *   - the requested name is validated (espnow_share_name_is_safe),
 *   - the responding user must explicitly grant each request, and
 *   - the FSM never transitions to EXECUTING without both consent and a valid,
 *     shareable request.
 *
 * This module models only the request/consent/execute/result state machine and
 * its wire framing (app type block 0x30, see espnow_appmsg.h).  The actual
 * radio replay is performed by the scene on top of it and is out of scope here.
 *
 * No HAL, RTOS, or display dependencies.  Host-testable.
 *
 * M1 Project
 */

#ifndef ESPNOW_TRIGGER_H_
#define ESPNOW_TRIGGER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "espnow_appmsg.h"
#include "espnow_shareable.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum capture-name length carried in a request. */
#define ESPNOW_TRIG_NAME_MAX     48u

/** Wire message types (within the 0x30 app block). */
typedef enum {
    ESPNOW_TRIG_MSG_REQUEST = ESPNOW_APP_TRIGGER_BASE + 0u, /* 0x30 */
    ESPNOW_TRIG_MSG_ACCEPT  = ESPNOW_APP_TRIGGER_BASE + 1u, /* 0x31 */
    ESPNOW_TRIG_MSG_REJECT  = ESPNOW_APP_TRIGGER_BASE + 2u, /* 0x32 */
    ESPNOW_TRIG_MSG_RESULT  = ESPNOW_APP_TRIGGER_BASE + 3u, /* 0x33 */
} espnow_trig_msg_t;

/** Reason a request was rejected (carried in a REJECT frame). */
typedef enum {
    ESPNOW_TRIG_REJECT_NONE = 0,
    ESPNOW_TRIG_REJECT_DISABLED,   /**< Remote trigger not enabled on responder. */
    ESPNOW_TRIG_REJECT_DENIED,     /**< User declined the request. */
    ESPNOW_TRIG_REJECT_NOT_FOUND,  /**< Named capture not present on responder. */
    ESPNOW_TRIG_REJECT_BAD_NAME,   /**< Name failed validation. */
} espnow_trig_reject_t;

/** Execution result (carried in a RESULT frame). */
typedef enum {
    ESPNOW_TRIG_RESULT_OK   = 0,
    ESPNOW_TRIG_RESULT_FAIL = 1,
} espnow_trig_result_t;

/** Which side of the exchange a context represents. */
typedef enum {
    ESPNOW_TRIG_ROLE_INITIATOR = 0, /**< Asks the peer to replay. */
    ESPNOW_TRIG_ROLE_RESPONDER = 1, /**< Owns the capture and may replay it. */
} espnow_trig_role_t;

/** FSM states (shared enum; valid subset depends on role). */
typedef enum {
    ESPNOW_TRIG_STATE_IDLE = 0,
    /* Initiator */
    ESPNOW_TRIG_STATE_REQ_SENT,      /**< Waiting for ACCEPT/REJECT. */
    ESPNOW_TRIG_STATE_AWAIT_RESULT,  /**< Accepted; waiting for RESULT. */
    /* Responder */
    ESPNOW_TRIG_STATE_REQ_RECEIVED,  /**< Valid request; awaiting user consent. */
    ESPNOW_TRIG_STATE_EXECUTING,     /**< Consent granted; replay underway. */
    /* Terminal */
    ESPNOW_TRIG_STATE_DONE,          /**< Completed successfully. */
    ESPNOW_TRIG_STATE_REJECTED,      /**< Declined / failed. */
} espnow_trig_state_t;

/* =========================================================================
 * Context
 * =========================================================================*/

typedef struct {
    espnow_trig_role_t   role;
    espnow_trig_state_t  state;
    bool                 allow_remote;   /**< Responder opt-in. */
    espnow_share_kind_t  kind;           /**< Requested capture kind. */
    char                 name[ESPNOW_TRIG_NAME_MAX + 1];
    espnow_trig_reject_t reject_reason;  /**< Valid in REJECTED state. */
    espnow_trig_result_t result;         /**< Valid in DONE state. */
} espnow_trigger_ctx_t;

/* =========================================================================
 * Wire framing (pure build/parse)
 * =========================================================================*/

/**
 * @brief  Build a REQUEST frame: type + kind + name.
 * @return true on success; false if name is unsafe/too long or won't fit.
 */
bool espnow_trig_build_request(espnow_share_kind_t kind, const char *name,
                               uint8_t *out, size_t out_cap, size_t *out_len);

/**
 * @brief  Parse a REQUEST frame.
 * @param  out_kind  Receives the requested kind (may be NULL).
 * @param  name_out  Receives the null-terminated name.
 * @param  name_cap  Capacity of @p name_out.
 * @return true if well-formed and the name is safe and fits; false otherwise.
 */
bool espnow_trig_parse_request(const uint8_t *frame, size_t len,
                               espnow_share_kind_t *out_kind,
                               char *name_out, size_t name_cap);

/**
 * @brief  Build a single-byte status frame (ACCEPT/REJECT/RESULT).
 *
 * ACCEPT carries a 0 code; REJECT carries an espnow_trig_reject_t; RESULT
 * carries an espnow_trig_result_t.
 *
 * @return true on success, false on a bad type or insufficient buffer.
 */
bool espnow_trig_build_status(espnow_trig_msg_t type, uint8_t code,
                              uint8_t *out, size_t out_cap, size_t *out_len);

/**
 * @brief  Parse a single-byte status frame.
 * @param  out_type  Receives the message type (may be NULL).
 * @param  out_code  Receives the code byte (may be NULL).
 * @return true if the frame is a valid ACCEPT/REJECT/RESULT status frame.
 */
bool espnow_trig_parse_status(const uint8_t *frame, size_t len,
                              espnow_trig_msg_t *out_type, uint8_t *out_code);

/* =========================================================================
 * FSM
 * =========================================================================*/

/** Initialise a context. @p allow_remote is only meaningful for a responder. */
void espnow_trigger_init(espnow_trigger_ctx_t *ctx, espnow_trig_role_t role,
                         bool allow_remote);

/* ---- Initiator ---- */

/**
 * @brief  Record that a REQUEST was sent. IDLE → REQ_SENT.
 * @return true on success; false if not initiator/IDLE or the name is unsafe.
 */
bool espnow_trigger_request_sent(espnow_trigger_ctx_t *ctx,
                                 espnow_share_kind_t kind, const char *name);

/** Process a received ACCEPT. REQ_SENT → AWAIT_RESULT. */
bool espnow_trigger_on_accept(espnow_trigger_ctx_t *ctx);

/** Process a received REJECT. REQ_SENT → REJECTED (records reason). */
bool espnow_trigger_on_reject(espnow_trigger_ctx_t *ctx,
                              espnow_trig_reject_t reason);

/** Process a received RESULT. AWAIT_RESULT → DONE (records result). */
bool espnow_trigger_on_result(espnow_trigger_ctx_t *ctx,
                              espnow_trig_result_t result);

/* ---- Responder ---- */

/**
 * @brief  Process an inbound REQUEST.
 *
 * Applies the safety gate: if remote trigger is disabled, or the name is
 * invalid, the context transitions straight to REJECTED with the appropriate
 * reason (the caller should then send a REJECT).  Otherwise it moves to
 * REQ_RECEIVED to await explicit user consent.
 *
 * @return true if the request was accepted for consent (REQ_RECEIVED); false
 *         if it was auto-rejected or the context was not a fresh responder.
 *         Inspect ctx->state / ctx->reject_reason either way.
 */
bool espnow_trigger_on_request(espnow_trigger_ctx_t *ctx,
                               espnow_share_kind_t kind, const char *name);

/** User grants a pending request. REQ_RECEIVED → EXECUTING. */
bool espnow_trigger_grant(espnow_trigger_ctx_t *ctx);

/** User denies a pending request. REQ_RECEIVED → REJECTED (DENIED). */
bool espnow_trigger_deny(espnow_trigger_ctx_t *ctx);

/**
 * @brief  Record the outcome of the replay. EXECUTING → DONE/REJECTED.
 * @param  ok  true if the replay succeeded.
 * @return true on a valid transition.
 */
bool espnow_trigger_execution_done(espnow_trigger_ctx_t *ctx, bool ok);

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_TRIGGER_H_ */
