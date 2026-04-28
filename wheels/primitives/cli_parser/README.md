# cli_parser

**STATUS: candidate**
**Layer: L0 (primitives)**

Minimal CLI parser for embedded debug shells. Character-at-a-time feed
from UART, command table dispatch, built-in help, line editing (backspace).
Pure C99, zero dependencies, no heap allocation.

## Why not VSF native?

Checked VSF modules (2026-04-28):

- `vsf/source/component/` — no CLI / command parser utility found
- `vsf/source/service/` — no shell / command dispatch found
- `vsf/source/utilities/` — no readline / line editor found

VSF has no native CLI parsing. Closest is the linux shell component which
requires the full POSIX skin — far too heavy for a debug UART shell on
small MCUs.

**Conclusion**: no VSF equivalent. This wheel fills the gap as a pure L0
primitive with zero framework dependency.

## Features

- Character-at-a-time feed (`cli_parser_feed`) — works with UART ISR
- Command table: static array of `{name, handler, help}` entries
- Built-in `help` command lists all registered commands
- Argument splitting (whitespace-delimited, argc/argv style)
- Backspace / DEL handling with echo
- Configurable prompt string
- Line buffer overflow protection
- Pure C99, no heap, fully reentrant per-context
- Output via callback (works with any transport: UART, RTT, USB CDC)

## API summary

```c
/* Command handler signature */
typedef cli_err_t (*cli_handler_t)(int argc, const char *argv[], void *user_ctx);

/* Output callback */
typedef void (*cli_write_t)(const char *buf, size_t len, void *user_ctx);

/* Command table entry */
typedef struct {
    const char  *name;
    cli_handler_t handler;
    const char  *help;
} cli_cmd_t;

/* Configuration */
typedef struct {
    const cli_cmd_t *commands;
    size_t           num_commands;
    cli_write_t      write_fn;
    void            *user_ctx;
    const char      *prompt;   /* e.g. "> ", NULL to disable */
} cli_cfg_t;

/* Init, feed bytes, run line directly */
void      cli_parser_init(cli_parser_t *ctx, const cli_cfg_t *cfg);
void      cli_parser_feed(cli_parser_t *ctx, uint8_t byte);
cli_err_t cli_parser_run(cli_parser_t *ctx, const char *line);
```

## Configuration macros

| Macro | Default | Description |
|-------|---------|-------------|
| `CLI_LINE_BUFSIZE` | 128 | Max input line length |
| `CLI_MAX_ARGS` | 8 | Max number of arguments per command |

## Dependencies

None. Pure C99 with `<string.h>` and `<stddef.h>` only.

## Supported platforms

- [x] Linux / Windows host (sim) — passed 2026-04-28
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

None.

## Test status

- Unit tests: **PASS** (41 assertions across 20 test cases)
- Coverage: init, known/unknown/empty commands, argument parsing, whitespace
  handling, help, handler errors, overflow, max args truncation, null safety,
  feed+enter, backspace, DEL, feed overflow, backspace on empty

## Usage example

```c
#include "cli_parser.h"

static cli_err_t cmd_led(int argc, const char *argv[], void *ctx) {
    (void)ctx;
    if (argc < 2) return CLI_ERR_ARGS;
    /* argv[1] is "on" or "off" */
    led_set(strcmp(argv[1], "on") == 0);
    return CLI_OK;
}

static cli_err_t cmd_reboot(int argc, const char *argv[], void *ctx) {
    (void)argc; (void)argv; (void)ctx;
    NVIC_SystemReset();
    return CLI_OK;
}

static const cli_cmd_t commands[] = {
    { "led",    cmd_led,    "led <on|off>" },
    { "reboot", cmd_reboot, "software reset" },
};

static cli_parser_t cli;

void debug_shell_init(void) {
    cli_cfg_t cfg = {
        .commands     = commands,
        .num_commands = 2,
        .write_fn     = uart_write,   /* your UART output */
        .user_ctx     = NULL,
        .prompt       = "> ",
    };
    cli_parser_init(&cli, &cfg);
}

/* Call from UART RX ISR or polling loop */
void debug_shell_rx(uint8_t byte) {
    cli_parser_feed(&cli, byte);
}
```

## Design tradeoffs

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Tokenizer | In-place NUL insertion | Zero allocation, O(n) single pass |
| Quoting | Not supported | Embedded debug shells rarely need quoted args |
| History | Not implemented | Adds RAM cost (ring buffer); can add as opt-in later |
| Echo | Enabled by default | Expected for interactive UART; disable by setting write_fn=NULL |
| Thread safety | Not thread-safe | Callers serialize access (typical: single UART RX context) |

## API review

- Date: 2026-04-28
- Reviewer: @user (pending)
- Key decisions: pending first review

## Before promoting to stable

- [ ] At least 2 real projects use it
- [ ] Evaluate history support (ring buffer, up/down arrow)
- [ ] Evaluate tab completion
- [ ] Real hardware UART test
