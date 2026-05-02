# log_ringbuf

**Status: candidate**
**Layer: L0 (primitives)**

RAM circular log buffer with line-oriented storage and retrieval. Designed
as a companion sink for the `log` wheel — stores recent log lines in a
fixed-size RAM buffer for crash dump analysis, CLI "show log", or
debugger inspection.
## Origin

Proactive build — common pattern for embedded crash dump and CLI log retrieval.


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None. Pure C99 with libc `memcpy`.

Optional companion: `wheels/primitives/log/` (use `log_ringbuf_sink` as
sink adapter via `log_add_sink(log_ringbuf_sink, &rb)`).

## API

```c
#include "log_ringbuf.h"

log_ringbuf_t rb;
uint8_t buf[1024];

log_ringbuf_init(&rb, buf, sizeof(buf));
log_ringbuf_write(&rb, "[I] boot\n", 9);

/* As log wheel sink: */
log_add_sink(log_ringbuf_sink, &rb);

/* Retrieve stored logs (oldest first): */
char out[1024];
size_t n = log_ringbuf_read(&rb, out, sizeof(out));

/* Statistics: */
log_ringbuf_used(&rb);     /* bytes currently stored */
log_ringbuf_total(&rb);    /* total lines ever written */
log_ringbuf_dropped(&rb);  /* lines dropped due to overflow */

log_ringbuf_clear(&rb);    /* reset without realloc */
```

## Configuration (compile-time)

None — buffer size is runtime (user provides buffer at init).

## Supported platforms

- [x] Windows host (TDM-GCC) — passed 2026-04-30
- [x] Linux host (gcc) — passed 2026-04-30

## Known issues

None.

## How to test

```bash
cd wheels/primitives/log_ringbuf/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests (sim): PASS (12 tests)
- HIL: N/A (pure RAM computation)

## API convention deviations

None. Naming follows `log_ringbuf_<verb>` per api-conventions §1.1.

## API review

- **Date**: 2026-04-30
- **Key decisions**:
  - Byte-level circular buffer (not line-indexed) for simplicity
  - Eviction policy: drop oldest bytes when full
  - `log_ringbuf_sink()` matches `log_sink_fn` signature for zero-glue integration
  - No built-in thread safety — caller protects if needed

## Responsible human

@user
