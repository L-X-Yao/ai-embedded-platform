/*
 * cli_shell -- command-line shell module for embedded debug/configuration
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 */

#include "cli_shell.h"
#include <string.h>

/* ---------- Internal helpers ---------- */

static void sh_write(const cli_shell_t *sh, const char *s, uint16_t len)
{
    if (sh->io && sh->io->write && s && len > 0) {
        sh->io->write(sh->io->ctx, s, len);
    }
}

static void sh_puts(const cli_shell_t *sh, const char *s)
{
    if (s) {
        sh_write(sh, s, (uint16_t)strlen(s));
    }
}

static void sh_prompt(const cli_shell_t *sh)
{
    if (sh->prompt) {
        sh_puts(sh, sh->prompt);
    }
}

/*
 * Tokenize line in-place.  Handles double-quoted arguments:
 *   log "hello world"  ->  argc=2, argv=["log", "hello world"]
 * Returns argc.
 */
static int sh_tokenize(char *line, const char *argv[], int max_args)
{
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args) {
        /* skip whitespace */
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        if (*p == '"') {
            /* quoted argument */
            p++;                    /* skip opening quote */
            argv[argc++] = p;
            while (*p && *p != '"') {
                p++;
            }
            if (*p == '"') {
                *p++ = '\0';        /* terminate and skip closing quote */
            }
        } else {
            /* unquoted argument */
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') {
                p++;
            }
            if (*p) {
                *p++ = '\0';
            }
        }
    }
    return argc;
}

/* Built-in help command handler */
static void sh_builtin_help(void *ctx, int argc, const char **argv,
                            const cli_shell_io_t *io)
{
    (void)argc;
    (void)argv;
    (void)io;
    cli_shell_t *sh = (cli_shell_t *)ctx;

    for (uint8_t i = 0; i < sh->cmd_count; i++) {
        sh_puts(sh, "  ");
        sh_puts(sh, sh->cmds[i].name);
        if (sh->cmds[i].help) {
            sh_puts(sh, " -- ");
            sh_puts(sh, sh->cmds[i].help);
        }
        sh_puts(sh, "\r\n");
    }
}

/* Process a completed line: tokenize, find command, dispatch */
static void sh_execute(cli_shell_t *sh)
{
    char buf[CLI_SHELL_LINE_SIZE];
    memcpy(buf, sh->line, sh->line_pos);
    buf[sh->line_pos] = '\0';

    const char *argv[CLI_SHELL_MAX_ARGS];
    int argc = sh_tokenize(buf, argv, CLI_SHELL_MAX_ARGS);

    if (argc == 0) {
        return;
    }

    /* search registered commands */
    for (uint8_t i = 0; i < sh->cmd_count; i++) {
        if (strcmp(argv[0], sh->cmds[i].name) == 0) {
            sh->cmds[i].handler(sh->cmds[i].ctx, argc, argv, sh->io);
            return;
        }
    }

    /* unknown command */
    sh_puts(sh, "unknown: ");
    sh_puts(sh, argv[0]);
    sh_puts(sh, "\r\ntype \"help\" for commands\r\n");
}

/* ---------- Public API ---------- */

cli_shell_err_t cli_shell_init(cli_shell_t *sh, const cli_shell_io_t *io,
                               const char *prompt)
{
    if (!sh) {
        return CLI_SHELL_ERR_PARAMS;
    }

    memset(sh, 0, sizeof(*sh));
    sh->io     = io;
    sh->prompt = prompt;

    /* register built-in "help" */
    sh->cmds[0].name    = "help";
    sh->cmds[0].help    = "list commands";
    sh->cmds[0].handler = sh_builtin_help;
    sh->cmds[0].ctx     = sh;
    sh->cmd_count        = 1;

    sh_prompt(sh);
    return CLI_SHELL_OK;
}

cli_shell_err_t cli_shell_register(cli_shell_t *sh, const char *name,
                                   const char *help,
                                   cli_shell_handler_t handler, void *ctx)
{
    if (!sh || !name || !handler) {
        return CLI_SHELL_ERR_PARAMS;
    }
    if (sh->cmd_count >= CLI_SHELL_MAX_CMDS) {
        return CLI_SHELL_ERR_FULL;
    }

    cli_shell_cmd_t *cmd = &sh->cmds[sh->cmd_count];
    cmd->name    = name;
    cmd->help    = help;
    cmd->handler = handler;
    cmd->ctx     = ctx;
    sh->cmd_count++;
    return CLI_SHELL_OK;
}

void cli_shell_feed(cli_shell_t *sh, uint8_t byte)
{
    if (!sh) {
        return;
    }

    /* newline handling */
    if (byte == '\r' || byte == '\n') {
        if (byte == '\n' && sh->last_was_cr) {
            /* second half of CRLF -- ignore */
            sh->last_was_cr = 0;
            return;
        }
        sh->last_was_cr = (byte == '\r') ? 1U : 0U;

        sh_puts(sh, "\r\n");
        if (sh->line_pos > 0) {
            sh->line[sh->line_pos] = '\0';
            sh_execute(sh);
        }
        sh->line_pos = 0;
        sh_prompt(sh);
        return;
    }

    sh->last_was_cr = 0;

    /* backspace / DEL */
    if (byte == '\b' || byte == 0x7F) {
        if (sh->line_pos > 0) {
            sh->line_pos--;
            sh_puts(sh, "\b \b");
        }
        return;
    }

    /* printable characters */
    if (byte >= 0x20 && byte < 0x7F) {
        if (sh->line_pos < CLI_SHELL_LINE_SIZE - 1) {
            sh->line[sh->line_pos++] = (char)byte;
            /* echo */
            char c = (char)byte;
            sh_write(sh, &c, 1);
        }
        /* line full: silently drop extra chars */
        return;
    }

    /* non-printable control chars: ignore */
}
