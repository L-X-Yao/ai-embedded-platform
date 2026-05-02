# log_flash

**Status: candidate**
**Layer: L0 (primitives)**

Persistent flash log sink with sector-ring management. Stores log entries
in a circular ring of flash sectors, automatically erasing the oldest
sector when full. HAL injection via function pointers — no dependency
on any specific flash chip or driver.
## Origin

Proactive build — common pattern for embedded crash logging, flight
recorders, and diagnostic log persistence across power cycles.

## Dependencies

None. Pure C99. Flash hardware access injected via `log_flash_hal_t`
function pointers (read/write/erase).

Optional companion: `wheels/primitives/log/` (use `log_flash_sink` as
sink adapter via `log_add_sink(log_flash_sink, &f)`).

## API

```c
#include "log_flash.h"

/* Provide HAL callbacks for your flash chip */
log_flash_hal_t hal = {
    .read  = my_flash_read,   /* int(ctx, offset, buf, len) */
    .write = my_flash_write,  /* int(ctx, offset, data, len) */
    .erase = my_flash_erase,  /* int(ctx, sector_offset, sector_size) */
};

log_flash_t f;
log_flash_init(&f, &hal, NULL,
               0x100000,   /* base offset in flash */
               4096,       /* sector size */
               8);         /* 8 sectors = 32KB ring */

/* As log wheel sink: */
log_add_sink(log_flash_sink, &f);

/* Read back stored logs (oldest first): */
char buf[4096];
size_t n = log_flash_read(&f, buf, sizeof(buf));

/* Statistics: */
log_flash_total(&f);    /* entries written */
log_flash_dropped(&f);  /* entries dropped (too large or write error) */

int rc = log_flash_erase_all(&f); /* wipe and reset; returns 0 or -1 */
```

## Entry format

Each entry stored as: `[magic:1][len_lo:1][len_hi:1][data:len]`

- Magic byte `0xA5` marks valid entry start
- 16-bit little-endian length (max 65535 bytes per entry)
- Entries are packed sequentially within each sector

## Design decisions

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Ring topology | Sector-level ring | Matches flash erase granularity |
| Eviction | Erase oldest sector on wrap | Simple, deterministic |
| Entry header | 3 bytes (magic + len16) | Minimal overhead, supports recovery scan |
| HAL injection | Function pointers | No compile-time dependency on flash driver |
| Thread safety | None built-in | Caller adds mutex if needed |

## Supported platforms

- [x] Windows host (TDM-GCC, RAM-backed mock) — passed 2026-04-30
- [x] Linux host (gcc, RAM-backed mock) — passed 2026-04-30

## Known issues

- No power-loss recovery scan (`log_flash_init` starts at sector 0 position 0). A `log_flash_recover()` function scanning for the last valid magic byte would be needed for production use.

## How to test

```bash
cd wheels/primitives/log_flash/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests (sim): PASS (10 tests — RAM-backed mock flash, 4 sectors x 64 bytes)
- HIL: N/A (needs real flash hardware)

## API convention deviations

None. Naming follows `log_flash_<verb>` per api-conventions §1.1.

## API review

- **Date**: 2026-04-30
- **Key decisions**:
  - Sector-level ring (not byte-level) to match flash erase granularity
  - HAL injection via struct of function pointers
  - No built-in wear leveling — assumes uniform sector usage in ring
  - Power-loss recovery deferred to future enhancement

## Responsible human

@user
