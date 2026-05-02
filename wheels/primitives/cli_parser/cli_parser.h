// SPDX-License-Identifier: MIT
/*
 * cli_parser — minimal CLI parser for embedded debug shells
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CLI_LINE_BUFSIZE
#define CLI_LINE_BUFSIZE    128
#endif

#ifndef CLI_MAX_ARGS
#define CLI_MAX_ARGS        8
#endif

typedef enum {
    CLI_OK            =  0,
    CLI_ERR_NOT_FOUND = -1,
    CLI_ERR_ARGS      = -2,
    CLI_ERR_OVERFLOW  = -3,
} cli_err_t;

typedef cli_err_t (*cli_handler_t)(int argc, const char *argv[], void *user_ctx);
typedef void (*cli_write_t)(const char *buf, size_t len, void *user_ctx);

typedef struct {
    const char  *name;
    cli_handler_t handler;
    const char  *help;
} cli_cmd_t;

typedef struct {
    const cli_cmd_t *commands;
    size_t           num_commands;
    cli_write_t      write_fn;
    void            *user_ctx;
    const char      *prompt;
} cli_cfg_t;

typedef struct {
    cli_cfg_t   cfg;
    char        line_buf[CLI_LINE_BUFSIZE];
    size_t      line_len;
    uint8_t     last_was_cr;
} cli_parser_t;

/** Initialize parser. @thread_safe: no */
void      cli_parser_init(cli_parser_t *ctx, const cli_cfg_t *cfg);

/** Feed one byte (from UART ISR or poll). @thread_safe: caller_must_lock */
void      cli_parser_feed(cli_parser_t *ctx, uint8_t byte);

/** Parse and execute a complete line. @thread_safe: caller_must_lock */
cli_err_t cli_parser_run(cli_parser_t *ctx, const char *line);

#ifdef __cplusplus
}
#endif

#endif /* CLI_PARSER_H */
