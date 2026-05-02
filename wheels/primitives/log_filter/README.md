# log_filter

**Status: candidate**
**Layer: L0 (primitives)**

Module-based runtime filter sink for the `log` wheel. Wraps any existing sink
and forwards only lines whose `[module]` tag matches an ALLOW list or does not
match a DENY list. Lines with no parseable module tag are always forwarded.
## Origin

Proactive build — common companion pattern for leveled logging sinks in
multi-module embedded systems.


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None. Pure C99 with libc `strncmp` / `strlen`.

## API

```c
#include "log_filter.h"

/* Initialize in ALLOW mode: only forward lines from listed modules */
log_filter_init(&f, inner_sink_fn, inner_ctx, LOG_FILTER_ALLOW);
log_filter_add(&f, "ble");
log_filter_add(&f, "app");
log_add_sink(log_filter_sink, &f);

/* Or DENY mode: forward everything except listed modules */
log_filter_init(&f, inner_sink_fn, inner_ctx, LOG_FILTER_DENY);
log_filter_add(&f, "driver");  /* suppress noisy driver logs */
log_add_sink(log_filter_sink, &f);

/* Runtime management */
log_filter_clear(&f);          /* remove all module entries */
```

| Symbol | Description |
|--------|-------------|
| `log_filter_t` | Filter context struct |
| `log_filter_init(f, inner_fn, inner_ctx, mode)` | Initialize with ALLOW or DENY mode |
| `log_filter_add(f, module)` | Add a module tag to the filter list |
| `log_filter_clear(f)` | Remove all module entries |
| `log_filter_sink(ctx, line, len)` | Sink function — parses `[module]`, applies filter, forwards to inner |

Parsing depends on log output format: `[timestamp] [L] [module] message`.
The filter extracts the third bracketed field as the module tag.

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `LOG_FILTER_MAX_MODULES` | 8 | Maximum number of module entries in a single filter |

## Supported platforms

- [x] Windows host (TDM-GCC) — passed 2026-04-30
- [x] Linux host (gcc) — passed 2026-04-30

## Known issues

None.

## How to test

```bash
cd wheels/primitives/log_filter/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests (sim): PASS (8 tests)
- HIL: N/A

## API convention deviations

None. Naming follows `log_filter_<verb>` per api-conventions §1.1.

## API review

- **Date**: 2026-04-30
- **Key decisions**: ALLOW/DENY dual mode covers both "show only these" and
  "hide these noisy ones" use cases. Up to LOG_FILTER_MAX_MODULES (default 8)
  modules per filter instance. Lines without module brackets pass through
  unfiltered to avoid silently dropping malformed output.

## Responsible human

@user
