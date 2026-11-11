# datetime -- epoch <-> calendar struct conversion

**Status: candidate**
**Layer: L0 (primitives)**

Unix epoch (uint32_t seconds since 2000-01-01 00:00:00 UTC) to calendar struct
conversion and vice versa. Epoch base is 2000-01-01 (not 1970), fitting uint32_t
until 2136. Includes leap year handling, weekday computation, and add/subtract
seconds with rollover across day/month/year boundaries.

## Origin

Proactive build for embedded RTC integration and timestamping. Common need in
data loggers, telemetry systems, and any device that needs human-readable time.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-12):

- POSIX `time.h` / `gmtime`: requires libc with 1970 epoch; not available on bare-metal.
- lwIP `sntp`: couples to network stack; overkill for offline time math.
- No suitable zero-dependency C99 implementation with 2000 epoch base found.

Reference: Tomohiko Sakamoto's weekday algorithm.

## Dependencies

None.

## API

```c
#include "datetime.h"

datetime_err_t datetime_from_epoch(uint32_t epoch, datetime_t *out);
datetime_err_t datetime_to_epoch(const datetime_t *dt, uint32_t *out);
bool           datetime_is_leap_year(uint16_t year);
uint8_t        datetime_days_in_month(uint16_t year, uint8_t month);
datetime_err_t datetime_add_seconds(datetime_t *dt, int32_t seconds);
```

## Configuration (compile-time)

None.

## Supported platforms

- [x] Host (sim) -- 2026-05-12
- [ ] ARM Cortex-M -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/datetime/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_datetime
```

## Test status

- Unit tests (sim): PASS (18 cases)
- HIL: N/A (pure compute)

## API convention deviations

None. No `_init` function needed; all functions are stateless.

## API review

- **Date**: 2026-05-12
- **Key decisions**:
  - Epoch base 2000-01-01 (fits embedded RTC chips, uint32_t until 2136)
  - Weekday computed via Tomohiko Sakamoto algorithm
  - Year range validated 2000-2099; from_epoch can decode beyond 2099
  - No timezone support (UTC only)

## Responsible human

@L-X-Yao
