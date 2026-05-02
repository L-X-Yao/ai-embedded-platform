# timeout

**Status: candidate**
**Layer: L0 (primitives)**

Deadline-based timeout tracking with 32-bit tick wraparound handling. Uses
caller-provided tick value — no HAL dependency, no timer hardware required.
Suitable for cooperative polling loops, communication timeouts, and watchdog
feeding intervals.
## API

```c
#include "timeout.h"

void     timeout_set(timeout_t *t, uint32_t now, uint32_t duration);
bool     timeout_expired(const timeout_t *t, uint32_t now);
uint32_t timeout_remaining(const timeout_t *t, uint32_t now);
uint32_t timeout_elapsed(const timeout_t *t, uint32_t now);
void     timeout_restart(timeout_t *t, uint32_t now);
void     timeout_extend(timeout_t *t, uint32_t extra);
```

## How to test

```bash
cd wheels/primitives/timeout/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **9 C cases** — basic expiry, remaining, elapsed, 32-bit wraparound,
  restart, extend, zero duration, max duration, NULL safety

## Dependencies

None.

## Origin

Proactive build for common embedded components (2026-04-29 batch).


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Known issues

None.

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## API convention deviations

无。

## Responsible human

@user
