# log_color

**Status: candidate**
**Layer: L0 (primitives)**

ANSI color wrapper sink for the `log` wheel. Wraps any existing sink and
prepends/appends terminal color codes based on the log level character
(ERROR=red, WARN=yellow, INFO=green, DEBUG=gray). Lines with no recognized
level char are passed through unchanged.
## Origin

Proactive build — common companion pattern for leveled logging sinks.

## Dependencies

None. Pure C99 with libc `memcpy` / `strlen`.

## API

```c
#include "log_color.h"

/* Initialize: wraps an existing sink */
log_color_init(&lc, inner_sink_fn, inner_ctx);

/* Register as a log sink (compatible with log_sink_fn) */
log_add_sink(log_color_sink, &lc);
```

| Symbol | Description |
|--------|-------------|
| `log_color_t` | Opaque wrapper context struct |
| `log_color_init(lc, inner_fn, inner_ctx)` | Bind to an inner sink |
| `log_color_sink(ctx, line, len)` | Sink function — wraps line with ANSI codes, forwards to inner |

Color mapping:
- `[E]` — `\033[31m` (red)
- `[W]` — `\033[33m` (yellow)
- `[I]` — `\033[32m` (green)
- `[D]` — `\033[90m` (gray)
- No match — passthrough, no codes added

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `LOG_COLOR_BUFSIZE` | 512 | Stack buffer size for colored output |

## Supported platforms

- [x] Windows host (TDM-GCC) — passed 2026-04-30
- [x] Linux host (gcc) — passed 2026-04-30

## Known issues

None.

## How to test

```bash
cd wheels/primitives/log_color/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests (sim): PASS (9 tests)
- HIL: N/A

## API convention deviations

None. Naming follows `log_color_<verb>` per api-conventions §1.1.

## API review

- **Date**: 2026-04-30
- **Key decisions**: Wraps rather than modifies the original sink (decorator pattern).
  Color codes are ANSI standard (works on any modern terminal, UART terminal emulator,
  or Segger RTT viewer). Uses 512-byte stack buffer for colored output to avoid heap.

## Responsible human

@user
