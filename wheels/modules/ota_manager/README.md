# ota_manager

**Status: candidate**
**Layer: L3 (modules)**

Firmware update orchestration module. State machine drives the full OTA
flow: version check, chunked download, flash write, verify, activate.
All I/O through callbacks -- the module knows nothing about HTTP, MQTT,
flash details, or slot layout. Application drives the pace by calling
`download_step()` in a loop.

Pairs naturally with `ota_ab` (L2) for A/B slot management, but does
not depend on it. The application wires them together via callbacks.

## Origin

Proactive build. OTA capability is required in virtually all connected
embedded products (IoT, fleet management, consumer devices).

## Why self-written?

Searched GitHub (ADR-010 compliance). No suitable existing implementation found. Self-written for platform convention consistency.


## Dependencies

None. Pure C99 with `<stdint.h>`, `<stdbool.h>`, `<string.h>` only.

## API

```c
/* I/O callbacks -- application provides all transport/storage logic */
typedef struct {
    ota_mgr_err_t (*check_version)(void *ctx, const char *current_ver,
                                   bool *update_available, uint32_t *new_size);
    ota_mgr_err_t (*download_chunk)(void *ctx, uint32_t offset,
                                    uint8_t *buf, uint16_t buf_size,
                                    uint16_t *bytes_read);
    ota_mgr_err_t (*write_chunk)(void *ctx, uint32_t offset,
                                 const uint8_t *data, uint16_t len);
    ota_mgr_err_t (*verify)(void *ctx, uint32_t total_size);
    ota_mgr_err_t (*activate)(void *ctx);
    void *ctx;
} ota_mgr_ops_t;

/* Lifecycle */
ota_mgr_err_t ota_manager_init(ota_manager_t *mgr,
                               const ota_mgr_ops_t *ops,
                               const char *current_version);

/* Flow: check -> download_step (loop) -> verify -> activate */
ota_mgr_err_t ota_manager_check(ota_manager_t *mgr);
ota_mgr_err_t ota_manager_download_step(ota_manager_t *mgr,
                                        uint8_t *chunk_buf,
                                        uint16_t buf_size);
ota_mgr_err_t ota_manager_verify(ota_manager_t *mgr);
ota_mgr_err_t ota_manager_activate(ota_manager_t *mgr);

/* Query / control */
ota_mgr_state_t ota_manager_get_state(const ota_manager_t *mgr);
uint32_t ota_manager_get_progress(const ota_manager_t *mgr);
void ota_manager_reset(ota_manager_t *mgr);
```

## Configuration (compile-time)

| Macro | Default | Description |
|-------|---------|-------------|
| `OTA_MGR_CHUNK_SIZE` | 256 | Default chunk size hint (bytes) |
| `OTA_MGR_VERSION_MAX` | 32 | Max version string length |

## Supported platforms

- [x] Linux / Windows host (sim) -- passed 2026-05-01
- [ ] ARM Cortex-M -- not yet verified (no hardware dependency expected)

## Known issues

None.

## How to test

```bash
cd wheels/modules/ota_manager/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **PASS** (36 test cases, 0 failures)
- libFuzzer: plays a server and a flash slot that misbehave, with the slot exactly the announced size
- Coverage: init (valid/null-mgr/null-ops/null-version/missing-callback),
  check (update-available/no-update/callback-error), download (full/progress/
  offset-advances/failure/write-failure), verify (success/failure), activate,
  state guards (download-without-check/verify-before-complete/
  activate-before-verify/check-while-downloading), reset (from-error/
  clears-progress/then-restart), null safety (state/progress/reset/check/
  download-buf/download-size), full end-to-end flow, version truncation,
  chunk-smaller-than-buf

## API convention deviations

None.


## Before promoting to stable

- [ ] At least 1 real project uses it
- [ ] Wire up with ota_ab for full A/B OTA flow
- [ ] Real hardware test (flash write + reboot + rollback)
- [ ] Evaluate resume-on-reboot (persist download offset)
- [ ] Evaluate retry policy (auto-retry on transient errors)

## Design tradeoffs

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Architecture | Passive state machine | Application drives pace; no tasks, no timers |
| Download | One chunk per call | Non-blocking; works in main loop or RTOS task |
| I/O | All via callbacks | Transport/flash/slot agnostic |
| Progress | Integer 0-100 | Simple, overflow-safe for large firmware |
| Version | String (max 32) | Semantic versioning or any scheme; no parsing |
| State guards | ERR_STATE on wrong order | Prevents misuse; caller can check state first |
| Reset | Unconditional to IDLE | Simple error recovery; no partial cleanup needed |
| Thread safety | Not thread-safe | Caller serializes (typical: single OTA context) |

## Usage example

```c
#include "ota_manager.h"
#include "ota_ab.h"        /* optional: A/B slot manager */
#include "mqtt_packet.h"   /* optional: transport */

/* Callbacks wired to your transport + flash */
static ota_mgr_err_t my_check(void *ctx, const char *ver,
                               bool *avail, uint32_t *size) {
    /* query server for new firmware */
    return mqtt_check_ota(ver, avail, size);
}

static ota_mgr_err_t my_download(void *ctx, uint32_t offset,
                                  uint8_t *buf, uint16_t buf_size,
                                  uint16_t *bytes_read) {
    return http_get_chunk(offset, buf, buf_size, bytes_read);
}

static ota_mgr_err_t my_write(void *ctx, uint32_t offset,
                               const uint8_t *data, uint16_t len) {
    ota_ab_t *ab = (ota_ab_t *)ctx;
    uint32_t addr = ota_ab_slot_addr(ab, ota_ab_target_slot(ab));
    return flash_write(addr + offset, data, len);
}

static ota_mgr_err_t my_verify(void *ctx, uint32_t total_size) {
    /* CRC32 check of written flash region */
    return verify_flash_crc(total_size);
}

static ota_mgr_err_t my_activate(void *ctx) {
    ota_ab_t *ab = (ota_ab_t *)ctx;
    return ota_ab_mark_pending(ab, ...);
}

/* Setup */
static ota_manager_t ota;
static const ota_mgr_ops_t ops = {
    my_check, my_download, my_write, my_verify, my_activate, &ab_ctx
};

void ota_init(void) {
    ota_manager_init(&ota, &ops, "1.2.3");
}

/* Called from main loop -- non-blocking, one step at a time */
void ota_poll(void) {
    static uint8_t chunk[OTA_MGR_CHUNK_SIZE];
    ota_mgr_state_t st = ota_manager_get_state(&ota);

    switch (st) {
    case OTA_MGR_STATE_IDLE:
        if (time_to_check()) ota_manager_check(&ota);
        break;
    case OTA_MGR_STATE_DOWNLOADING:
        ota_manager_download_step(&ota, chunk, sizeof(chunk));
        break;
    case OTA_MGR_STATE_VERIFYING:
        ota_manager_verify(&ota);
        break;
    case OTA_MGR_STATE_READY:
        ota_manager_activate(&ota);
        /* reboot after activate */
        break;
    case OTA_MGR_STATE_ERROR:
        ota_manager_reset(&ota);
        break;
    default:
        break;
    }
}
```

## Features

- **State machine**: idle -> checking -> downloading -> verifying -> ready
- **Chunked download**: application calls `download_step()` in a loop
- **Progress tracking**: 0-100% via `get_progress()`
- **All I/O via callbacks**: check_version, download_chunk, write_chunk, verify, activate
- **State guards**: operations rejected if called in wrong state
- **Reset**: return to idle from any state (error recovery, abort)
- **Configurable** chunk size via compile-time macro
- Pure C99, no heap, no OS dependency, fully reentrant per-context

## Why not an open-source library?

Checked common embedded OTA libraries (2026-05-01):

- **MCUboot** -- full bootloader, requires OS integration (Zephyr/mynewt)
- **esp_ota_ops** -- ESP-IDF specific, not portable
- **mbed-cloud-client** -- Mbed OS specific, heavy dependencies
- **hawkBit DDI** -- server-side orchestration, client is OS-coupled
- **SWUpdate** -- Linux userspace, not bare-metal

All violate our wheel contract: zero OS dependency, no malloc, no globals,
callback-injected I/O. A passive orchestration state machine (~150 lines)
matches our platform conventions exactly.

**Conclusion**: no suitable open-source implementation. Self-contained
wheel with zero dependencies.

## API review

- Date: 2026-05-01
- Reviewer: @user (pending)
- Key decisions: pending first review

## Responsible human

@L-X-Yao
