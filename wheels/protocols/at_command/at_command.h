// SPDX-License-Identifier: MIT
/*
 * at_command — AT command parser with URC, timeout, multi-match
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      uart_hal
 * Instances: multi
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
#if AT_COMMAND_MAX_URCS > 255
#error "AT_COMMAND_MAX_URCS exceeds 255, the most a uint8_t count can hold"
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
    uint8_t         final_wanted;  /* the wait set by at_command_expect_final() */
} at_command_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/** Initialize parser state. @thread_safe: no */
void          at_command_init(at_command_t *at, const at_command_hal_t *hal);
/** Send AT command string (appends \r\n). @thread_safe: caller_must_lock */
void          at_command_send(at_command_t *at, const char *cmd);
/** Send raw bytes. @thread_safe: caller_must_lock */
void          at_command_send_raw(at_command_t *at, const uint8_t *data, uint16_t len);
/** Wait for the first line that contains expected, and end the wait there.
 *  Use it when that line ends the command, as "OK", "SEND OK" and "CONNECT OK"
 *  do. If the modem still owes a final result code it arrives after the wait,
 *  and an expect for the next command would take it as its own answer: for
 *  information such as "+CSQ:" use at_command_expect_final().
 *  Any line that contains "ERROR" also ends the wait, with AT_COMMAND_EVT_ERROR.
 *  @thread_safe: caller_must_lock */
void          at_command_expect(at_command_t *at, const char *expected);
/** Wait for the final result code of a command that answers with information
 *  text first (V.250 section 5.7). A line that contains info is kept for
 *  at_command_get_matched() and reported as AT_COMMAND_EVT_LINE, the last one
 *  wins if there are several, and the wait goes on. A line that is exactly OK
 *  ends it with AT_COMMAND_EVT_OK, even if no line held info: matched is then
 *  empty. A line that is exactly ERROR or starts +CME ERROR or +CMS ERROR ends
 *  it with AT_COMMAND_EVT_ERROR and clears matched. Result codes of a dial
 *  command (CONNECT, BUSY and so on) do not end it: use at_command_expect().
 *  @thread_safe: caller_must_lock */
void          at_command_expect_final(at_command_t *at, const char *info);
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
/** Get the last complete line no URC handler took. @thread_safe: caller_must_lock */
const char   *at_command_get_prev_line(const at_command_t *at);

#ifdef __cplusplus
}
#endif

#endif /* AT_COMMAND_H */
