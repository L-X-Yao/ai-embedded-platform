# log

**Status: candidate**
**Layer: L0 (primitives)**

Leveled logging with module tags, multi-sink output, and compile-time filtering.
Pure C99, zero dependencies.

## Origin

First wheel built for the platform (Sprint 0). Not extracted from a project — designed as canonical example for wheel structure, testing, and CI patterns. Consumed by drone_light BSP log adapter.

## Why self-written?

Searched GitHub (2026-05-02). Referenced rxi/log.c (1k stars, MIT) for design comparison. rxi/log.c uses global state and FILE*; our composable multi-sink design with function pointer injection is deliberately different and better suited to embedded targets. Self-written for platform convention consistency; algorithm cross-validated against reference.

## Dependencies

None. Pure C99 with libc `snprintf` / `vsnprintf`.

## API

```c
/* Declare module name at top of each .c file */
#define LOG_MODULE "mymodule"
#include "log.h"

/* Four main macros */
LOG_ERROR("...");
LOG_WARN("...");
LOG_INFO("...");
LOG_DEBUG("...");

/* Compile-time filtering (set LOG_LEVEL_WARN in release to strip DEBUG/INFO strings) */
#define LOG_LEVEL_MAX  LOG_LEVEL_DEBUG

/* Runtime */
log_set_level(LOG_LEVEL_INFO);
log_get_level();                     /* query current runtime level */
log_add_sink(my_sink_fn, ctx);       /* custom sink, returns sink id */
log_remove_sink(sink_id);            /* remove registered sink */
log_set_time_fn(my_ms_fn);           /* NULL disables timestamp */
```

## Configuration (compile-time)

| Macro | Default | Description |
|-------|---------|-------------|
| `LOG_LEVEL_MAX` | `LOG_LEVEL_DEBUG` | Compile-time max level (higher levels optimized out) |
| `LOG_MAX_SINKS` | 4 | Maximum registered sinks |
| `LOG_LINE_BUFSIZE` | 256 | Max bytes per line (prefix + message + newline) |

## Supported platforms

- [x] Linux host (gcc) — passed 2026-04-30
- [x] Windows host (TDM-GCC) — passed 2026-04-30

(L0 wheels don't require real hardware, but can be ticked off when a project uses it.)

## Known issues

None so far.

## How to test

```bash
cd wheels/primitives/log
cmake -GNinja -S test -B test/build
cmake --build test/build
ctest --test-dir test/build --output-on-failure
python3 -m pytest test/test_log.py -v
```

Expected: **12 C tests + 13 pytest cases all green** (including release build compile-time elimination verification).

## Test status

- Unit tests (sim): PASS (12/12 C tests + 4/4 pytest)
- HIL: N/A

## API convention deviations

- **`log_sink_fn` uses `_fn` suffix instead of `_t`** (api-conventions §1.3): Function pointer typedefs use `_fn` to distinguish from struct/enum types. Common C idiom for readability. Not renamed to avoid breaking existing consumers.

## API review

- Proposal date: 2026-04-21
- Reviewer: @user
- Key decisions: accepted all default recommendations (Q1-Q5 per AI suggestion)

## Responsible human

@user (2-3 person team)

## Output format

```
[00012345] [I] [ble] connected to peer 0a:1b:2c
[00012367] [W] [driver] i2c retry #1
[00012389] [E] [app] config crc mismatch
```

- `[XXXXXXXX]` timestamp (ms, 8-digit padded), can be disabled
- `[L]` level character `E/W/I/D`
- `[module]` caller's `LOG_MODULE` value

## Design tradeoffs

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Module tagging | `#define LOG_MODULE` per file | Simplest, no linker dependency, compile-time constant enables optimization |
| Compile-time filter | `if ((level) <= LOG_LEVEL_MAX)` | Compiler eliminates dead branch + string literals |
| Sink registration | Dynamic list (runtime) | Flexible, pluggable across projects; zero overhead when idle |
| No framework dependency | Output via injected sink function pointers | Projects bridge to vsf_trace / UART / RTT as needed |
| Thread safety | Stack buffer 256B per log_emit call | No shared state, no lock needed; tradeoff is stack usage |
| **Stack budget** | **log_emit=256B + each sink's own buffer** | **With log_color(512B): ~800B per LOG_* call. Tune LOG_LINE_BUFSIZE/LOG_COLOR_BUFSIZE for tight stacks.** |
| Structured logging | Not implemented | Typical embedded products don't need JSON logs |

## Companion sink wheels

All sinks use the same `log_sink_fn` signature, composable via `log_add_sink()`.

| Sink | Wheel | Type | Description |
|------|-------|------|-------------|
| stdout | **built-in** | output | `log_add_stdout_sink()` — fprintf to stdout |
| UART | `log_uart` | output | HAL function pointer injection, any UART |
| RTT | `log_rtt` | output | Segger RTT-style, channel-aware, HAL injected |
| File | `log_file` | output | FILE* with append mode + auto-flush |
| Ring buffer | `log_ringbuf` | output | RAM circular buffer for crash dump / CLI |
| Flash | `log_flash` | output | Persistent sector-ring on SPI flash, HAL injected |
| Color | `log_color` | transform | ANSI color wrapper (E=red W=yellow I=green D=gray) |
| Filter | `log_filter` | transform | Module-based ALLOW/DENY runtime filter |
| Rate limit | `log_rate` | transform | Token bucket, prevents log flood |
| Dedup | `log_dedup` | transform | Suppress consecutive identical lines |

**Composition example** (color UART + filtered flash + crash ringbuf):

```c
log_color_init(&color, uart_write_fn, NULL);
log_filter_init(&filter, log_flash_sink, &flash, LOG_FILTER_ALLOW);
log_filter_add(&filter, "imu");

log_add_sink(log_color_sink, &color);      /* all levels → colored UART */
log_add_sink(log_filter_sink, &filter);    /* imu only → flash */
log_add_sink(log_ringbuf_sink, &ringbuf);  /* all levels → RAM ring */
```

## Usage examples

### UART sink (bare-metal)

```c
#define LOG_MODULE "app"
#include "log.h"

static void uart_sink(void *ctx, const char *line, size_t len) {
    (void)ctx;
    uart_write((const uint8_t *)line, len);
}

int main(void) {
    log_add_sink(uart_sink, NULL);
    log_set_level(LOG_LEVEL_INFO);
    LOG_INFO("boot ok version=%d.%d", 1, 2);
}
```

### Multi-sink (UART + Flash)

```c
#define LOG_MODULE "ble"
#include "log.h"

void ble_init(void) {
    log_add_sink(uart_sink, NULL);
    log_add_sink(flash_sink, &flash_ctx);
    LOG_INFO("ble stack started");
}
```

### VSF project bridge

```c
static void vsf_trace_sink(void *ctx, const char *line, size_t len) {
    (void)ctx;
    __vsf_trace_output(line, (uint_fast32_t)len);
}
log_add_sink(vsf_trace_sink, NULL);
```

## Before promoting to stable

- [ ] At least 1 real project uses it
- [ ] Add "Performance reference" section (log_emit latency / RAM footprint)
- [ ] Add "Changelog" section
- [ ] Cross-platform tick (at least STM32/ESP32 verified on real hardware)
- [ ] Evaluate rtdbg.h reference points (`_eval/rtthread_rtdbg/`): ANSI color (needs per-sink scheme) / `LOG_HERE` (function+line) / `LOG_RAW` (skip prefix). All are **new API** — go through wheel-contract §7 approval when triggered by a real project. Hex dump: use VSF `vsf_trace_dump_buff` (ADR-006), don't reinvent.
