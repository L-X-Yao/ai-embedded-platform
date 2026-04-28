/*
 * cli_parser — minimal CLI parser for embedded debug shells
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "cli_parser.h"
#include <string.h>

static void cli_puts(cli_parser_t *ctx, const char *s)
{
    if (ctx->cfg.write_fn && s) {
        ctx->cfg.write_fn(s, strlen(s), ctx->cfg.user_ctx);
    }
}

static void cli_prompt(cli_parser_t *ctx)
{
    if (ctx->cfg.prompt) {
        cli_puts(ctx, ctx->cfg.prompt);
    }
}

static int cli_tokenize(char *line, const char *argv[], int max_args)
{
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

static cli_err_t cli_builtin_help(cli_parser_t *ctx)
{
    for (size_t i = 0; i < ctx->cfg.num_commands; i++) {
        cli_puts(ctx, "  ");
        cli_puts(ctx, ctx->cfg.commands[i].name);
        if (ctx->cfg.commands[i].help) {
            const char *sep = " -- ";
            cli_puts(ctx, sep);
            cli_puts(ctx, ctx->cfg.commands[i].help);
        }
        cli_puts(ctx, "\r\n");
    }
    cli_puts(ctx, "  help -- show this help\r\n");
    return CLI_OK;
}

void cli_parser_init(cli_parser_t *ctx, const cli_cfg_t *cfg)
{
    if (!ctx || !cfg) return;
    ctx->cfg = *cfg;
    ctx->line_len = 0;
    ctx->line_buf[0] = '\0';
    cli_prompt(ctx);
}

void cli_parser_feed(cli_parser_t *ctx, uint8_t byte)
{
    if (!ctx) return;

    if (byte == '\r' || byte == '\n') {
        if (ctx->line_len == 0 && byte == '\n') {
            cli_prompt(ctx);
            return;
        }
        if (ctx->line_len > 0) {
            cli_puts(ctx, "\r\n");
            ctx->line_buf[ctx->line_len] = '\0';
            cli_parser_run(ctx, ctx->line_buf);
            ctx->line_len = 0;
        }
        cli_prompt(ctx);
        return;
    }

    if (byte == '\b' || byte == 0x7F) {
        if (ctx->line_len > 0) {
            ctx->line_len--;
            cli_puts(ctx, "\b \b");
        }
        return;
    }

    if (byte >= 0x20 && byte < 0x7F) {
        if (ctx->line_len < CLI_LINE_BUFSIZE - 1) {
            ctx->line_buf[ctx->line_len++] = (char)byte;
            if (ctx->cfg.write_fn) {
                char c = (char)byte;
                ctx->cfg.write_fn(&c, 1, ctx->cfg.user_ctx);
            }
        }
    }
}

cli_err_t cli_parser_run(cli_parser_t *ctx, const char *line)
{
    if (!ctx || !line) return CLI_ERR_ARGS;

    char buf[CLI_LINE_BUFSIZE];
    size_t len = strlen(line);
    if (len >= CLI_LINE_BUFSIZE) return CLI_ERR_OVERFLOW;
    memcpy(buf, line, len + 1);

    const char *argv[CLI_MAX_ARGS];
    int argc = cli_tokenize(buf, argv, CLI_MAX_ARGS);
    if (argc == 0) return CLI_OK;

    if (strcmp(argv[0], "help") == 0) {
        return cli_builtin_help(ctx);
    }

    for (size_t i = 0; i < ctx->cfg.num_commands; i++) {
        if (strcmp(argv[0], ctx->cfg.commands[i].name) == 0) {
            return ctx->cfg.commands[i].handler(argc, argv, ctx->cfg.user_ctx);
        }
    }

    cli_puts(ctx, "unknown command: ");
    cli_puts(ctx, argv[0]);
    cli_puts(ctx, "\r\n");
    return CLI_ERR_NOT_FOUND;
}
