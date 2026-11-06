// SPDX-License-Identifier: MIT
/*
 * json_parser — minimal SAX-style JSON parser for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Zero-allocation streaming parser with callback-per-token design.
 * Feed chunks of data (streaming-friendly for UART/network).
 * No dynamic memory — caller provides json_parser_t on stack/static.
 * Thread safety: not reentrant (one parser instance per context).
 */

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Error codes ─────────────────────────────────────────────────── */

typedef enum {
    JSON_PARSER_OK             =  0,
    JSON_PARSER_ERR_PARAM      = -1,
    JSON_PARSER_ERR_SYNTAX     = -2,
    JSON_PARSER_ERR_DEPTH      = -3,
    JSON_PARSER_ERR_INCOMPLETE = -4,
} json_parser_err_t;

/* ── Event types ─────────────────────────────────────────────────── */

typedef enum {
    JSON_EVENT_OBJECT_START,
    JSON_EVENT_OBJECT_END,
    JSON_EVENT_ARRAY_START,
    JSON_EVENT_ARRAY_END,
    JSON_EVENT_KEY,
    JSON_EVENT_STRING,
    JSON_EVENT_NUMBER,
    JSON_EVENT_TRUE,
    JSON_EVENT_FALSE,
    JSON_EVENT_NULL,
} json_event_t;

/* ── Callback type ───────────────────────────────────────────────── */

/**
 * Called for each JSON token.
 * @param ctx      user context pointer
 * @param event    the token type
 * @param value    pointer into the input buffer (valid only during callback);
 *                 NULL for structural tokens (object/array start/end)
 * @param value_len length of value in bytes
 */
typedef void (*json_parser_cb_t)(void *ctx, json_event_t event,
                                  const char *value, uint16_t value_len);

/* ── Parser state (internal) ─────────────────────────────────────── */

/* Max value buffer for literals/numbers accumulated across chunks */
#ifndef JSON_PARSER_VALUE_BUF_SIZE
#define JSON_PARSER_VALUE_BUF_SIZE  64
#endif

typedef enum {
    JSON_STATE_VALUE,        /* expecting a JSON value                  */
    JSON_STATE_STRING,       /* inside a string                         */
    JSON_STATE_STRING_ESC,   /* backslash seen inside string            */
    JSON_STATE_STRING_U,     /* \uXXXX: collecting hex digits           */
    JSON_STATE_NUMBER,       /* accumulating a number                   */
    JSON_STATE_LITERAL,      /* accumulating true/false/null            */
    JSON_STATE_AFTER_VALUE,  /* after a complete value, expect , or ])} */
    JSON_STATE_AFTER_KEY,    /* after a key string, expect :            */
    JSON_STATE_DONE,         /* top-level value complete                */
    JSON_STATE_OBJECT_START, /* after {, expect a key or }              */
    JSON_STATE_ARRAY_START,  /* after [, expect a value or ]            */
    JSON_STATE_KEY,          /* expect an object key                    */
} json_state_t;

/* Container kind pushed onto the stack */
#define JSON_CONTAINER_OBJECT  0
#define JSON_CONTAINER_ARRAY   1

typedef struct {
    json_parser_cb_t  callback;               /* user callback                */
    void             *ctx;                     /* user context                 */

    /* -- internal state (do not touch) -- */
    json_state_t      state;                   /* current parser state         */
    uint8_t           depth;                   /* current nesting depth        */
    uint8_t           max_depth;               /* maximum allowed depth        */
    uint8_t           container_stack[16];      /* object/array stack (bitfield)*/
    char              vbuf[JSON_PARSER_VALUE_BUF_SIZE]; /* value accumulator   */
    uint16_t          vlen;                     /* bytes used in vbuf          */
    uint8_t           ucnt;                     /* hex digits collected for \u */
    bool              expect_key;              /* next string is an object key */
    bool              got_value;               /* at least one top-level value */
} json_parser_t;

/**
 * Initialize the parser.
 * @param max_depth  maximum nesting depth (capped at 128)
 * @thread_safe: no (instance not shared) @isr_safe: no
 */
json_parser_err_t json_parser_init(json_parser_t *p, json_parser_cb_t cb,
                                    void *ctx, uint8_t max_depth);

/**
 * Feed a chunk of JSON data.
 * May invoke the callback zero or more times.
 * @thread_safe: no (instance not shared) @isr_safe: yes
 */
json_parser_err_t json_parser_feed(json_parser_t *p, const char *data,
                                    uint16_t len);

/**
 * Finalize parsing. Returns ERR_INCOMPLETE if JSON is unfinished.
 * @thread_safe: no (instance not shared) @isr_safe: yes
 */
json_parser_err_t json_parser_finish(json_parser_t *p);

/**
 * Reset the parser to initial state (same callback/ctx/max_depth).
 * @thread_safe: no (instance not shared) @isr_safe: no
 */
void json_parser_reset(json_parser_t *p);

#ifdef __cplusplus
}
#endif

#endif /* JSON_PARSER_H */
