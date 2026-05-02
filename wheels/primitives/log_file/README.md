# log_file

**Status: candidate**
**Layer: L0 (primitives)**

FILE* output sink for the `log` wheel. Opens files in append mode for
persistent log capture, or attaches to an existing FILE* (e.g. stderr).
Supports auto-flush for crash-safe logging. Intended for host simulation
and test log capture rather than bare-metal targets.
## Origin

Proactive build — common companion pattern for host-side log capture during
simulation and testing.


## Why self-written?

Searched GitHub (2026-05-02). No lightweight zero-dependency C99 implementation found. 36-line FILE*-based sink adapter; trivial and platform-specific. Self-written to match platform conventions.

## Dependencies

None. Pure C99 with libc stdio (`fopen` / `fwrite` / `fflush` / `fclose`).

**Note**: stdio is not available on all bare-metal targets. This wheel is
intended for host/sim use.

## API

```c
#include "log_file.h"

/* Open a file for append logging */
log_file_open(&lf, "/tmp/test.log", true);  /* auto_flush = true */
log_add_sink(log_file_sink, &lf);

/* Or attach to an existing FILE* */
log_file_attach(&lf, stderr, true);
log_add_sink(log_file_sink, &lf);

/* Clean up (closes file if opened via log_file_open) */
log_file_close(&lf);
```

| Symbol | Description |
|--------|-------------|
| `log_file_t` | File sink context struct |
| `log_file_open(lf, path, auto_flush)` | Open file in binary append mode ("ab") |
| `log_file_attach(lf, fp, auto_flush)` | Attach to existing FILE* (caller owns lifetime) |
| `log_file_sink(ctx, line, len)` | Sink function — writes line to FILE*, optional flush |
| `log_file_close(lf)` | Close file if opened via `log_file_open`; no-op for attached |

## Configuration (compile-time)

No compile-time configuration macros.

## Supported platforms

- [x] Windows host (TDM-GCC) — passed 2026-04-30
- [x] Linux host (gcc) — passed 2026-04-30

## Known issues

None.

## How to test

```bash
cd wheels/primitives/log_file/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests (sim): PASS (6 tests)
- HIL: N/A

## API convention deviations

None. Naming follows `log_file_<verb>` per api-conventions §1.1.

## API review

- **Date**: 2026-04-30
- **Key decisions**: Binary append mode ("ab") for cross-platform consistency.
  Does NOT own FILE* when using `attach` (caller responsible for closing).
  Auto-flush option trades throughput for crash-safety — recommended for
  debug/test logging.

## Responsible human

@user
