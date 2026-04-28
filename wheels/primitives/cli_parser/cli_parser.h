#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include <stddef.h>
#include <stdint.h>

#ifndef CLI_LINE_BUFSIZE
#define CLI_LINE_BUFSIZE    128
#endif

#ifndef CLI_MAX_ARGS
#define CLI_MAX_ARGS        8
#endif

typedef enum {
    CLI_OK = 0,
    CLI_ERR_NOT_FOUND,
    CLI_ERR_ARGS,
    CLI_ERR_OVERFLOW,
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
} cli_parser_t;

void      cli_parser_init(cli_parser_t *ctx, const cli_cfg_t *cfg);
void      cli_parser_feed(cli_parser_t *ctx, uint8_t byte);
cli_err_t cli_parser_run(cli_parser_t *ctx, const char *line);

#endif /* CLI_PARSER_H */
