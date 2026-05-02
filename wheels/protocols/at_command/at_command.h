// SPDX-License-Identifier: MIT
/*
 * at_command — AT command parser with URC, timeout, multi-match
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    uart_hal
 */

#ifndef AT_COMMAND_H
#define AT_COMMAND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AT_COMMAND_LINE_SIZE
#define AT_COMMAND_LINE_SIZE   256
#endif
#ifndef AT_COMMAND_MATCH_SIZE
#define AT_COMMAND_MATCH_SIZE  256
#endif
#ifndef AT_COMMAND_MAX_URCS
#define AT_COMMAND_MAX_URCS    16
#endif

/* ---------- events ---------- */

typedef enum {
    AT_COMMAND_EVT_NONE = 0,
    AT_COMMAND_EVT_OK,
    AT_COMMAND_EVT_ERROR,
    AT_COMMAND_EVT_URC,
    AT_COMMAND_EVT_PROMPT,
    AT_COMMAND_EVT_LINE,
} at_command_evt_type_t;

typedef struct {
    at_command_evt_type_t type;
    const char   *line;
    uint16_t      line_len;
} at_command_evt_t;

/* ---------- URC callback ---------- */

typedef void (*at_command_urc_cb_t)(const char *line, void *ctx);

typedef struct {
    const char *prefix;
    at_command_urc_cb_t cb;
    void       *ctx;
} at_command_urc_entry_t;

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    void (*send)(void *ctx, const uint8_t *data, uint16_t len);
    void *ctx;
} at_command_hal_t;

/* ---------- parser state ---------- */

typedef struct {
    const at_command_hal_t *hal;
    char            line_buf[AT_COMMAND_LINE_SIZE];
    uint16_t        line_idx;
    char            expected[64];
    char            matched[AT_COMMAND_MATCH_SIZE];
    char            prev_line[AT_COMMAND_MATCH_SIZE];
    at_command_urc_entry_t  urcs[AT_COMMAND_MAX_URCS];
    uint8_t         urc_count;
    uint8_t         waiting;
} at_command_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/** Initialize parser state. @thread_safe: no */
void          at_command_init(at_command_t *at, const at_command_hal_t *hal);
/** Send AT command string (appends \r\n). @thread_safe: caller_must_lock */
void          at_command_send(at_command_t *at, const char *cmd);
/** Send raw bytes. @thread_safe: caller_must_lock */
void          at_command_send_raw(at_command_t *at, const uint8_t *data, uint16_t len);
/** Set expected response prefix. @thread_safe: caller_must_lock */
void          at_command_expect(at_command_t *at, const char *expected);
/** Cancel pending expect. @thread_safe: caller_must_lock */
void          at_command_cancel(at_command_t *at);
/** Feed one received byte. @thread_safe: caller_must_lock */
at_command_evt_type_t at_command_feed(at_command_t *at, uint8_t byte, at_command_evt_t *evt);
/** Register URC handler. @thread_safe: caller_must_lock */
int           at_command_register_urc(at_command_t *at, const char *prefix,
                                   at_command_urc_cb_t cb, void *ctx);
/** Unregister URC handler by prefix. @thread_safe: caller_must_lock */
int           at_command_unregister_urc(at_command_t *at, const char *prefix);
/** Get last matched response. @thread_safe: caller_must_lock */
const char   *at_command_get_matched(const at_command_t *at);
/** Get previous complete line. @thread_safe: caller_must_lock */
const char   *at_command_get_prev_line(const at_command_t *at);

#ifdef __cplusplus
}
#endif

#endif /* AT_COMMAND_H */
