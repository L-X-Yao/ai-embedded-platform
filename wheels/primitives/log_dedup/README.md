# log_dedup

**Status: candidate**
**Layer: L0 (primitives)**

Deduplication sink for the `log` wheel. Suppresses consecutive identical log lines (ignoring timestamp differences). When a different line arrives after N repeats, emits `[repeated N times]` summary before forwarding the new line. Pure C99, zero dependencies.

## Origin

Proactive build as companion sink for `wheels/primitives/log/`. Sensor error loops and periodic status messages commonly produce long runs of identical lines that obscure useful output. This wheel intercepts at the sink level, keeping call sites clean.

## Why self-written?

Searched GitHub (2026-05-02). No lightweight zero-dependency C99 implementation found. Dedup-by-hash sink is unique to this platform's composable log ecosystem. Self-written to match platform conventions.

## Dependencies

None. Pure C99 with `<stddef.h>`, `<stdint.h>`, and `<string.h>`.

Designed to pair with `wheels/primitives/log/` (register via `log_add_sink(log_dedup_sink, &dedup_ctx)`), but has no compile-time dependency on it.

## API

```c
#include "log_dedup.h"

#ifndef LOG_DEDUP_LINE_MAX
#define LOG_DEDUP_LINE_MAX  256  /* max line buffer for comparison */
#endif

/* Inner sink: where accepted (non-duplicate) lines go */
typedef void (*log_dedup_output_fn)(void *ctx, const char *line, size_t len);

/* Sink state */
typedef struct {
    log_dedup_output_fn inner_fn;
    void               *inner_ctx;
    char                last_line[LOG_DEDUP_LINE_MAX];
    size_t              last_len;
    uint32_t            repeat_count;
    uint32_t            total_suppressed;
} log_dedup_t;

/* Initialize (not thread-safe -- call during init) */
void log_dedup_init(log_dedup_t *d, log_dedup_output_fn inner_fn, void *inner_ctx);

/* Sink adapter: compatible with log_sink_fn.
 * Suppresses consecutive identical lines. When a new different line
 * arrives after N repeats, emits "[repeated N times]" before the new line.
 * Not thread-safe (modifies last_line state). */
void log_dedup_sink(void *ctx, const char *line, size_t len);

/* Flush pending repeat count (call before shutdown).
 * If the last line was repeated, emits "[repeated N times]".
 * If the last line was unique, does nothing extra. */
void log_dedup_flush(log_dedup_t *d);

/* Return total lines suppressed since init. */
uint32_t log_dedup_suppressed(const log_dedup_t *d);
```

### Usage example

```c
#define LOG_MODULE "main"
#include "log.h"
#include "log_dedup.h"
#include "log_uart.h"

static log_uart_t  g_uart;
static log_dedup_t g_dedup;

int main(void) {
    log_uart_init(&g_uart, hal_uart_write, NULL);

    /* Dedup -> UART chain */
    log_dedup_init(&g_dedup, log_uart_sink, &g_uart);
    log_add_sink(log_dedup_sink, &g_dedup);

    LOG_ERROR("sensor offline");  /* forwarded */
    LOG_ERROR("sensor offline");  /* suppressed */
    LOG_ERROR("sensor offline");  /* suppressed */
    LOG_INFO("sensor online");    /* emits "[repeated 2 times]", then forwards */
}
```

### Shutdown flush

```c
/* Before system halt, flush any pending repeat summary */
log_dedup_flush(&g_dedup);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `LOG_DEDUP_LINE_MAX` | 256 | Maximum line length for comparison buffer. Lines longer than this are truncated for comparison purposes. |

## Supported platforms

- [x] Host (sim) -- passed 2026-04-30
- [ ] ARM Cortex-M -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/log_dedup/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_log_dedup
```

## Test status

- Unit tests (sim): **PASS** (8/8 C tests)
  - No duplicates (3 distinct lines -- all forwarded, 0 suppressed)
  - Consecutive duplicates (3 identical lines -- 1 forwarded, 2 suppressed)
  - Repeat message on change (emits `[repeated N times]` when different line arrives)
  - Flush pending (emits repeat summary on explicit flush)
  - Timestamp ignored for comparison (different `[XXXXXXXX]` prefixes, same body -- deduplicated)
  - No timestamp comparison (lines without timestamp prefix -- still deduplicated)
  - Single line no repeat (flush after one unique line emits no repeat summary)
  - NULL safety (NULL self, NULL inner fn)
- HIL: N/A

## API convention deviations

- **`log_dedup_output_fn` uses `_fn` suffix instead of `_t`** (api-conventions S1.3): Function pointer typedefs use `_fn` to distinguish from struct/enum types. Consistent with `log_sink_fn` in the parent `log` wheel.

## API review

- **Date**: 2026-04-30
- **Key decisions**: Timestamp-aware comparison (strips `[XXXXXXXX]` prefix). 256-byte line buffer. Flush before shutdown to emit pending repeat count.

## Responsible human

@user

## Design decisions

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Comparison | Timestamp-aware: strips `[XXXXXXXX] ` prefix (10 chars) before comparing | Same message at different times should still deduplicate |
| Buffer | 256-byte fixed line buffer in `log_dedup_t` | Matches `LOG_LINE_BUFSIZE` default in `log` wheel; no heap allocation |
| Summary format | `[repeated N times]\n` | Matches Linux kernel `dmesg` convention |
| Flush | Explicit `log_dedup_flush()` before shutdown | Ensures pending repeat count is not lost on system halt |
| Scope | Consecutive only | Non-consecutive duplicates are not tracked (would require hash table / heap) |
| Thread safety | Not thread-safe | Caller is responsible for locking if multi-threaded; keeps the wheel simple |
