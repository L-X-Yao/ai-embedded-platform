# data_logger

**Status: candidate**
**Layer: L3 (modules)**

Record-based data logger for embedded systems. Passive design:
application calls begin/add/commit to build CSV-formatted records,
logger formats fields and calls a storage callback. The application
decides the backend (flash, SD card, UART, etc.).

This is the second L3 wheel. It composes concepts from L0 (byte_buffer
pattern: staged buffer writes) but is self-contained with no `#include`
dependencies on other wheels.

## Origin

Proactive build. Structured data logging is needed in nearly every
sensor/telemetry project (drones, weather stations, industrial monitors).

## Why not an open-source library?

Checked common embedded data logging libraries (2026-05-01):

- **fatfs + csv** -- requires filesystem, malloc, OS integration
- **ulog / EasyLogger** -- log-oriented (text messages), not structured records
- **embedded-log** -- Arduino C++, not portable C99
- **SD card libraries** -- hardware-coupled, not storage-agnostic

All violate our wheel contract: zero OS dependency, no malloc, no globals,
callback-injected storage. A passive, record-based logger with typed fields
and CSV output is straightforward to implement (~160 lines) and matches our
platform conventions exactly.

**Conclusion**: no suitable open-source implementation. Self-contained
wheel with zero dependencies.

## Dependencies

None. Pure C99 with `<string.h>`, `<stddef.h>`, `<stdint.h>`, `<stdio.h>` only.

## API summary

```c
/* Storage abstraction -- application provides write/flush callbacks */
typedef struct {
    data_logger_err_t (*write)(void *ctx, const uint8_t *data, uint16_t len);
    data_logger_err_t (*flush)(void *ctx);
    void *ctx;
} data_logger_storage_t;

/* Field schema -- define record structure at init time */
typedef struct {
    const char             *name;
    data_logger_field_type_t type;  /* INT32, UINT32, FLOAT, STRING */
} data_logger_field_t;

/* Initialize with storage backend and field schema */
data_logger_err_t data_logger_init(data_logger_t *dl,
                                   const data_logger_storage_t *storage,
                                   const data_logger_field_t *fields,
                                   uint8_t field_count);

/* Build a record: begin -> add fields -> commit */
data_logger_err_t data_logger_begin(data_logger_t *dl, uint32_t timestamp);
data_logger_err_t data_logger_add_int(data_logger_t *dl, int32_t value);
data_logger_err_t data_logger_add_uint(data_logger_t *dl, uint32_t value);
data_logger_err_t data_logger_add_float(data_logger_t *dl, float value);
data_logger_err_t data_logger_add_string(data_logger_t *dl, const char *value);
data_logger_err_t data_logger_commit(data_logger_t *dl);

/* Flush storage / query state */
data_logger_err_t data_logger_flush(data_logger_t *dl);
uint32_t data_logger_get_count(const data_logger_t *dl);
```

## Configuration macros

| Macro | Default | Description |
|-------|---------|-------------|
| `DATA_LOGGER_BUF_SIZE` | 256 | Max record buffer size (bytes) |
| `DATA_LOGGER_MAX_FIELDS` | 8 | Max fields per record |

## Supported platforms

- [x] Linux / Windows host (sim) -- passed 2026-05-01
- [ ] ARM Cortex-M -- not yet verified (no hardware dependency expected)

## Known issues

None.

## How to test

```bash
cd wheels/modules/data_logger/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **PASS** (30 test cases, 0 failures)
- Coverage: init (valid/null/null-storage/null-write/null-fields/zero-fields),
  single-field record, multi-field record (int+uint+float+string),
  timestamp verification, record count tracking, buffer overflow,
  flush (with/without callback), sequential records, float formatting
  (positive/negative/zero), string field, field count overflow,
  wrong field type, storage write failure, begin-null, commit-without-begin,
  add-without-begin, get-count-null, flush-null, add-string-null,
  max-fields-exceeded, zero-fields-timestamp-only, large timestamp

## API review

- Date: 2026-05-01
- Reviewer: @user (pending)
- Key decisions: pending first review

## Responsible human

@user

## Features

- **Passive design**: application drives begin/add/commit cycle
- **Typed fields**: INT32, UINT32, FLOAT, STRING with schema validation
- **CSV output**: `timestamp,field1,field2,...\n` format
- **Storage abstraction**: write + flush callbacks, application chooses backend
- **Field count validation**: rejects extra fields or wrong types
- **Buffer overflow protection**: returns ERR_OVERFLOW, record discarded
- **Record counting**: track total committed records
- **Configurable** via compile-time macros (buffer size, max fields)
- Pure C99, no heap, no OS dependency, fully reentrant per-context

## Usage example

```c
#include "data_logger.h"

/* Storage callback: write to flash via W25QXX */
static data_logger_err_t flash_write(void *ctx, const uint8_t *data,
                                     uint16_t len) {
    flash_ctx_t *fc = (flash_ctx_t *)ctx;
    w25qxx_write(&fc->dev, fc->addr, data, len);
    fc->addr += len;
    return DATA_LOGGER_OK;
}

static data_logger_err_t flash_flush(void *ctx) {
    (void)ctx;
    return DATA_LOGGER_OK;  /* flash writes are synchronous */
}

/* Setup */
static data_logger_t logger;
static flash_ctx_t flash_ctx;
static const data_logger_storage_t flash_storage = {
    flash_write, flash_flush, &flash_ctx
};

static const data_logger_field_t sensor_fields[] = {
    { "temperature", DATA_LOGGER_FIELD_INT32  },
    { "pressure",    DATA_LOGGER_FIELD_UINT32 },
    { "altitude",    DATA_LOGGER_FIELD_FLOAT  },
    { "status",      DATA_LOGGER_FIELD_STRING },
};

void logger_init(void) {
    data_logger_init(&logger, &flash_storage, sensor_fields, 4);
}

/* Called periodically from main loop */
void logger_sample(uint32_t tick_ms) {
    data_logger_begin(&logger, tick_ms);
    data_logger_add_int(&logger, read_temperature());
    data_logger_add_uint(&logger, read_pressure());
    data_logger_add_float(&logger, calc_altitude());
    data_logger_add_string(&logger, system_ok() ? "OK" : "ERR");
    data_logger_commit(&logger);
}
```

## Design tradeoffs

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Format | CSV text | Human-readable, easy to parse, universally supported |
| Float precision | 2 decimal places | Sufficient for most sensor data; configurable if needed |
| Field validation | Type-checked at add | Catches schema mismatches early, prevents corrupt records |
| Buffer | Per-record staging | Complete record or nothing; no partial writes |
| Storage | Callback injection | Application decides backend; flash, SD, UART all work |
| Thread safety | Not thread-safe | Caller serializes (typical: single logging context) |
| Timestamp | Caller-provided | No OS clock dependency; application owns time source |
| Max fields | Compile-time limit | No malloc, predictable memory footprint |

## Before promoting to stable

- [ ] At least 1 real project uses it
- [ ] Evaluate binary record format option
- [ ] Evaluate header row output (field names)
- [ ] Real hardware flash/SD test
