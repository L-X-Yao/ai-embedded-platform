# log_rate

**Status: candidate**
**Layer: L0 (primitives)**

Token bucket rate limiter sink for the `log` wheel. Prevents log flood from overwhelming output bandwidth (UART, RTT, flash). Configurable: max N lines per interval, time source injected via function pointer. Pure C99, zero dependencies, no OS requirement.
## Origin

Proactive build as companion sink for `wheels/primitives/log/`. Log flood is a recurring embedded problem: a sensor error loop or protocol retry storm can saturate UART and mask other output. This wheel intercepts at the sink level rather than requiring every call site to self-throttle.


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None. Pure C99 with `<stddef.h>` and `<stdint.h>`.

Designed to pair with `wheels/primitives/log/` (register via `log_add_sink(log_rate_sink, &rate_ctx)`), but has no compile-time dependency on it. The time source is injected -- no OS timer dependency.

## API

```c
#include "log_rate.h"

/* Inner sink: where accepted lines go */
typedef void (*log_rate_output_fn)(void *ctx, const char *line, size_t len);

/* Time source: returns current time in ms (injected, no OS dependency) */
typedef uint32_t (*log_rate_time_fn)(void);

/* Sink state */
typedef struct {
    log_rate_output_fn inner_fn;
    void              *inner_ctx;
    log_rate_time_fn   time_fn;
    uint32_t           max_tokens;
    uint32_t           refill_interval_ms;
    uint32_t           tokens;
    uint32_t           last_refill_ms;
    uint32_t           dropped;
} log_rate_t;

/* Initialize rate limiter.
 *   max_per_interval: max lines forwarded per interval
 *   interval_ms: refill interval in milliseconds
 *   time_fn: returns current time in ms
 * Not thread-safe -- call during init. */
void log_rate_init(log_rate_t *r, log_rate_output_fn inner_fn, void *inner_ctx,
                   log_rate_time_fn time_fn,
                   uint32_t max_per_interval, uint32_t interval_ms);

/* Sink adapter: compatible with log_sink_fn.
 * Pass log_rate_t* as ctx when registering via log_add_sink().
 * Not thread-safe (modifies token state). */
void log_rate_sink(void *ctx, const char *line, size_t len);

/* Return number of lines dropped since init/reset. */
uint32_t log_rate_dropped(const log_rate_t *r);

/* Reset token bucket and dropped counter. */
void log_rate_reset(log_rate_t *r);
```

### Usage example

```c
#define LOG_MODULE "sensor"
#include "log.h"
#include "log_rate.h"
#include "log_uart.h"

static log_uart_t  g_uart;
static log_rate_t  g_rate;

static uint32_t sys_tick_ms(void) { return HAL_GetTick(); }

int main(void) {
    log_uart_init(&g_uart, hal_uart_write, NULL);

    /* Max 10 lines per second, forwarded to UART */
    log_rate_init(&g_rate, log_uart_sink, &g_uart,
                  sys_tick_ms, 10, 1000);

    log_add_sink(log_rate_sink, &g_rate);

    /* ... sensor loop that may spam errors */
    for (;;) {
        LOG_ERROR("i2c nack");  /* only 10/sec reach UART */
    }
}
```

### Checking dropped count

```c
if (log_rate_dropped(&g_rate) > 0) {
    LOG_WARN("rate limiter dropped %u lines", log_rate_dropped(&g_rate));
    log_rate_reset(&g_rate);
}
```

## Design decisions

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Algorithm | Token bucket | Simple, bounded memory, well-understood; refill is one comparison per call |
| Time source | Injected `time_fn` | No OS/HAL dependency; testable with fake clock |
| Granularity | Line-level (not byte-level) | Matches log semantics; a single `LOG_ERROR(...)` is one token |
| Dropped counter | Monotonic `uint32_t` | Diagnostic: callers can periodically report dropped count |
| Thread safety | Not thread-safe | Caller is responsible for locking if multi-threaded; keeps the wheel simple |

## Supported platforms

- [x] Host (sim) -- passed 2026-04-30
- [ ] ARM Cortex-M -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/log_rate/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_log_rate
```

## Test status

- Unit tests (sim): **PASS** (6/6 C tests)
  - Within limit (3 lines, limit 3 -- all forwarded)
  - Exceeds limit (4 lines, limit 2 -- 2 dropped)
  - Refill after interval (tokens replenished after elapsed time)
  - Multiple intervals (verifies repeated refill cycles)
  - Reset (dropped counter and tokens cleared)
  - NULL safety (NULL self, NULL inner fn, NULL time fn)
- HIL: N/A

## API convention deviations

- **`log_rate_output_fn` and `log_rate_time_fn` use `_fn` suffix instead of `_t`** (api-conventions S1.3): Function pointer typedefs use `_fn` to distinguish from struct/enum types. Consistent with `log_sink_fn` in the parent `log` wheel.

## API review

- **Date**: 2026-04-30
- **Key decisions**: Token bucket algorithm, time injection (no OS dependency), dropped counter for diagnostics.

## Responsible human

@user
