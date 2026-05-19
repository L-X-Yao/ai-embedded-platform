# kvs — Key-Value Storage (FlashDB)

**Status: candidate**

Production-grade KV database over flash. Wear leveling, power-fail safe,
near-zero RAM. Vendored from armink's FlashDB v2.1.0.

## Origin

Vendor: see VENDOR.md.

## Why self-written?

Not self-written. Vendored from armink's FlashDB v2.1.0 (BSD-3). See "Why this upstream?" below.

## Dependencies

- `fal` (L0, concrete, ADR-009) — flash partition management

## API

FlashDB's upstream API is used directly. Consumers `#include <flashdb.h>`.

```c
/* Project provides fdb_cfg.h + fal_cfg.h on the include path.
 * See wheels/primitives/kvs/test/fdb_cfg.h for a minimal example. */
#include "flashdb.h"

static void kvs_example(void) {
    struct fdb_kvdb db;
    struct fdb_default_kv_node defaults[] = {
        { "boot_count", "0",   0 },
        { "device_id",  "A01", 0 },
    };
    struct fdb_default_kv default_kv = { .kvs = defaults, .num = 2 };

    uint32_t sec_size = 4096;
    fdb_kvdb_control(&db, FDB_KVDB_CTRL_SET_SEC_SIZE, &sec_size);
    fdb_kvdb_init(&db, "app", "kvdb", &default_kv, NULL);

    /* String API */
    fdb_kv_set(&db, "wifi_ssid", "MyNetwork");
    char *ssid = fdb_kv_get(&db, "wifi_ssid");
    (void)ssid;

    /* Binary blob API */
    struct fdb_blob blob;
    uint32_t val = 42;
    fdb_kv_set_blob(&db, "counter", fdb_blob_make(&blob, &val, sizeof(val)));

    uint32_t read_val;
    fdb_kv_get_blob(&db, "counter", fdb_blob_make(&blob, &read_val, sizeof(read_val)));

    /* Delete */
    fdb_kv_del(&db, "temp_key");
}
```

## Configuration (compile-time, in fdb_cfg.h)

| Define | Default | Purpose |
|--------|---------|---------|
| `FDB_USING_FAL_MODE` | (must define) | Use FAL for flash access |
| `FDB_USING_KVDB` | (must define) | Enable KV database |
| `FDB_USING_TSDB` | (optional) | Enable time-series database |
| `FDB_KV_NAME_MAX` | 64 | Max key name length |
| `FDB_WRITE_GRAN` | 1 | Flash write granularity (bits) |
| `FDB_KV_CACHE_TABLE_SIZE` | 64 | KV lookup cache entries |

## Supported platforms

- [x] Host (sim, RAM-backed flash mock) — passed 2026-04-28
- [ ] GD32F405 (flybox) — FlashDB used in project, wheel not separately HIL tested

## Known issues

- Upstream warnings on 64-bit host: `cast from pointer to integer of different size` in offset macros. Harmless on 32-bit MCU targets.
- No thread safety by default — set lock/unlock via `fdb_kvdb_control()` if multi-threaded.
- TSDB included but optional — disable by omitting `FDB_USING_TSDB` in fdb_cfg.h.

## How to test

```bash
cd wheels/primitives/kvs/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_kvs
```

42 assertions across 11 test cases: FAL init, KVDB init, default KV values,
string set/get, overwrite, delete, binary blob set/get, persistence across
re-init, nonexistent key, 20-key batch, KV iterator.

## Test status

- Unit tests (sim): PASS (11 C tests, 42 assertions)
- HIL: N/A

## API convention deviations

- **No `kvs_` wrapper**: Uses upstream FlashDB API directly (`fdb_kv_set` not `kvs_set`). Wrapping a production-grade API adds confusion for anyone familiar with FlashDB.

## Porting (per-project fdb_cfg.h)

Each project must provide `fdb_cfg.h` on the include path:

```c
#define FDB_USING_FAL_MODE
#define FDB_USING_KVDB
/* #define FDB_USING_TSDB */  /* enable if needed */
#define FDB_PRINT(...)  printf(__VA_ARGS__)
```

Also requires `fal_cfg.h` for flash partition configuration (see fal wheel).

## Why this upstream?

FlashDB is the most widely used embedded KV library in Chinese embedded
(~2.5k GitHub stars, actively maintained). Features: KV + time-series DB,
wear leveling, power-fail safe, ~6KB Flash / near-zero RAM.

Evaluated alternatives: TFDB (too minimal, one sector per variable), KVED
(numeric keys only), Zephyr NVS (not extractable), ESP NVS (ESP32 only).
See ADR-010.

## API review

- **Date**: 2026-04-28
- **Key decisions**: Vendor FlashDB v2.1.0 (user's production copy) per ADR-010. Use upstream API directly (no wrapper). Concrete dependency on fal wheel per ADR-009. FAL mode only (no file mode). Both KVDB and TSDB vendored, TSDB optional via compile flag.

## Responsible human

@user
