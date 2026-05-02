# cli_parser

**Status: candidate**
**Layer: L0 (primitives)**

Minimal CLI parser for embedded debug shells. Character-at-a-time feed
from UART, command table dispatch, built-in help, line editing (backspace).
Pure C99, zero dependencies, no heap allocation.

## Origin

Proactive build for embedded debug shells. Not extracted from a specific project — common pattern across flybox (USART0 debug) and drone_light (development diagnostics).

## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None. Pure C99 with `<string.h>` and `<stddef.h>` only.

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

## Supported platforms

- [x] Linux / Windows host (sim) — passed 2026-04-28
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/cli_parser/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **PASS** (53 assertions across 25 test cases)
- Coverage: init, known/unknown/empty commands, argument parsing, whitespace
  handling, tab tokenization, mixed spaces+tabs, help, handler errors, overflow,
  max args truncation, null safety, feed+enter, backspace, DEL, feed overflow,
  backspace on empty, null commands, CR-only line ending, CRLF sequence,
  control characters ignored

## API review

- Date: 2026-04-28
- Reviewer: @user (pending)
- Key decisions: pending first review

## Responsible human

@user

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

## Before promoting to stable

- [ ] At least 2 real projects use it
- [ ] Evaluate history support (ring buffer, up/down arrow)
- [ ] Evaluate tab completion
- [ ] Real hardware UART test
