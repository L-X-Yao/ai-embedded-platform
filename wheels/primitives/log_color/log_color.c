/*
 * log_color — ANSI color wrapper sink for log wheel.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#include "log_color.h"
#include <string.h>

#define ANSI_RED     "\033[31m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_GRAY    "\033[90m"
#define ANSI_RESET   "\033[0m"

void log_color_init(log_color_t *lc, log_color_output_fn inner_fn, void *inner_ctx) {
    if (!lc) return;
    lc->inner_fn = inner_fn;
    lc->inner_ctx = inner_ctx;
}

static char find_level_char(const char *line, size_t len) {
    /* Log format: "[TIMESTAMP] [L] ..." or "[L] ..." — find "[X]" where X is E/W/I/D */
    for (size_t i = 0; i + 2 < len; i++) {
        if (line[i] == '[' && line[i + 2] == ']') {
            char c = line[i + 1];
            if (c == 'E' || c == 'W' || c == 'I' || c == 'D')
                return c;
        }
    }
    return 0;
}

void log_color_sink(void *ctx, const char *line, size_t len) {
    log_color_t *lc = (log_color_t *)ctx;
    if (!lc || !lc->inner_fn || !line || len == 0) return;

    const char *color;
    char lvl = find_level_char(line, len);
    switch (lvl) {
        case 'E': color = ANSI_RED;    break;
        case 'W': color = ANSI_YELLOW; break;
        case 'I': color = ANSI_GREEN;  break;
        case 'D': color = ANSI_GRAY;   break;
        default:
            lc->inner_fn(lc->inner_ctx, line, len);
            return;
    }

    size_t color_len = strlen(color);
    size_t reset_len = strlen(ANSI_RESET);

    /* Build colored line: color + original (minus trailing \n) + reset + \n */
    char buf[512];
    size_t pos = 0;
    size_t body_len = len;
    int has_newline = (len > 0 && line[len - 1] == '\n');
    if (has_newline) body_len--;

    if (color_len + body_len + reset_len + 1 > sizeof(buf)) {
        lc->inner_fn(lc->inner_ctx, line, len);
        return;
    }

    memcpy(buf + pos, color, color_len); pos += color_len;
    memcpy(buf + pos, line, body_len);   pos += body_len;
    memcpy(buf + pos, ANSI_RESET, reset_len); pos += reset_len;
    if (has_newline) buf[pos++] = '\n';

    lc->inner_fn(lc->inner_ctx, buf, pos);
}
