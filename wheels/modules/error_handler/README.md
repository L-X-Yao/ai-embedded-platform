# error_handler

**STATUS: candidate**
**Layer: L3 (modules)**

Fault logging and recovery module for embedded systems. Ring buffer of
error records with severity levels, timestamps, and arbitrary context
data. Fatal errors trigger a callback for application-defined recovery.
Storage callbacks enable persistence across reboots.

Passive design: application calls `error_handler_report()` when errors
occur. No OS, no tasks, no malloc.

## Why not an open-source library?

Checked common embedded error/fault handling libraries (2026-05-01):

- **CException** -- setjmp/longjmp based, C++ exception semantics, no
  record logging or persistence
- **err.h / errno patterns** -- thread-local globals, no ring buffer,
  no severity, no persistence
- **CMSIS fault handlers** -- ARM-specific, ISR-coupled, not portable

All violate our wheel contract: zero OS dependency, no malloc, no
globals, callback-injected storage. A passive ring-buffer error
recorder with severity, fatal callbacks, and pluggable storage is
straightforward (~130 lines) and matches platform conventions exactly.

**Conclusion**: no suitable open-source implementation. Self-contained
wheel with zero dependencies.

## Features

- **Ring buffer storage**: fixed-size circular buffer, oldest records
  overwritten when full
- **Severity levels**: INFO, WARNING, ERROR, FATAL
- **Fatal callback**: application-defined handler invoked on FATAL errors
- **Persistence**: save/load via pluggable storage callbacks (Flash, EEPROM, etc.)
- **Lifetime counters**: total errors and total fatals tracked across clears
- **Context field**: 32-bit arbitrary data per record (register value, address, etc.)
- **Configurable** via compile-time macro (`ERROR_HANDLER_MAX_RECORDS`)
- Pure C99, no heap, no OS dependency, fully reentrant per-context

## API summary

```c
/* Initialize handler. storage may be NULL (no persistence). */
error_handler_err_t error_handler_init(error_handler_t *eh,
                                       const error_handler_storage_t *storage);

/* Report an error. Overwrites oldest record when buffer is full. */
error_handler_err_t error_handler_report(error_handler_t *eh, uint16_t code,
                                          error_severity_t severity,
                                          uint32_t timestamp, uint32_t context);

/* Set callback invoked on FATAL errors. */
void error_handler_set_fatal_callback(error_handler_t *eh,
                                       error_handler_callback_t cb, void *ctx);

/* Return number of records currently stored. */
uint8_t error_handler_get_count(const error_handler_t *eh);

/* Get record by index (0 = oldest). */
error_handler_err_t error_handler_get(const error_handler_t *eh, uint8_t index,
                                       error_record_t *record);

/* Persist current records via storage callbacks. */
error_handler_err_t error_handler_save(error_handler_t *eh);

/* Load records from storage, replacing current buffer. */
error_handler_err_t error_handler_load(error_handler_t *eh);

/* Clear all records and reset counters. */
void error_handler_clear(error_handler_t *eh);

/* Lifetime counters. */
uint32_t error_handler_total_errors(const error_handler_t *eh);
uint32_t error_handler_total_fatals(const error_handler_t *eh);
```

## Configuration macros

| Macro | Default | Description |
|-------|---------|-------------|
| `ERROR_HANDLER_MAX_RECORDS` | 16 | Max error records in ring buffer |

## Origin

Proactive build for embedded fault management. Common pattern across
projects: log errors with severity, trigger recovery on fatal faults,
persist error history across reboots for post-mortem diagnostics.

## Dependencies

None. Pure C99 with `<string.h>`, `<stddef.h>`, `<stdint.h>` only.

## How to test

```bash
cd wheels/modules/error_handler/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Supported platforms

- [x] Linux / Windows host (sim) -- passed 2026-05-01
- [ ] ARM Cortex-M -- not yet verified (no hardware dependency expected)

## Known issues

None.

## Test status

- Unit tests: **PASS** (31 test cases, 0 failures)
- Coverage: init (valid/null/with-storage), report+get, ring buffer
  wraparound, fatal callback (triggered/not-set), severity levels,
  total counters (valid/null), save/load roundtrip, clear (valid/null),
  storage errors (save/load), no-storage errors, NULL params, invalid
  index, empty buffer get, max records fill, multiple errors sequence,
  load-on-init, save empty buffer, save after wraparound, report after
  load, save/load null handler

## Usage example

```c
#include "error_handler.h"

/* Optional: Flash storage callbacks */
static error_handler_err_t flash_save(void *ctx,
                                       const error_record_t *records,
                                       uint8_t count) {
    (void)ctx;
    return flash_write(ERROR_LOG_ADDR, records,
                       count * sizeof(error_record_t))
           ? ERROR_HANDLER_OK : ERROR_HANDLER_ERR_STORAGE;
}

static error_handler_err_t flash_load(void *ctx, error_record_t *records,
                                       uint8_t max, uint8_t *count) {
    (void)ctx;
    return flash_read_error_log(ERROR_LOG_ADDR, records, max, count)
           ? ERROR_HANDLER_OK : ERROR_HANDLER_ERR_STORAGE;
}

static const error_handler_storage_t storage = {
    flash_save, flash_load, NULL
};

/* Fatal handler: reboot or enter safe mode */
static void on_fatal(void *ctx, const error_record_t *record) {
    (void)ctx;
    error_handler_save(&global_eh);  /* persist before reboot */
    system_reboot();
}

static error_handler_t global_eh;

void error_system_init(void) {
    error_handler_init(&global_eh, &storage);
    error_handler_load(&global_eh);  /* restore from last boot */
    error_handler_set_fatal_callback(&global_eh, on_fatal, NULL);
}

/* From anywhere in the application */
void report_sensor_failure(uint16_t sensor_id) {
    error_handler_report(&global_eh, sensor_id,
                          ERROR_SEVERITY_ERROR,
                          get_uptime_ms(), 0);
}
```

## Design tradeoffs

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Buffer | Ring buffer (overwrite oldest) | Bounded memory, no malloc, most recent errors preserved |
| Indexing | 0 = oldest | Natural for post-mortem: read chronologically |
| Storage | Callback injection | Application decides backend (Flash, EEPROM, NVS) |
| Fatal handling | Callback, not longjmp | Predictable control flow, no stack unwinding |
| Counters | Lifetime totals | Survive ring buffer overflow for diagnostics |
| Thread safety | Not thread-safe | Caller serializes (typical: single error context) |
| Save format | Linearized array | Simple, portable, no endianness issues at storage level |

## API review

- Date: 2026-05-01
- Reviewer: @user (pending)
- Key decisions: pending first review

## Before promoting to stable

- [ ] At least 1 real project uses it
- [ ] Evaluate per-module error filtering
- [ ] Evaluate error rate limiting
- [ ] Real hardware persistence test

## Responsible human
@user
