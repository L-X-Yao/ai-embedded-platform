# telemetry

**Status: candidate**
**Layer: L3 (modules)**

Periodic telemetry data formatter and publisher. Register named fields,
update values as they change, call `telemetry_publish()` when ready.
Publish formats all fields in the chosen output format and calls a send
callback. The application decides when to publish and where to send
(UART, MQTT, HTTP, file, etc.).

Passive design: no OS, no tasks, no malloc, no globals. The application
drives the timing.

## Why not an open-source library?

Checked common embedded telemetry/data formatting libraries (2026-05-01):

- **cJSON / jsmn** -- JSON-only, typically require malloc or large buffers
- **printf-based ad-hoc** -- no structured field registration, no format switching
- **IoT SDK formatters** -- coupled to specific cloud SDKs (AWS IoT, Azure)

All violate our wheel contract: zero OS dependency, no malloc, no globals,
callback-injected I/O, multi-format support. A passive formatter with
named field registration and pluggable send callback is straightforward
(~170 lines) and matches our platform conventions exactly.

**Conclusion**: no suitable open-source implementation. Self-contained
wheel with zero dependencies.

## Features

- **Three output formats**: JSON, CSV, key-value (selectable at init)
- **Named field registration**: `telemetry_register()` at runtime
- **Update by name or index**: name lookup for clarity, index for speed
- **Send callback**: application injects transport (UART, MQTT, etc.)
- **Updated flag tracking**: fields track whether they changed since last publish
- **Error counting**: tracks send failures separately from send successes
- **Float formatting**: trailing zeros trimmed (25.50 -> "25.5", 60.00 -> "60")
- **Buffer overflow protection**: returns ERR_OVERFLOW if output exceeds buffer
- **Reset**: clears values and counters without losing field registrations
- Pure C99, no heap, no OS dependency, fully reentrant per-context

## API summary

```c
/* Error codes */
typedef enum {
    TELEMETRY_OK            =  0,
    TELEMETRY_ERR_PARAMS    = -1,
    TELEMETRY_ERR_FULL      = -2,
    TELEMETRY_ERR_SEND      = -3,
    TELEMETRY_ERR_OVERFLOW  = -4,
} telemetry_err_t;

/* Output formats */
typedef enum {
    TELEMETRY_FMT_JSON,
    TELEMETRY_FMT_CSV,
    TELEMETRY_FMT_KEY_VALUE,
} telemetry_format_t;

/* Send callback signature */
typedef telemetry_err_t (*telemetry_send_fn)(void *ctx, const char *data,
                                            uint16_t len);

/* Initialize with send callback and format */
telemetry_err_t telemetry_init(telemetry_t *t, telemetry_send_fn send,
                               void *ctx, telemetry_format_t fmt);

/* Register a named field */
telemetry_err_t telemetry_register(telemetry_t *t, const char *name);

/* Update field value by name */
telemetry_err_t telemetry_update(telemetry_t *t, const char *name,
                                 float value);

/* Update field value by index (faster) */
telemetry_err_t telemetry_update_by_index(telemetry_t *t, uint8_t index,
                                          float value);

/* Format and send all fields */
telemetry_err_t telemetry_publish(telemetry_t *t, uint32_t timestamp);

/* Query counters */
uint32_t telemetry_get_send_count(const telemetry_t *t);
uint32_t telemetry_get_error_count(const telemetry_t *t);

/* Reset values and counters (keeps field registrations) */
void telemetry_reset(telemetry_t *t);
```

## Output format examples

Given fields "temp" = 25.5 and "hum" = 60, timestamp = 1000:

| Format | Output |
|--------|--------|
| JSON | `{"ts":1000,"temp":25.5,"hum":60}` |
| CSV | `1000,25.5,60` |
| Key-value | `ts=1000 temp=25.5 hum=60` |

## Configuration macros

| Macro | Default | Description |
|-------|---------|-------------|
| `TELEMETRY_MAX_FIELDS` | 16 | Max registered fields |
| `TELEMETRY_BUF_SIZE` | 512 | Internal format buffer size (bytes) |

## Origin

Proactive build for embedded telemetry systems. Common pattern across
projects: periodic sensor data collection formatted for transmission
over UART/MQTT/HTTP. Decouples data collection (driver layer) from
formatting and transport.

## Dependencies

None. Pure C99 with `<stdbool.h>`, `<stdint.h>`, `<stdio.h>`, `<string.h>` only.

## How to test

```bash
cd wheels/modules/telemetry/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Supported platforms

- [x] Linux / Windows host (sim) -- passed 2026-05-01
- [ ] ARM Cortex-M -- not yet verified (no hardware dependency expected)

## Known issues

None.

## Test status

- Unit tests: **PASS** (27 test cases, 0 failures)
- Coverage: init (valid/null telemetry/null send), register (valid/null/max),
  update by name (valid/unknown/null), update by index (valid/out-of-range),
  publish JSON/CSV/key-value, timestamp, send callback invocation,
  send failure error counting, multiple publishes, reset (valid/null),
  publish null, updated flag tracking, buffer overflow, get count null,
  publish no fields, send context passing, float integer formatting

## Usage example

```c
#include "telemetry.h"

/* UART send callback */
static telemetry_err_t uart_send(void *ctx, const char *data, uint16_t len)
{
    (void)ctx;
    uart_transmit(data, len);
    return TELEMETRY_OK;
}

/* Setup */
static telemetry_t telem;

void telemetry_setup(void)
{
    telemetry_init(&telem, uart_send, NULL, TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "temp");
    telemetry_register(&telem, "hum");
    telemetry_register(&telem, "pressure");
}

/* Called periodically from main loop */
void telemetry_tick(uint32_t now_ms)
{
    telemetry_update(&telem, "temp", read_temperature());
    telemetry_update(&telem, "hum", read_humidity());
    telemetry_update(&telem, "pressure", read_pressure());
    telemetry_publish(&telem, now_ms);
}
```

## Design tradeoffs

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Float format | snprintf %.2f + trim | Portable, readable, small code |
| Field storage | Static array | No malloc, predictable memory footprint |
| Name lookup | Linear scan | Simple, fast enough for <= 16 fields |
| Timestamp | Caller-provided | No OS dependency, application controls time source |
| Format selection | At init | One context = one format; create multiple for multi-format |
| Updated flags | Auto-clear on publish | Track stale vs fresh data per cycle |
| Thread safety | Not thread-safe | Caller serializes (typical: single publish context) |

## API review

- Date: 2026-05-01
- Reviewer: @user (pending)
- Key decisions: pending first review

## Before promoting to stable

- [ ] At least 1 real project uses it
- [ ] Evaluate binary format support (CBOR/MessagePack)
- [ ] Real hardware UART/MQTT test

## Responsible human
@user
