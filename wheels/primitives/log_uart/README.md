# log_uart

**Status: candidate**
**Layer: L0 (primitives)**

UART output sink for the `log` wheel via HAL function pointer injection. The most common embedded log output path -- wraps a user-provided `write(ctx, data, len)` function into a `log_sink_fn`-compatible adapter. Pure C99, zero dependencies.

## Origin

Proactive build as companion sink for `wheels/primitives/log/`. Same HAL injection pattern used by all L1 drivers in the platform.

## Why self-written?

Searched GitHub (2026-05-02). No lightweight zero-dependency C99 implementation found. 20-line UART sink adapter; trivial and platform-specific. Self-written to match platform conventions.

## Dependencies

None. Pure C99 with `<stddef.h>` and `<stdint.h>`.

Designed to pair with `wheels/primitives/log/` (register via `log_add_sink(log_uart_sink, &uart_ctx)`), but has no compile-time dependency on it.

## API

```c
#include "log_uart.h"

/* HAL write callback: user provides this */
typedef void (*log_uart_write_fn)(void *ctx, const uint8_t *data, size_t len);

/* Sink state */
typedef struct {
    log_uart_write_fn write;
    void             *ctx;
} log_uart_t;

/* Initialize (not thread-safe -- call during init) */
void log_uart_init(log_uart_t *u, log_uart_write_fn write, void *ctx);

/* Sink adapter: compatible with log_sink_fn.
 * Pass log_uart_t* as ctx when registering via log_add_sink().
 * Thread-safe if the underlying write function is thread-safe. */
void log_uart_sink(void *ctx, const char *line, size_t len);
```

### Usage example

```c
#define LOG_MODULE "app"
#include "log.h"
#include "log_uart.h"

static void hal_uart_write(void *ctx, const uint8_t *data, size_t len) {
    (void)ctx;
    HAL_UART_Transmit(&huart1, data, len, 100);
}

static log_uart_t g_uart_sink;

int main(void) {
    log_uart_init(&g_uart_sink, hal_uart_write, NULL);
    log_add_sink(log_uart_sink, &g_uart_sink);
    LOG_INFO("boot ok");
}
```

## Supported platforms

- [x] Host (sim) -- passed 2026-04-30
- [ ] ARM Cortex-M (real UART) -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/log_uart/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_log_uart
```

## Test status

- Unit tests (sim): **PASS** (4/4 C tests)
  - Basic write forwarding
  - Multiple writes
  - Context passthrough
  - NULL safety (NULL self, NULL write fn, NULL line, zero length)
- HIL: N/A

## API convention deviations

- **`log_uart_write_fn` uses `_fn` suffix instead of `_t`** (api-conventions S1.3): Function pointer typedefs use `_fn` to distinguish from struct/enum types. Consistent with `log_sink_fn` in the parent `log` wheel.

## Design decisions

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| HAL binding | Function pointer injection | Same pattern as all L1 drivers; no link-time extern, no platform `#ifdef` |
| Write signature | `(void *ctx, const uint8_t *data, size_t len)` | Matches typical MCU UART HAL APIs (STM32 HAL, GD32, etc.) |
| Complexity | Thin wrapper, ~10 lines of logic | Value is standardization and testability, not algorithm |

## API review

- **Date**: 2026-04-30
- **Key decisions**: HAL injection via function pointer (not link-time extern). Same pattern as all L1 drivers.

## Responsible human

@user
