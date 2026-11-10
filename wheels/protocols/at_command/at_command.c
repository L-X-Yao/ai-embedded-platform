// SPDX-License-Identifier: MIT
/*
 * at_command — AT command parser with URC, timeout, multi-match
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      uart_hal
 * Instances: multi
 */

#include "at_command.h"
#include "contract.h"
#include <stddef.h>
#include <string.h>

#define AT_INVARIANT(at) do { \
    INVARIANT((at)->line_idx < AT_COMMAND_LINE_SIZE); \
    INVARIANT((at)->urc_count <= AT_COMMAND_MAX_URCS); \
} while(0)

void at_command_init(at_command_t *at, const at_command_hal_t *hal)
{
    if (at == NULL) {
        return;
    }
    at->hal          = hal;
    at->line_idx     = 0U;
    at->line_buf[0]  = '\0';
    at->expected[0]  = '\0';
    at->matched[0]   = '\0';
    at->prev_line[0] = '\0';
    at->urc_count    = 0U;
    at->waiting      = 0U;
    at->final_wanted = 0U;
    ENSURE(at->line_idx == 0U);
    ENSURE(at->urc_count == 0U);
}

void at_command_send(at_command_t *at, const char *cmd)
{
    uint16_t len;

    if (at == NULL || at->hal == NULL || cmd == NULL) {
        return;
    }
    size_t slen = strlen(cmd);
    if (slen > UINT16_MAX) return;
    len = (uint16_t)slen;
    at->hal->send(at->hal->ctx, (const uint8_t *)cmd, len);

    {
        static const uint8_t crlf[2] = {'\r', '\n'};
        at->hal->send(at->hal->ctx, crlf, 2U);
    }
}

void at_command_send_raw(at_command_t *at, const uint8_t *data, uint16_t len)
{
    if (at == NULL || at->hal == NULL || data == NULL || len == 0U) {
        return;
    }
    at->hal->send(at->hal->ctx, data, len);
}

static void start_wait(at_command_t *at, const char *expected, uint8_t final_wanted)
{
    if (at == NULL || expected == NULL) {
        return;
    }
    strncpy(at->expected, expected, sizeof(at->expected) - 1U);
    at->expected[sizeof(at->expected) - 1U] = '\0';
    at->matched[0] = '\0';
    at->waiting = 1U;
    at->final_wanted = final_wanted;
}

void at_command_expect(at_command_t *at, const char *expected)
{
    start_wait(at, expected, 0U);
}

void at_command_expect_final(at_command_t *at, const char *info)
{
    start_wait(at, info, 1U);
}

void at_command_cancel(at_command_t *at)
{
    if (at == NULL) {
        return;
    }
    at->expected[0] = '\0';
    at->waiting = 0U;
}

int at_command_register_urc(at_command_t *at, const char *prefix,
                         at_command_urc_cb_t cb, void *ctx)
{
    if (at == NULL || prefix == NULL || cb == NULL) {
        return -1;
    }
    if (at->urc_count >= AT_COMMAND_MAX_URCS) {
        return -1;
    }
    at->urcs[at->urc_count].prefix = prefix;
    at->urcs[at->urc_count].cb     = cb;
    at->urcs[at->urc_count].ctx    = ctx;
    at->urc_count++;
    INVARIANT(at->urc_count <= AT_COMMAND_MAX_URCS);
    return 0;
}

int at_command_unregister_urc(at_command_t *at, const char *prefix)
{
    if (at == NULL || prefix == NULL) {
        return -1;
    }
    for (uint8_t i = 0U; i < at->urc_count; i++) {
        if (strcmp(at->urcs[i].prefix, prefix) == 0) {
            at->urc_count--;
            for (uint8_t j = i; j < at->urc_count; j++) {
                at->urcs[j] = at->urcs[j + 1U];
            }
            memset(&at->urcs[at->urc_count], 0, sizeof(at_command_urc_entry_t));
            return 0;
        }
    }
    return -1;
}

const char *at_command_get_matched(const at_command_t *at)
{
    if (at == NULL) {
        return "";
    }
    return at->matched;
}

const char *at_command_get_prev_line(const at_command_t *at)
{
    if (at == NULL) {
        return "";
    }
    return at->prev_line;
}

static int contains(const char *haystack, const char *needle)
{
    return strstr(haystack, needle) != NULL;
}

static int starts_with(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

/** Hand the line to the first matching URC handler. Returns 1 if one took it. */
static int dispatch_urc(at_command_t *at, const char *line)
{
    uint8_t i;

    for (i = 0U; i < at->urc_count; i++) {
        if (starts_with(line, at->urcs[i].prefix)) {
            at->urcs[i].cb(line, at->urcs[i].ctx);
            return 1;
        }
    }
    return 0;
}

/** A whole line that reports failure: ERROR is V.250 table 1, +CME ERROR is
 *  TS 27.007 section 9.2, +CMS ERROR is TS 27.005 section 3.2.5. */
static int is_final_error(const char *line)
{
    return strcmp(line, "ERROR") == 0 ||
           starts_with(line, "+CME ERROR") ||
           starts_with(line, "+CMS ERROR");
}

static at_command_evt_type_t end_wait(at_command_t *at, at_command_evt_t *evt,
                                      at_command_evt_type_t type)
{
    at->expected[0] = '\0';
    at->waiting = 0U;
    evt->type = type;
    return type;
}

static void keep_line(char *dst, size_t size, const char *line)
{
    strncpy(dst, line, size - 1U);
    dst[size - 1U] = '\0';
}

static at_command_evt_type_t process_line(at_command_t *at, at_command_evt_t *evt)
{
    const char *line = at->line_buf;
    size_t slen = strlen(line);
    if (slen > UINT16_MAX) slen = UINT16_MAX;
    uint16_t len = (uint16_t)slen;

    if (len == 0U) {
        return AT_COMMAND_EVT_NONE;
    }

    evt->line     = line;
    evt->line_len = len;

    if (at->waiting && at->final_wanted) {
        /* only a whole line is a final result code: information text can
         * hold the same words */
        if (strcmp(line, "OK") == 0) {
            return end_wait(at, evt, AT_COMMAND_EVT_OK);
        }
        if (is_final_error(line)) {
            at->matched[0] = '\0';
            return end_wait(at, evt, AT_COMMAND_EVT_ERROR);
        }
        if (at->expected[0] != '\0' && contains(line, at->expected)) {
            keep_line(at->matched, sizeof(at->matched), line);
            keep_line(at->prev_line, sizeof(at->prev_line), line);
            evt->type = AT_COMMAND_EVT_LINE;
            return AT_COMMAND_EVT_LINE;
        }
    } else if (at->waiting && at->expected[0] != '\0') {
        /* a prompt is matched in at_command_feed, never as part of a line */
        if (at->expected[0] != '>' && contains(line, at->expected)) {
            keep_line(at->matched, sizeof(at->matched), line);
            return end_wait(at, evt, AT_COMMAND_EVT_OK);
        }

        if (contains(line, "ERROR")) {
            at->matched[0] = '\0';
            return end_wait(at, evt, AT_COMMAND_EVT_ERROR);
        }
    }

    /* not a response match — check URCs, and save a line no handler took
     * as prev_line: a URC must not replace the data of a pending command */
    if (!dispatch_urc(at, line)) {
        keep_line(at->prev_line, sizeof(at->prev_line), line);
    }

    evt->type = at->waiting ? AT_COMMAND_EVT_LINE : AT_COMMAND_EVT_URC;
    return evt->type;
}

at_command_evt_type_t at_command_feed(at_command_t *at, uint8_t byte, at_command_evt_t *evt)
{
    at_command_evt_type_t result;

    if (at == NULL || evt == NULL) {
        return AT_COMMAND_EVT_NONE;
    }

    evt->type     = AT_COMMAND_EVT_NONE;
    evt->line     = NULL;
    evt->line_len = 0U;

    /* ">" prompt detection (data mode, e.g., AT+CMGS, AT+MPUBEX). The prompt
     * starts a line, CR LF > SP in TS 27.005 3.5.1: a > further along is
     * text, such as a command echo. */
    if (byte == '>' && at->line_idx == 0U && at->waiting && at->expected[0] == '>') {
        at->matched[0]  = '>';
        at->matched[1]  = '\0';
        at->expected[0] = '\0';
        at->waiting     = 0U;
        evt->type       = AT_COMMAND_EVT_PROMPT;
        evt->line       = at->matched;
        evt->line_len   = 1U;
        return AT_COMMAND_EVT_PROMPT;
    }

    if (byte == '\n') {
        /* strip trailing CR */
        if (at->line_idx > 0U && at->line_buf[at->line_idx - 1U] == '\r') {
            at->line_idx--;
        }
        at->line_buf[at->line_idx] = '\0';
        result = process_line(at, evt);
        at->line_idx = 0U;
        return result;
    }

    if (at->line_idx < AT_COMMAND_LINE_SIZE - 1U) {
        at->line_buf[at->line_idx++] = (char)byte;
    }
    INVARIANT(at->line_idx < AT_COMMAND_LINE_SIZE);

    return AT_COMMAND_EVT_NONE;
}
