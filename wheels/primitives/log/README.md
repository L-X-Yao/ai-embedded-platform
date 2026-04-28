# log

**STATUS: candidate**
**Layer: L0 (primitives)**

Leveled logging with module tags, multi-sink output, and compile-time filtering.
Built on top of `vsf_trace`.

## Why not VSF native?

Checked VSF modules:

- `vsf_trace` (`service/trace/`): provides levels (NONE/ERROR/WARNING/INFO/DEBUG), printf-style, color, stream output, buffer hex dump. **Covers ~60% of requirements.**
- `vsf_bgtrace` (same file): typed background ring buffer trace for high-frequency tracing. Non-competing with this wheel.

**4 capabilities VSF native lacks** (common in embedded products):
1. **Module tagging** — cannot distinguish BLE / driver / app origin in logs
2. **Multi-sink output** — vsf_trace binds to a single stream, cannot UART + flash + remote in parallel
3. **Compile-time level filtering** — vsf_trace is runtime-only, DEBUG strings still compiled into release binary
4. **Timestamps** — vsf_trace does not auto-prepend time

**Conclusion**: this wheel **wraps vsf_trace**, does not replace it. Default sink (`log_add_vsf_trace_sink`) bypasses vsf_trace formatting and outputs pre-formatted lines via `__vsf_trace_output()` to vsf_trace's bound stream. Reuses vsf_trace's mature stream + lock protection, no custom low-level IO.

## How to test

```bash
cd wheels/primitives/log

# Default build (LOG_LEVEL_MAX=LOG_LEVEL_DEBUG, all levels emit)
VSF_PATH=/mnt/d/temp/project/ai-embedded-platform/vsf cmake -GNinja -S . -B build
cmake --build build

# Release-style build (LOG_LEVEL_MAX=LOG_LEVEL_WARN, DEBUG/INFO eliminated by compiler)
# Verifies that DEBUG/INFO macros expand to ((void)0) with no side-effect evaluation
VSF_PATH=/mnt/d/temp/project/ai-embedded-platform/vsf cmake -GNinja -S . -B build_release -DCMAKE_C_FLAGS='-DLOG_LEVEL_MAX=LOG_LEVEL_WARN'
cmake --build build_release

python3 -m pytest test_log.py -v
```

Expected: **11 C tests + 4 pytest cases all green** (including release build compile-time elimination verification).

## API summary

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
log_add_vsf_trace_sink();            /* built-in: route to vsf_trace stream */
log_add_stream_sink(&my_stream);     /* any VSF stream */
log_add_sink(my_sink_fn, ctx);       /* custom sink, returns sink id */
log_remove_sink(sink_id);            /* remove registered sink */
log_set_time_fn(my_ms_fn);           /* NULL disables timestamp */
```

## Output format

```
[00012345] [I] [ble] connected to peer 0a:1b:2c
[00012367] [W] [driver] i2c retry #1
[00012389] [E] [app] config crc mismatch
```

- `[XXXXXXXX]` timestamp (ms, 8-digit padded), can be disabled
- `[L]` level character `E/W/I/D`
- `[module]` caller's `LOG_MODULE` value

## Configuration macros

| Macro | Default | Description |
|-------|---------|-------------|
| `LOG_LEVEL_MAX` | `LOG_LEVEL_DEBUG` | Compile-time max level (higher levels optimized out) |
| `LOG_MAX_SINKS` | 4 | Maximum registered sinks |
| `LOG_LINE_BUFSIZE` | 256 | Max bytes per line (prefix + message + newline) |

## Design tradeoffs

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Module tagging | `#define LOG_MODULE` per file | Simplest, no linker dependency, compile-time constant enables optimization |
| Compile-time filter | `if ((level) <= LOG_LEVEL_MAX)` | Compiler eliminates dead branch + string literals |
| Sink registration | Dynamic list (runtime) | Flexible, pluggable across projects; zero overhead when idle |
| vsf_trace integration | Bypass its formatting, reuse stream output | Avoids double prefix, reuses mature lock protection |
| Thread safety | Stack buffer 256B per log_emit call | No shared state, no lock needed; tradeoff is stack usage |
| Structured logging | Not implemented | Typical embedded products don't need JSON logs |

## Origin

First wheel built for the platform (Sprint 0). Not extracted from a project — designed as canonical example for wheel structure, testing, and CI patterns. Consumed by drone_light BSP log adapter.

## Dependencies

- VSF kernel
- `VSF_USE_SIMPLE_STREAM = ENABLED`
- `VSF_USE_TRACE = ENABLED` (if using `log_add_vsf_trace_sink`)
- libc `snprintf` / `vsnprintf` (available via linux skin simple_libc)

## Supported platforms

- [x] VSF linux host (sim) — passed 2026-04-21
- [x] VSF mps2-an500 bare-metal (QEMU) — 2026-04-22 via custom semihosting sink, see `tooling/experiments/vsf-tryouts/mps2_bare/`
- [ ] VSF + STM32F4 — not yet verified
- [ ] VSF + ESP32 — not yet verified

(L0 wheels don't require real hardware, but can be ticked off when a project uses it.)

## Known issues

None so far.

## Test status

- Unit tests (sim): PASS (11/11 C tests + 4/4 pytest)
- HIL: N/A

## Usage examples

### 基础

```c
#include "log.h"
#define LOG_MODULE "app"

int main(void) {
    log_add_vsf_trace_sink();           /* 默认路由到 vsf_trace */
    log_set_level(LOG_LEVEL_INFO);
    LOG_INFO("boot ok version=%d.%d", 1, 2);
    return 0;
}
```

### 多 sink

```c
#include "log.h"
#define LOG_MODULE "ble"

extern vsf_stream_t flash_log_stream;

void ble_init(void) {
    log_add_stream_sink(&flash_log_stream);   /* 日志同时存 flash */
    LOG_INFO("ble stack started");
}
```

### 自定义 sink

```c
static void my_net_sink(void *ctx, const char *line, size_t len) {
    /* 发到远程 MQTT topic 等 */
    (void)ctx;
    mqtt_publish("device/log", line, len);
}

log_add_sink(my_net_sink, NULL);
```

## Responsible human

@user (2-3 person team)

## Convention Deviations

- **`log_sink_fn` uses `_fn` suffix instead of `_t`** (api-conventions §1.3): Function pointer typedefs use `_fn` to distinguish from struct/enum types. Common C idiom for readability. Not renamed to avoid breaking existing consumers.

## API review

- Proposal date: 2026-04-21
- Reviewer: @user
- Key decisions: accepted all default recommendations (Q1-Q5 per AI suggestion)

## Before promoting to stable

- [ ] At least 1 real project uses it
- [ ] Add "Performance reference" section (log_emit latency / RAM footprint)
- [ ] Add "Changelog" section
- [ ] Cross-platform tick (at least STM32/ESP32 verified on real hardware)
- [ ] Evaluate rtdbg.h reference points (`_eval/rtthread_rtdbg/`): ANSI color (needs per-sink scheme) / `LOG_HERE` (function+line) / `LOG_RAW` (skip prefix). All are **new API** — go through wheel-contract §7 approval when triggered by a real project. Hex dump: use VSF `vsf_trace_dump_buff` (ADR-006), don't reinvent.
