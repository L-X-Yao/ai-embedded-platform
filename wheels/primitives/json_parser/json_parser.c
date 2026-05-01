/*
 * json_parser — minimal SAX-style JSON parser for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "json_parser.h"
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────────── */

static bool is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_hex(char c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool is_number_char(char c)
{
    return is_digit(c) || c == '.' || c == '-' || c == '+' ||
           c == 'e' || c == 'E';
}

static void push_container(json_parser_t *p, uint8_t kind)
{
    uint8_t byte_idx = (uint8_t)(p->depth / 8u);
    uint8_t bit_idx  = (uint8_t)(p->depth % 8u);
    if (kind) {
        p->container_stack[byte_idx] |= (uint8_t)(1u << bit_idx);
    } else {
        p->container_stack[byte_idx] &= (uint8_t)(~(1u << bit_idx));
    }
}

static uint8_t peek_container(const json_parser_t *p)
{
    if (p->depth == 0) return 0xFF;  /* no container */
    uint8_t byte_idx = (uint8_t)((p->depth - 1u) / 8u);
    uint8_t bit_idx  = (uint8_t)((p->depth - 1u) % 8u);
    return (p->container_stack[byte_idx] >> bit_idx) & 1u;
}

static void emit(json_parser_t *p, json_event_t ev,
                 const char *val, uint16_t len)
{
    if (p->callback) {
        p->callback(p->ctx, ev, val, len);
    }
}

static void vbuf_reset(json_parser_t *p)
{
    p->vlen = 0;
}

static bool vbuf_push(json_parser_t *p, char c)
{
    if (p->vlen < JSON_PARSER_VALUE_BUF_SIZE) {
        p->vbuf[p->vlen++] = c;
        return true;
    }
    return false;  /* overflow — silently truncate */
}

/* ── API ─────────────────────────────────────────────────────────── */

json_parser_err_t json_parser_init(json_parser_t *p, json_parser_cb_t cb,
                                    void *ctx, uint8_t max_depth)
{
    if (!p || !cb) return JSON_PARSER_ERR_PARAM;
    if (max_depth == 0) max_depth = 16;
    if (max_depth > 128) max_depth = 128;

    memset(p, 0, sizeof(*p));
    p->callback  = cb;
    p->ctx       = ctx;
    p->max_depth = max_depth;
    p->state     = JSON_STATE_VALUE;
    return JSON_PARSER_OK;
}

void json_parser_reset(json_parser_t *p)
{
    if (!p) return;
    json_parser_cb_t cb = p->callback;
    void *ctx = p->ctx;
    uint8_t md = p->max_depth;
    memset(p, 0, sizeof(*p));
    p->callback  = cb;
    p->ctx       = ctx;
    p->max_depth = md;
    p->state     = JSON_STATE_VALUE;
}

/* Transition after a complete value has been emitted */
static void after_value(json_parser_t *p)
{
    p->got_value = true;
    if (p->depth == 0) {
        p->state = JSON_STATE_DONE;
    } else {
        p->state = JSON_STATE_AFTER_VALUE;
    }
}

/* Flush a completed number from vbuf */
static void flush_number(json_parser_t *p)
{
    emit(p, JSON_EVENT_NUMBER, p->vbuf, p->vlen);
    vbuf_reset(p);
    after_value(p);
}

/* Flush a completed literal (true/false/null) from vbuf */
static json_parser_err_t flush_literal(json_parser_t *p)
{
    if (p->vlen == 4 && memcmp(p->vbuf, "true", 4) == 0) {
        emit(p, JSON_EVENT_TRUE, p->vbuf, p->vlen);
    } else if (p->vlen == 5 && memcmp(p->vbuf, "false", 5) == 0) {
        emit(p, JSON_EVENT_FALSE, p->vbuf, p->vlen);
    } else if (p->vlen == 4 && memcmp(p->vbuf, "null", 4) == 0) {
        emit(p, JSON_EVENT_NULL, p->vbuf, p->vlen);
    } else {
        return JSON_PARSER_ERR_SYNTAX;
    }
    vbuf_reset(p);
    after_value(p);
    return JSON_PARSER_OK;
}

json_parser_err_t json_parser_feed(json_parser_t *p, const char *data,
                                    uint16_t len)
{
    if (!p || (!data && len > 0)) return JSON_PARSER_ERR_PARAM;

    for (uint16_t i = 0; i < len; i++) {
        char c = data[i];

        switch (p->state) {

        /* ── Expecting a value ───────────────────────────────── */
        case JSON_STATE_VALUE:
            if (is_ws(c)) break;
            if (c == '{') {
                if (p->depth >= p->max_depth)
                    return JSON_PARSER_ERR_DEPTH;
                push_container(p, JSON_CONTAINER_OBJECT);
                p->depth++;
                p->expect_key = true;
                emit(p, JSON_EVENT_OBJECT_START, NULL, 0);
                /* stay in VALUE state for first key or } */
                break;
            }
            if (c == '[') {
                if (p->depth >= p->max_depth)
                    return JSON_PARSER_ERR_DEPTH;
                push_container(p, JSON_CONTAINER_ARRAY);
                p->depth++;
                p->expect_key = false;
                emit(p, JSON_EVENT_ARRAY_START, NULL, 0);
                break;
            }
            if (c == '}') {
                /* empty object */
                if (p->depth == 0 || peek_container(p) != JSON_CONTAINER_OBJECT)
                    return JSON_PARSER_ERR_SYNTAX;
                p->depth--;
                emit(p, JSON_EVENT_OBJECT_END, NULL, 0);
                after_value(p);
                break;
            }
            if (c == ']') {
                /* empty array */
                if (p->depth == 0 || peek_container(p) != JSON_CONTAINER_ARRAY)
                    return JSON_PARSER_ERR_SYNTAX;
                p->depth--;
                emit(p, JSON_EVENT_ARRAY_END, NULL, 0);
                after_value(p);
                break;
            }
            if (c == '"') {
                vbuf_reset(p);
                p->state = JSON_STATE_STRING;
                break;
            }
            if (c == '-' || is_digit(c)) {
                vbuf_reset(p);
                vbuf_push(p, c);
                p->state = JSON_STATE_NUMBER;
                break;
            }
            if (c == 't' || c == 'f' || c == 'n') {
                vbuf_reset(p);
                vbuf_push(p, c);
                p->state = JSON_STATE_LITERAL;
                break;
            }
            return JSON_PARSER_ERR_SYNTAX;

        /* ── Inside a string ─────────────────────────────────── */
        case JSON_STATE_STRING:
            if (c == '\\') {
                p->state = JSON_STATE_STRING_ESC;
                break;
            }
            if (c == '"') {
                /* end of string */
                if (p->expect_key) {
                    emit(p, JSON_EVENT_KEY, p->vbuf, p->vlen);
                    p->expect_key = false;
                    p->state = JSON_STATE_AFTER_KEY;
                } else {
                    emit(p, JSON_EVENT_STRING, p->vbuf, p->vlen);
                    after_value(p);
                }
                vbuf_reset(p);
                break;
            }
            vbuf_push(p, c);
            break;

        /* ── Escape character ────────────────────────────────── */
        case JSON_STATE_STRING_ESC:
            switch (c) {
            case '"':  vbuf_push(p, '"');  break;
            case '\\': vbuf_push(p, '\\'); break;
            case '/':  vbuf_push(p, '/');  break;
            case 'b':  vbuf_push(p, '\b'); break;
            case 'f':  vbuf_push(p, '\f'); break;
            case 'n':  vbuf_push(p, '\n'); break;
            case 'r':  vbuf_push(p, '\r'); break;
            case 't':  vbuf_push(p, '\t'); break;
            case 'u':
                p->ucnt = 0;
                p->state = JSON_STATE_STRING_U;
                goto next_char;  /* don't fall to state reset below */
            default:
                return JSON_PARSER_ERR_SYNTAX;
            }
            p->state = JSON_STATE_STRING;
            break;

        /* ── Unicode escape \uXXXX ───────────────────────────── */
        case JSON_STATE_STRING_U:
            if (!is_hex(c)) return JSON_PARSER_ERR_SYNTAX;
            vbuf_push(p, c);  /* store hex digit literally */
            p->ucnt++;
            if (p->ucnt == 4) {
                p->state = JSON_STATE_STRING;
            }
            break;

        /* ── Accumulating a number ───────────────────────────── */
        case JSON_STATE_NUMBER:
            if (is_number_char(c)) {
                vbuf_push(p, c);
                break;
            }
            /* end of number — process this char again */
            flush_number(p);
            i--;  /* re-process current char in new state */
            break;

        /* ── Accumulating a literal ──────────────────────────── */
        case JSON_STATE_LITERAL: {
            if ((c >= 'a' && c <= 'z')) {
                vbuf_push(p, c);
                /* Check if we have a complete literal */
                if ((p->vlen == 4 && (memcmp(p->vbuf, "true", 4) == 0 ||
                                       memcmp(p->vbuf, "null", 4) == 0)) ||
                    (p->vlen == 5 && memcmp(p->vbuf, "false", 5) == 0)) {
                    json_parser_err_t err = flush_literal(p);
                    if (err != JSON_PARSER_OK) return err;
                }
                break;
            }
            /* non-alpha ends the literal; must be complete */
            json_parser_err_t err = flush_literal(p);
            if (err != JSON_PARSER_OK) return err;
            i--;  /* re-process current char */
            break;
        }

        /* ── After a key string, expect ':' ──────────────────── */
        case JSON_STATE_AFTER_KEY:
            if (is_ws(c)) break;
            if (c == ':') {
                p->state = JSON_STATE_VALUE;
                break;
            }
            return JSON_PARSER_ERR_SYNTAX;

        /* ── After a value, expect ',' or closing bracket ────── */
        case JSON_STATE_AFTER_VALUE:
            if (is_ws(c)) break;
            if (c == ',') {
                uint8_t container = peek_container(p);
                if (container == JSON_CONTAINER_OBJECT) {
                    p->expect_key = true;
                }
                p->state = JSON_STATE_VALUE;
                break;
            }
            if (c == '}') {
                if (p->depth == 0 || peek_container(p) != JSON_CONTAINER_OBJECT)
                    return JSON_PARSER_ERR_SYNTAX;
                p->depth--;
                emit(p, JSON_EVENT_OBJECT_END, NULL, 0);
                after_value(p);
                break;
            }
            if (c == ']') {
                if (p->depth == 0 || peek_container(p) != JSON_CONTAINER_ARRAY)
                    return JSON_PARSER_ERR_SYNTAX;
                p->depth--;
                emit(p, JSON_EVENT_ARRAY_END, NULL, 0);
                after_value(p);
                break;
            }
            return JSON_PARSER_ERR_SYNTAX;

        /* ── Top-level value already complete ─────────────────── */
        case JSON_STATE_DONE:
            if (is_ws(c)) break;
            return JSON_PARSER_ERR_SYNTAX;  /* trailing non-ws */
        }

next_char:;
    }

    return JSON_PARSER_OK;
}

json_parser_err_t json_parser_finish(json_parser_t *p)
{
    if (!p) return JSON_PARSER_ERR_PARAM;

    /* Flush pending number */
    if (p->state == JSON_STATE_NUMBER) {
        flush_number(p);
    }

    /* Flush pending literal */
    if (p->state == JSON_STATE_LITERAL) {
        json_parser_err_t err = flush_literal(p);
        if (err != JSON_PARSER_OK) return err;
    }

    if (p->state == JSON_STATE_DONE) return JSON_PARSER_OK;

    /* Bare value (number/literal) completed and depth==0 */
    if (p->state == JSON_STATE_DONE) return JSON_PARSER_OK;

    if (!p->got_value) return JSON_PARSER_ERR_INCOMPLETE;
    if (p->depth > 0)  return JSON_PARSER_ERR_INCOMPLETE;

    return JSON_PARSER_ERR_INCOMPLETE;
}
