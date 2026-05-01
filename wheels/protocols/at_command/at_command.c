/*
 * at_command — AT command parser with URC, timeout, multi-match
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    uart_hal
 */

#include "at_command.h"
#include <stddef.h>
#include <string.h>

void at_cmd_init(at_cmd_t *at, const at_hal_t *hal)
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
}

void at_cmd_send(at_cmd_t *at, const char *cmd)
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

void at_cmd_send_raw(at_cmd_t *at, const uint8_t *data, uint16_t len)
{
    if (at == NULL || at->hal == NULL || data == NULL || len == 0U) {
        return;
    }
    at->hal->send(at->hal->ctx, data, len);
}

void at_cmd_expect(at_cmd_t *at, const char *expected)
{
    if (at == NULL || expected == NULL) {
        return;
    }
    strncpy(at->expected, expected, sizeof(at->expected) - 1U);
    at->expected[sizeof(at->expected) - 1U] = '\0';
    at->matched[0] = '\0';
    at->waiting = 1U;
}

void at_cmd_cancel(at_cmd_t *at)
{
    if (at == NULL) {
        return;
    }
    at->expected[0] = '\0';
    at->waiting = 0U;
}

int at_cmd_register_urc(at_cmd_t *at, const char *prefix,
                         at_urc_cb_t cb, void *ctx)
{
    if (at == NULL || prefix == NULL || cb == NULL) {
        return -1;
    }
    if (at->urc_count >= AT_CMD_MAX_URCS) {
        return -1;
    }
    at->urcs[at->urc_count].prefix = prefix;
    at->urcs[at->urc_count].cb     = cb;
    at->urcs[at->urc_count].ctx    = ctx;
    at->urc_count++;
    return 0;
}

int at_cmd_unregister_urc(at_cmd_t *at, const char *prefix)
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
            memset(&at->urcs[at->urc_count], 0, sizeof(at_urc_entry_t));
            return 0;
        }
    }
    return -1;
}

const char *at_cmd_get_matched(const at_cmd_t *at)
{
    if (at == NULL) {
        return "";
    }
    return at->matched;
}

const char *at_cmd_get_prev_line(const at_cmd_t *at)
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

static void dispatch_urc(at_cmd_t *at, const char *line)
{
    uint8_t i;

    for (i = 0U; i < at->urc_count; i++) {
        if (starts_with(line, at->urcs[i].prefix)) {
            at->urcs[i].cb(line, at->urcs[i].ctx);
            return;
        }
    }
}

static at_evt_type_t process_line(at_cmd_t *at, at_evt_t *evt)
{
    const char *line = at->line_buf;
    uint16_t len = (uint16_t)strlen(line);

    if (len == 0U) {
        return AT_EVT_NONE;
    }

    evt->line     = line;
    evt->line_len = len;

    if (at->waiting && at->expected[0] != '\0') {
        if (contains(line, at->expected)) {
            strncpy(at->matched, line, sizeof(at->matched) - 1U);
            at->matched[sizeof(at->matched) - 1U] = '\0';
            at->expected[0] = '\0';
            at->waiting = 0U;
            evt->type = AT_EVT_OK;
            return AT_EVT_OK;
        }

        if (contains(line, "ERROR")) {
            at->matched[0] = '\0';
            at->expected[0] = '\0';
            at->waiting = 0U;
            evt->type = AT_EVT_ERROR;
            return AT_EVT_ERROR;
        }
    }

    /* not a response match — check URCs then save as prev_line */
    dispatch_urc(at, line);
    strncpy(at->prev_line, line, sizeof(at->prev_line) - 1U);
    at->prev_line[sizeof(at->prev_line) - 1U] = '\0';

    evt->type = at->waiting ? AT_EVT_LINE : AT_EVT_URC;
    return evt->type;
}

at_evt_type_t at_cmd_feed(at_cmd_t *at, uint8_t byte, at_evt_t *evt)
{
    at_evt_type_t result;

    if (at == NULL || evt == NULL) {
        return AT_EVT_NONE;
    }

    evt->type     = AT_EVT_NONE;
    evt->line     = NULL;
    evt->line_len = 0U;

    /* ">" prompt detection (data mode, e.g., AT+CMGS, AT+MPUBEX) */
    if (byte == '>' && at->waiting && at->expected[0] == '>') {
        at->matched[0]  = '>';
        at->matched[1]  = '\0';
        at->expected[0] = '\0';
        at->waiting     = 0U;
        evt->type       = AT_EVT_PROMPT;
        evt->line       = at->matched;
        evt->line_len   = 1U;
        at->line_idx    = 0U;
        return AT_EVT_PROMPT;
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

    if (at->line_idx < AT_CMD_LINE_SIZE - 1U) {
        at->line_buf[at->line_idx++] = (char)byte;
    }

    return AT_EVT_NONE;
}
