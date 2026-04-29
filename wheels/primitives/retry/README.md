# retry

**STATUS: candidate**
**Layer: L0 (primitives)**

Configurable retry strategy with three backoff modes: fixed, linear, exponential.
Pure computation — calculates next delay, caller provides the actual wait
mechanism (RTOS delay / busy-wait / timer callback).

## Why not VSF native?

Checked VSF source (2026-04-29): `vsfip_netif.c` has an internal `retry` counter
for ARP, but no reusable retry strategy utility in service/utilities/component.

## API

```c
#include "retry.h"

typedef enum { RETRY_FIXED, RETRY_LINEAR, RETRY_EXPONENTIAL } retry_strategy_t;

void     retry_init(retry_ctx_t *ctx, const retry_config_t *config);
bool     retry_should_retry(retry_ctx_t *ctx);
uint32_t retry_get_delay(const retry_ctx_t *ctx);
uint8_t  retry_get_attempt(const retry_ctx_t *ctx);
void     retry_reset(retry_ctx_t *ctx);
```

## How to test

```bash
cd wheels/primitives/retry/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **8 C cases** — fixed/linear/exponential strategies, max delay cap,
  linear cap, reset, zero attempts, NULL safety

## Dependencies

None.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

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
