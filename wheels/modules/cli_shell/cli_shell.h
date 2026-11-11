// SPDX-License-Identifier: MIT
/*
 * cli_shell -- command-line shell module for embedded debug/configuration
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none -- self-contained, does not #include cli_parser)
 * Instances: multi
 *
 * Passive design: application feeds bytes via cli_shell_feed(),
 * shell parses and dispatches to registered command handlers.
 * Output via write callback -- application decides the transport.
 */

#ifndef CLI_SHELL_H
#define CLI_SHELL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Configuration macros (override before #include) ---------- */

/*
 * Warning: CLI_SHELL_LINE_SIZE is allocated on the cli_shell_t struct
 * (stack or static). Large values (e.g. 512+) increase stack usage
 * significantly. Keep <=256 on memory-constrained MCUs.
 */
#ifndef CLI_SHELL_LINE_SIZE
#define CLI_SHELL_LINE_SIZE     128
#endif

#ifndef CLI_SHELL_MAX_ARGS
#define CLI_SHELL_MAX_ARGS      8
#endif

#ifndef CLI_SHELL_MAX_CMDS
#define CLI_SHELL_MAX_CMDS      16
#endif
#if CLI_SHELL_MAX_CMDS > 255
#error "CLI_SHELL_MAX_CMDS exceeds 255, the most a uint8_t count can hold"
#endif

/* ---------- Error codes ---------- */

typedef enum {
    CLI_SHELL_OK          =  0,
    CLI_SHELL_ERR_PARAM  = -1,
    CLI_SHELL_ERR_FULL    = -2,
} cli_shell_err_t;

/* ---------- I/O abstraction ---------- */

typedef struct {
    void (*write)(void *ctx, const char *data, uint16_t len);
    void  *ctx;
} cli_shell_io_t;

/* ---------- Command handler ---------- */

typedef void (*cli_shell_handler_t)(void *ctx, int argc, const char **argv,
                                    const cli_shell_io_t *io);

typedef struct {
    const char          *name;
    const char          *help;
    cli_shell_handler_t  handler;
    void                *ctx;
} cli_shell_cmd_t;

/* ---------- Shell context ---------- */

typedef struct {
    const cli_shell_io_t *io;
    cli_shell_cmd_t       cmds[CLI_SHELL_MAX_CMDS];
    uint8_t               cmd_count;
    char                  line[CLI_SHELL_LINE_SIZE];
    uint16_t              line_pos;
    const char           *prompt;
    uint8_t               last_was_cr;
    uint8_t               line_overflow;  /* internal: this line lost bytes */
} cli_shell_t;

/* ---------- API ---------- */

/**
 * Initialize shell context. Prints prompt if io is set.
 * @thread_safe: no
 */
cli_shell_err_t cli_shell_init(cli_shell_t *sh, const cli_shell_io_t *io,
                               const char *prompt);

/**
 * Register a command. Returns ERR_FULL if table is full.
 *
 * name and help are kept by pointer, not copied: name is compared with the
 * first word of every command line, and the built-in help command prints
 * both. They must stay valid and unchanged for as long as sh is in use:
 * string literals or static arrays. A stack buffer, or one buffer reused for
 * several commands, gives wrong or dead names and no error.
 * @thread_safe: no
 */
cli_shell_err_t cli_shell_register(cli_shell_t *sh, const char *name,
                                   const char *help,
                                   cli_shell_handler_t handler, void *ctx);

/**
 * Feed one byte from a task or polling loop. Every printable byte calls the
 * write callback and end of line runs the command handler inside this call,
 * so a UART ISR must queue its bytes rather than call this.
 * A line that outgrows CLI_SHELL_LINE_SIZE is refused at end of line with
 * "line too long": the handler does not run, and backspace does not undo it.
 * @thread_safe: no
 * @isr_safe: no
 */
void cli_shell_feed(cli_shell_t *sh, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* CLI_SHELL_H */
