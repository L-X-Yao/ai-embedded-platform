# cli_shell

**Status: candidate**
**Layer: L3 (modules)**

Command-line shell module for embedded debug and configuration. Passive
feed/write design: application feeds bytes one at a time, shell parses
input, dispatches to registered command handlers, and sends output via
a write callback. The application decides the transport (UART, USB CDC,
TCP, RTT).

This is the first L3 wheel. It composes concepts from L0 (cli_parser
pattern: tokenization, line editing) but is self-contained with no
`#include` dependencies on other wheels.

## Origin

Proactive build for embedded debug shells. Common pattern across
projects: UART-connected debug console with modular command registration.
cli_parser provided the L0 foundation; this L3 module adds dynamic
registration and quoted argument support.

## Why self-written?

Searched GitHub (ADR-010 compliance). No suitable existing implementation found. Self-written for platform convention consistency.


## Dependencies

None. Pure C99 with `<string.h>`, `<stddef.h>`, `<stdint.h>` only.

## API

```c
/* I/O abstraction -- application provides write callback */
typedef struct {
    void (*write)(void *ctx, const char *data, uint16_t len);
    void  *ctx;
} cli_shell_io_t;

/* Command handler signature */
typedef void (*cli_shell_handler_t)(void *ctx, int argc, const char **argv,
                                    const cli_shell_io_t *io);

/* Initialize shell (registers built-in "help") */
cli_shell_err_t cli_shell_init(cli_shell_t *sh, const cli_shell_io_t *io,
                               const char *prompt);

/* Register a command dynamically */
cli_shell_err_t cli_shell_register(cli_shell_t *sh, const char *name,
                                   const char *help,
                                   cli_shell_handler_t handler, void *ctx);

/* Feed one byte from a task or polling loop, never from an ISR */
void cli_shell_feed(cli_shell_t *sh, uint8_t byte);
```

## Configuration (compile-time)

| Macro | Default | Description |
|-------|---------|-------------|
| `CLI_SHELL_LINE_SIZE` | 128 | Max input line length (bytes) |
| `CLI_SHELL_MAX_ARGS` | 8 | Max arguments per command |
| `CLI_SHELL_MAX_CMDS` | 16 | Max registered commands (including help), at most 255 |

## Supported platforms

- [x] Linux / Windows host (sim) -- passed 2026-05-01
- [ ] ARM Cortex-M -- not yet verified (no hardware dependency expected)

## Known issues

None.

## How to test

```bash
cd wheels/modules/cli_shell/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **PASS** (26 test cases, 0 failures)
- libFuzzer: types the input and checks the line and the argument split against a separate model
- Coverage: init (valid/null/prompt), register+call, argc/argv correctness,
  multi-arg commands, built-in help, unknown command feedback, empty line,
  backspace, line overflow, sequential commands, max commands (ERR_FULL),
  echo, quoted arguments, CRLF/CR/LF handling, re-init reset,
  null parameter validation, control char filtering, DEL key,
  handler I/O write, whitespace-only input, handler context passing,
  multiple quoted arguments

## API convention deviations

None.


## Before promoting to stable

- [ ] At least 1 real project uses it
- [ ] Evaluate history support (up/down arrow)
- [ ] Evaluate tab completion
- [ ] Real hardware UART test

## Design tradeoffs

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Registration | Dynamic (runtime) | Modules register own commands at startup |
| Tokenizer | In-place NUL insertion | Zero allocation, O(n) single pass |
| Quoting | Double quotes only | Covers common case (strings with spaces) |
| History | Not implemented | RAM cost; add as opt-in wrapper later |
| Tab completion | Not implemented | Complexity/ROM cost; separate wheel if needed |
| Echo | Always on | Expected for interactive terminal |
| Thread safety | Not thread-safe | Caller serializes (typical: single RX context) |
| Max commands | Compile-time array | No malloc, predictable memory footprint |

## Usage example

```c
#include "cli_shell.h"

/* UART write callback */
static void uart_write(void *ctx, const char *data, uint16_t len) {
    (void)ctx;
    uart_send(data, len);
}

/* Command handlers */
static void cmd_led(void *ctx, int argc, const char **argv,
                    const cli_shell_io_t *io) {
    (void)ctx;
    if (argc < 2) {
        io->write(io->ctx, "usage: led <on|off>\r\n", 20);
        return;
    }
    led_set(strcmp(argv[1], "on") == 0);
    io->write(io->ctx, "OK\r\n", 4);
}

static void cmd_status(void *ctx, int argc, const char **argv,
                       const cli_shell_io_t *io) {
    (void)ctx; (void)argc; (void)argv;
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "uptime: %lu s\r\n", get_uptime());
    io->write(io->ctx, buf, (uint16_t)n);
}

/* Setup */
static cli_shell_t shell;
static const cli_shell_io_t shell_io = { uart_write, NULL };

void debug_shell_init(void) {
    cli_shell_init(&shell, &shell_io, "> ");
    cli_shell_register(&shell, "led", "led <on|off>", cmd_led, NULL);
    cli_shell_register(&shell, "status", "show device status", cmd_status, NULL);
}

/* Call from a task or polling loop, never from an ISR: feed echoes through
 * the write callback and runs the command handler. A UART RX ISR queues the
 * byte (ring_buffer, say) and the loop drains the queue into here. */
void debug_shell_rx(uint8_t byte) {
    cli_shell_feed(&shell, byte);
}
```

## Differences from cli_parser (L0)

| Aspect | cli_parser (L0) | cli_shell (L3) |
|--------|-----------------|----------------|
| Command table | Static array at init | Dynamic registration |
| I/O callback | `(buf, len, ctx)` | `(ctx, buf, len)` via io struct |
| Handler signature | Returns error code | Void, writes output via io |
| Quoting | Not supported | Double-quote support |
| Help | Built-in, lists static table | Built-in, lists dynamic table |
| Dependencies | None | None (self-contained) |

cli_shell is designed for applications that need runtime command
registration (e.g., each module registers its own debug commands at
startup). cli_parser is for simpler cases with a fixed command table.

## Features

- **Passive design**: `cli_shell_feed()` processes one byte at a time
- **Dynamic command registration**: `cli_shell_register()` at runtime
- **Built-in help**: auto-registered, lists all commands with descriptions
- **Quoted arguments**: `log "hello world"` parses as argc=2
- **Backspace / DEL handling** with terminal echo
- **Echo**: input bytes echoed back for interactive terminal use
- **CRLF handling**: `\r\n`, `\r`, `\n` all treated as single newline
- **Unknown command feedback**: prints error + suggests "help"
- **Line overflow protection**: a line that outgrows the buffer is refused at end of line with `line too long`
- **Configurable** via compile-time macros (line size, max args, max cmds)
- Pure C99, no heap, no OS dependency, fully reentrant per-context

## Why not an open-source library?

Checked common embedded CLI libraries (2026-05-01):

- **microrl** -- requires malloc or static global, couples to terminal I/O
- **embedded-cli** -- Arduino-centric, C++ API, task-based design
- **cli** -- most C implementations are POSIX-coupled (stdin/stdout)

All violate our wheel contract: zero OS dependency, no malloc, no globals,
callback-injected I/O. A passive, feed-based shell with dynamic command
registration is straightforward to implement (~150 lines) and matches our
platform conventions exactly.

**Conclusion**: no suitable open-source implementation. Self-contained
wheel with zero dependencies.

## API review

- Date: 2026-05-01
- Reviewer: @user (pending)
- Key decisions: pending first review

## Responsible human

@L-X-Yao
