# log_rtt

**Status: candidate**
**Layer: L0 (primitives)**

Segger RTT-style output sink for the `log` wheel via HAL injection. Supports channel selection. Does NOT depend on the SEGGER_RTT library -- takes a `write(ctx, channel, data, len)` function pointer that matches `SEGGER_RTT_Write()` semantics. Pure C99, zero dependencies.
## Origin

Proactive build as companion sink for `wheels/primitives/log/`. RTT is the second most common embedded debug output after UART, especially during development with J-Link probes.


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None. Pure C99 with `<stddef.h>` and `<stdint.h>`.

Designed to pair with `wheels/primitives/log/` (register via `log_add_sink(log_rtt_sink, &rtt_ctx)`), but has no compile-time dependency on it.

## API

```c
#include "log_rtt.h"

/* HAL write callback: user provides this.
 * Signature matches SEGGER_RTT_Write(unsigned, const void*, unsigned). */
typedef unsigned (*log_rtt_write_fn)(void *ctx, unsigned channel,
                                     const void *data, unsigned len);

/* Sink state */
typedef struct {
    log_rtt_write_fn write;
    void            *ctx;
    unsigned         channel;
} log_rtt_t;

/* Initialize (not thread-safe -- call during init) */
void log_rtt_init(log_rtt_t *r, log_rtt_write_fn write, void *ctx,
                  unsigned channel);

/* Sink adapter: compatible with log_sink_fn.
 * Pass log_rtt_t* as ctx when registering via log_add_sink().
 * Thread-safe if the underlying write function is thread-safe. */
void log_rtt_sink(void *ctx, const char *line, size_t len);
```

### Usage example

```c
#define LOG_MODULE "app"
#include "log.h"
#include "log_rtt.h"
#include "SEGGER_RTT.h"

static unsigned rtt_write(void *ctx, unsigned channel,
                          const void *data, unsigned len) {
    (void)ctx;
    return SEGGER_RTT_Write(channel, data, len);
}

static log_rtt_t g_rtt_sink;

int main(void) {
    SEGGER_RTT_Init();
    log_rtt_init(&g_rtt_sink, rtt_write, NULL, 0);  /* channel 0 */
    log_add_sink(log_rtt_sink, &g_rtt_sink);
    LOG_INFO("boot ok via RTT");
}
```

### Multi-channel example

```c
static log_rtt_t g_rtt_info;  /* channel 0: general logs */
static log_rtt_t g_rtt_perf;  /* channel 1: performance data */

log_rtt_init(&g_rtt_info, rtt_write, NULL, 0);
log_rtt_init(&g_rtt_perf, rtt_write, NULL, 1);
```

## Design decisions

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| HAL binding | Function pointer injection | No SEGGER_RTT library dependency; testable on host without J-Link |
| Write signature | `(void *ctx, unsigned channel, const void *data, unsigned len)` | Mirrors `SEGGER_RTT_Write()` for zero-friction integration |
| Channel | Stored in `log_rtt_t`, set at init | Multiple sinks can target different RTT channels |
| Return value | `unsigned` (bytes written) | Matches SEGGER convention; sink adapter ignores it |

## Supported platforms

- [x] Host (sim) -- passed 2026-04-30
- [ ] ARM Cortex-M (J-Link RTT) -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/log_rtt/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_log_rtt
```

## Test status

- Unit tests (sim): **PASS** (4/4 C tests)
  - Basic write forwarding (channel 0)
  - Channel selection (verifies channel parameter passed through)
  - Multiple writes
  - NULL safety (NULL self, NULL write fn, NULL line, zero length)
- HIL: N/A

## API convention deviations

- **`log_rtt_write_fn` uses `_fn` suffix instead of `_t`** (api-conventions S1.3): Function pointer typedefs use `_fn` to distinguish from struct/enum types. Consistent with `log_sink_fn` in the parent `log` wheel.

## API review

- **Date**: 2026-04-30
- **Key decisions**: Channel-aware write signature matches SEGGER_RTT_Write() semantics but without vendor dependency.

## Responsible human

@user
