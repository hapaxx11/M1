/* See COPYING.txt for license details. */

/**
 * @file   espnow_trigger.c
 * @brief  ESP-NOW danger-gated remote trigger — pure logic.
 *
 * See espnow_trigger.h.
 *
 * M1 Project
 */

#include "espnow_trigger.h"

#include <stdio.h>
#include <string.h>

/* =========================================================================
 * Wire framing
 * =========================================================================*/

static bool espnow_trig_kind_is_valid(espnow_share_kind_t kind)
{
    return kind >= ESPNOW_SHARE_KIND_SUBGHZ && kind <= ESPNOW_SHARE_KIND_IR;
}

bool espnow_trig_kind_can_execute(espnow_share_kind_t kind)
{
    return kind == ESPNOW_SHARE_KIND_SUBGHZ || kind == ESPNOW_SHARE_KIND_IR;
}

bool espnow_trig_build_replay_path(espnow_share_kind_t kind, const char *name,
                                   char *out, size_t out_cap)
{
    const char *dir = espnow_share_kind_dir(kind);
    int n;

    if (name == NULL || out == NULL || out_cap == 0u)
        return false;
    if (!espnow_trig_kind_can_execute(kind))
        return false;
    if (!espnow_share_name_is_safe(name, ESPNOW_TRIG_NAME_MAX))
        return false;
    if (espnow_share_classify(name) != kind)
        return false;
    if (dir == NULL)
        return false;

    n = snprintf(out, out_cap, "%s/%s", dir, name);
    return n > 0 && (size_t)n < out_cap;
}

bool espnow_trig_build_request(espnow_share_kind_t kind, const char *name,
                               uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (name == NULL || out == NULL || out_len == NULL)
        return false;
    if (!espnow_share_name_is_safe(name, ESPNOW_TRIG_NAME_MAX))
        return false;
    if (!espnow_trig_kind_can_execute(kind))
        return false;
    if (espnow_share_classify(name) != kind)
        return false;

    size_t nlen = strlen(name);
    size_t need = 2u + nlen;              /* type + kind + name */
    if (need > out_cap)
        return false;

    out[0] = (uint8_t)ESPNOW_TRIG_MSG_REQUEST;
    out[1] = (uint8_t)kind;
    memcpy(&out[2], name, nlen);

    *out_len = need;
    return true;
}

bool espnow_trig_parse_request(const uint8_t *frame, size_t len,
                               espnow_share_kind_t *out_kind,
                               char *name_out, size_t name_cap)
{
    if (frame == NULL || name_out == NULL || name_cap == 0u)
        return false;
    if (len < 3u)                          /* type + kind + ≥1 name byte */
        return false;
    if (frame[0] != (uint8_t)ESPNOW_TRIG_MSG_REQUEST)
        return false;
    espnow_share_kind_t kind = (espnow_share_kind_t)frame[1];
    if (!espnow_trig_kind_is_valid(kind))
        return false;

    size_t nlen = len - 2u;
    if (nlen + 1u > name_cap)
        return false;

    char tmp[ESPNOW_TRIG_NAME_MAX + 1];
    if (nlen > ESPNOW_TRIG_NAME_MAX)
        return false;
    if (memchr(&frame[2], '\0', nlen) != NULL)
        return false;
    memcpy(tmp, &frame[2], nlen);
    tmp[nlen] = '\0';

    /* Validate the received name before handing it back. */
    if (!espnow_share_name_is_safe(tmp, ESPNOW_TRIG_NAME_MAX))
        return false;

    memcpy(name_out, tmp, nlen + 1u);
    if (out_kind != NULL)
        *out_kind = kind;
    return true;
}

bool espnow_trig_build_status(espnow_trig_msg_t type, uint8_t code,
                              uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (out == NULL || out_len == NULL)
        return false;
    if (type != ESPNOW_TRIG_MSG_ACCEPT &&
        type != ESPNOW_TRIG_MSG_REJECT &&
        type != ESPNOW_TRIG_MSG_RESULT)
        return false;
    if (out_cap < 2u)
        return false;

    out[0] = (uint8_t)type;
    out[1] = code;
    *out_len = 2u;
    return true;
}

bool espnow_trig_parse_status(const uint8_t *frame, size_t len,
                              espnow_trig_msg_t *out_type, uint8_t *out_code)
{
    if (frame == NULL || len < 2u)
        return false;

    espnow_trig_msg_t t = (espnow_trig_msg_t)frame[0];
    if (t != ESPNOW_TRIG_MSG_ACCEPT &&
        t != ESPNOW_TRIG_MSG_REJECT &&
        t != ESPNOW_TRIG_MSG_RESULT)
        return false;

    if (out_type != NULL)
        *out_type = t;
    if (out_code != NULL)
        *out_code = frame[1];
    return true;
}

/* =========================================================================
 * FSM
 * =========================================================================*/

void espnow_trigger_init(espnow_trigger_ctx_t *ctx, espnow_trig_role_t role,
                         bool allow_remote)
{
    if (ctx == NULL)
        return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->role         = role;
    ctx->state        = ESPNOW_TRIG_STATE_IDLE;
    ctx->allow_remote = allow_remote;
    ctx->kind         = ESPNOW_SHARE_KIND_UNKNOWN;
    ctx->reject_reason = ESPNOW_TRIG_REJECT_NONE;
}

/* ---- Initiator ---- */

bool espnow_trigger_request_sent(espnow_trigger_ctx_t *ctx,
                                 espnow_share_kind_t kind, const char *name)
{
    if (ctx == NULL || name == NULL)
        return false;
    if (ctx->role != ESPNOW_TRIG_ROLE_INITIATOR ||
        ctx->state != ESPNOW_TRIG_STATE_IDLE)
        return false;
    if (!espnow_share_name_is_safe(name, ESPNOW_TRIG_NAME_MAX))
        return false;
    if (!espnow_trig_kind_can_execute(kind))
        return false;
    if (espnow_share_classify(name) != kind)
        return false;

    ctx->kind = kind;
    strncpy(ctx->name, name, ESPNOW_TRIG_NAME_MAX);
    ctx->name[ESPNOW_TRIG_NAME_MAX] = '\0';
    ctx->state = ESPNOW_TRIG_STATE_REQ_SENT;
    return true;
}

bool espnow_trigger_on_accept(espnow_trigger_ctx_t *ctx)
{
    if (ctx == NULL || ctx->role != ESPNOW_TRIG_ROLE_INITIATOR ||
        ctx->state != ESPNOW_TRIG_STATE_REQ_SENT)
        return false;
    ctx->state = ESPNOW_TRIG_STATE_AWAIT_RESULT;
    return true;
}

bool espnow_trigger_on_reject(espnow_trigger_ctx_t *ctx,
                              espnow_trig_reject_t reason)
{
    if (ctx == NULL || ctx->role != ESPNOW_TRIG_ROLE_INITIATOR ||
        ctx->state != ESPNOW_TRIG_STATE_REQ_SENT)
        return false;
    ctx->reject_reason = reason;
    ctx->state = ESPNOW_TRIG_STATE_REJECTED;
    return true;
}

bool espnow_trigger_on_result(espnow_trigger_ctx_t *ctx,
                              espnow_trig_result_t result)
{
    if (ctx == NULL || ctx->role != ESPNOW_TRIG_ROLE_INITIATOR ||
        ctx->state != ESPNOW_TRIG_STATE_AWAIT_RESULT)
        return false;
    ctx->result = result;
    ctx->state = (result == ESPNOW_TRIG_RESULT_OK)
                     ? ESPNOW_TRIG_STATE_DONE
                     : ESPNOW_TRIG_STATE_REJECTED;
    return true;
}

/* ---- Responder ---- */

bool espnow_trigger_on_request(espnow_trigger_ctx_t *ctx,
                               espnow_share_kind_t kind, const char *name)
{
    if (ctx == NULL || name == NULL)
        return false;
    if (ctx->role != ESPNOW_TRIG_ROLE_RESPONDER ||
        ctx->state != ESPNOW_TRIG_STATE_IDLE)
        return false;

    ctx->kind = kind;
    strncpy(ctx->name, name, ESPNOW_TRIG_NAME_MAX);
    ctx->name[ESPNOW_TRIG_NAME_MAX] = '\0';

    /* Safety gate 1 — feature must be opted in. */
    if (!ctx->allow_remote) {
        ctx->reject_reason = ESPNOW_TRIG_REJECT_DISABLED;
        ctx->state = ESPNOW_TRIG_STATE_REJECTED;
        return false;
    }

    /* Safety gate 2 — the name must be valid and the kind executable. */
    if (!espnow_share_name_is_safe(name, ESPNOW_TRIG_NAME_MAX) ||
        !espnow_trig_kind_can_execute(kind) ||
        espnow_share_classify(name) != kind) {
        ctx->reject_reason = ESPNOW_TRIG_REJECT_BAD_NAME;
        ctx->state = ESPNOW_TRIG_STATE_REJECTED;
        return false;
    }

    /* Await explicit user consent. */
    ctx->state = ESPNOW_TRIG_STATE_REQ_RECEIVED;
    return true;
}

bool espnow_trigger_grant(espnow_trigger_ctx_t *ctx)
{
    if (ctx == NULL || ctx->role != ESPNOW_TRIG_ROLE_RESPONDER ||
        ctx->state != ESPNOW_TRIG_STATE_REQ_RECEIVED)
        return false;
    ctx->state = ESPNOW_TRIG_STATE_EXECUTING;
    return true;
}

bool espnow_trigger_deny(espnow_trigger_ctx_t *ctx)
{
    if (ctx == NULL || ctx->role != ESPNOW_TRIG_ROLE_RESPONDER ||
        ctx->state != ESPNOW_TRIG_STATE_REQ_RECEIVED)
        return false;
    ctx->reject_reason = ESPNOW_TRIG_REJECT_DENIED;
    ctx->state = ESPNOW_TRIG_STATE_REJECTED;
    return true;
}

bool espnow_trigger_execution_done(espnow_trigger_ctx_t *ctx, bool ok)
{
    if (ctx == NULL || ctx->role != ESPNOW_TRIG_ROLE_RESPONDER ||
        ctx->state != ESPNOW_TRIG_STATE_EXECUTING)
        return false;
    ctx->result = ok ? ESPNOW_TRIG_RESULT_OK : ESPNOW_TRIG_RESULT_FAIL;
    ctx->state = ok ? ESPNOW_TRIG_STATE_DONE : ESPNOW_TRIG_STATE_REJECTED;
    return true;
}
