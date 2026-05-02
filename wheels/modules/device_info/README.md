# device_info

**Status: candidate**
**Layer: L3 (modules)**

Device identity and runtime information module. Stores firmware/hardware
version, serial number, build date, device name, uptime, boot count, and
reset reason. Passive design: application populates fields at startup,
calls `device_info_tick()` once per second for uptime tracking, and uses
`device_info_format()` to serialize all info as key=value text for CLI
display or telemetry payloads.

No persistence -- caller saves/loads boot_count via kvs or similar.

## Why not an open-source library?

No existing embedded "device info" library found that meets our wheel
contract (no OS, no malloc, no globals, no build-system coupling). This
is a thin data container with string setters + snprintf formatter -- too
simple and platform-specific to warrant a dependency. ~110 lines of C.

**Conclusion**: self-contained wheel with zero dependencies.

## Features

- **Passive design**: no tasks, no timers, no malloc
- **String fields**: firmware version, hardware version, serial number, build date, device name
- **Runtime counters**: uptime (tick-based), boot count, reset reason
- **Overflow protection**: long strings truncated with ERR_OVERFLOW return
- **Format output**: `device_info_format()` produces key=value text for CLI/telemetry
- **NULL safety**: all API functions guard against NULL pointers
- **Configurable**: `DEVICE_INFO_STR_SIZE` compile-time macro (default 32)
- Pure C99, no heap, no OS dependency, fully reentrant per-context

## API summary

```c
/* Zero-initialize all fields */
device_info_err_t device_info_init(device_info_t *info);

/* String field setters (truncate on overflow) */
device_info_err_t device_info_set_firmware_version(device_info_t *info, const char *version);
device_info_err_t device_info_set_hardware_version(device_info_t *info, const char *version);
device_info_err_t device_info_set_serial_number(device_info_t *info, const char *serial);
device_info_err_t device_info_set_build_date(device_info_t *info, const char *date);
device_info_err_t device_info_set_device_name(device_info_t *info, const char *name);

/* Counter setters */
void device_info_set_boot_count(device_info_t *info, uint32_t count);
void device_info_set_reset_reason(device_info_t *info, uint32_t reason);

/* Call once per second to increment uptime */
void device_info_tick(device_info_t *info);

/* Get current uptime in seconds */
uint32_t device_info_get_uptime(const device_info_t *info);

/* Format all info as key=value lines into buffer */
int device_info_format(const device_info_t *info, char *buf, uint16_t buf_size);
```

## Configuration macros

| Macro | Default | Description |
|-------|---------|-------------|
| `DEVICE_INFO_STR_SIZE` | 32 | Max length of each string field (including NUL) |

## Origin

Proactive build for embedded device telemetry and debug. Common pattern
across projects: report firmware version, serial number, uptime, and
boot count via CLI or telemetry channel. Extracted as reusable module.

## Dependencies

None. Pure C99 with `<string.h>`, `<stddef.h>`, `<stdint.h>`, `<stdio.h>` only.

## How to test

```bash
cd wheels/modules/device_info/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Supported platforms

- [x] Linux / Windows host (sim) -- passed 2026-05-01
- [ ] ARM Cortex-M -- not yet verified (no hardware dependency expected)

## Known issues

None.

## Test status

- Unit tests: **PASS** (25 test cases, 0 failures)
- Coverage: init (valid/null), set all five string fields, set all fields
  together, uptime after ticks, multiple ticks (100), string overflow
  truncation, NULL parameter validation (all setters), boot count, reset
  reason, format output (all keys present), format buffer too small,
  format NULL params, empty fields format, set same field twice, tick/
  get_uptime/set_boot_count/set_reset_reason NULL safety, exact-fit
  string, format byte count matches strlen

## Usage example

```c
#include "device_info.h"

static device_info_t dev_info;

void app_init(void) {
    device_info_init(&dev_info);
    device_info_set_firmware_version(&dev_info, "1.2.3");
    device_info_set_hardware_version(&dev_info, "rev-B");
    device_info_set_serial_number(&dev_info, "SN-001234");
    device_info_set_build_date(&dev_info, __DATE__);
    device_info_set_device_name(&dev_info, "FlyBox");
    device_info_set_boot_count(&dev_info, kvs_read_u32("boot_count") + 1);
    device_info_set_reset_reason(&dev_info, RCC_GetResetReason());
}

/* Call from 1-second timer */
void app_1s_tick(void) {
    device_info_tick(&dev_info);
}

/* CLI or telemetry handler */
void cmd_info(void) {
    char buf[256];
    device_info_format(&dev_info, buf, sizeof(buf));
    uart_puts(buf);
}
```

## Design tradeoffs

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Persistence | None (caller handles) | kvs/flash strategy varies per project |
| Uptime | Tick-based counter | No OS timer dependency; caller calls from 1s tick |
| Format | snprintf key=value | Human-readable, easy to parse, no JSON overhead |
| String size | Fixed compile-time | No malloc, predictable memory footprint |
| Overflow | Truncate + return ERR | Caller can detect and handle or ignore |
| Thread safety | Not thread-safe | Caller serializes (typical: single init + periodic tick) |

## API review

- Date: 2026-05-01
- Reviewer: @user (pending)
- Key decisions: pending first review

## Before promoting to stable

- [ ] At least 1 real project uses it
- [ ] Evaluate adding custom field support
- [ ] Real hardware test

## Responsible human
@user
