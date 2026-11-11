# config_store

**Status: candidate**
**Layer: L3 (modules)**

Configuration management module with typed entries, defaults, range
validation, and storage callbacks. Passive design: application defines
config entries at startup, gets/sets values with automatic type and
range checking, and decides when to persist via save/load.

## Origin

Common pattern across embedded projects: device configuration with
typed parameters, factory defaults, runtime validation, and persistent
storage. Extracted as a reusable module to avoid per-project reimplementation.

## Why self-written?

Searched GitHub (ADR-010 compliance). No suitable existing implementation found. Self-written for platform convention consistency.


## Dependencies

None. Pure C99 with `<stdbool.h>`, `<stddef.h>`, `<stdint.h>`,
`<string.h>` only.

## API

```c
/* Storage abstraction -- application provides save/load callbacks */
typedef struct {
    config_store_err_t (*save)(void *ctx, const char *key,
                               const uint8_t *data, uint16_t len);
    config_store_err_t (*load)(void *ctx, const char *key,
                               uint8_t *data, uint16_t len);
    void *ctx;
} config_store_storage_t;

/* Initialize store (storage may be NULL for no persistence) */
config_store_err_t config_store_init(config_store_t *cs,
                                     const config_store_storage_t *storage);

/* Define entries with defaults and ranges */
config_store_err_t config_store_define_int(config_store_t *cs, const char *key,
                                           int32_t default_val, int32_t min, int32_t max);
config_store_err_t config_store_define_uint(config_store_t *cs, const char *key,
                                            uint32_t default_val, uint32_t min, uint32_t max);
config_store_err_t config_store_define_float(config_store_t *cs, const char *key,
                                             float default_val, float min, float max);
config_store_err_t config_store_define_bool(config_store_t *cs, const char *key,
                                            bool default_val);
config_store_err_t config_store_define_string(config_store_t *cs, const char *key,
                                              const char *default_val);

/* Set values (validated) */
config_store_err_t config_store_set_int(config_store_t *cs, const char *key, int32_t val);
/* ... similar for uint, float, bool, string */

/* Get values */
config_store_err_t config_store_get_int(const config_store_t *cs, const char *key, int32_t *val);
/* ... similar for uint, float, bool, string */

/* Persistence + reset */
config_store_err_t config_store_save_all(config_store_t *cs);
config_store_err_t config_store_load_all(config_store_t *cs);
config_store_err_t config_store_reset_defaults(config_store_t *cs);
uint8_t config_store_count(const config_store_t *cs);
```

## Configuration (compile-time)

| Macro | Default | Description |
|-------|---------|-------------|
| `CONFIG_STORE_MAX_ENTRIES` | 16 | Max config entries per store, at most 255 |
| `CONFIG_STORE_STRING_SIZE` | 32 | Max string value length (bytes) |

## Supported platforms

- [x] Windows host (sim) -- passed 2026-05-01
- [ ] ARM Cortex-M -- not yet verified (no hardware dependency expected)

## Known issues

None.

## How to test

```bash
cd wheels/modules/config_store/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **PASS** (34 test cases, 0 failures)
- Coverage: init (valid/null), define+get for all 5 types,
  set+get int, range validation (int/uint/float), type mismatch
  (3 cross-type cases), not found (get/set), max entries (ERR_FULL),
  modified flag (int/string toggle), save/load roundtrip (4 types),
  save/load without storage (ERR_STORAGE), save/load failure via mock,
  reset defaults, NULL parameter validation (define/get/set),
  string length overflow (define/set), count null, reset/save/load null,
  load updates modified flag, multiple entries independence

## API convention deviations

None.


## Before promoting to stable

- [ ] At least 1 real project uses it
- [ ] Wire to kvs wheel for flash persistence
- [ ] Evaluate iterator/foreach API
- [ ] Real hardware test

## Design tradeoffs

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Type system | 5 fixed types (i32/u32/f32/bool/str) | Covers embedded config needs without generics |
| Validation | Range check on set | Fail fast, never store invalid values |
| Persistence | Explicit save/load | Passive -- no auto-save overhead, caller controls timing |
| Storage | Callback injection | Decoupled from backend (kvs, flash, EEPROM, file) |
| String size | Compile-time fixed | No malloc, predictable memory |
| Key lookup | Linear scan | Simple, O(n) is fine for <= 16 entries |
| Thread safety | Not thread-safe | Caller serializes (typical: single config context) |

## Usage example

```c
#include "config_store.h"

/* Wire to your KVS / flash backend */
static config_store_err_t my_save(void *ctx, const char *key,
                                  const uint8_t *data, uint16_t len) {
    return kvs_write(key, data, len) == 0
           ? CONFIG_STORE_OK : CONFIG_STORE_ERR_STORAGE;
}

static config_store_err_t my_load(void *ctx, const char *key,
                                  uint8_t *data, uint16_t len) {
    return kvs_read(key, data, len) == 0
           ? CONFIG_STORE_OK : CONFIG_STORE_ERR_STORAGE;
}

static const config_store_storage_t storage = { my_save, my_load, NULL };
static config_store_t cfg;

void config_init(void) {
    config_store_init(&cfg, &storage);

    /* Define entries with defaults and ranges */
    config_store_define_int(&cfg, "volume", 50, 0, 100);
    config_store_define_uint(&cfg, "baud", 115200, 9600, 921600);
    config_store_define_float(&cfg, "gain", 1.0f, 0.1f, 10.0f);
    config_store_define_bool(&cfg, "debug", false);
    config_store_define_string(&cfg, "name", "device-1");

    /* Try loading saved values (falls back to defaults on error) */
    config_store_load_all(&cfg);
}

void config_set_volume(int32_t vol) {
    if (config_store_set_int(&cfg, "volume", vol) == CONFIG_STORE_OK) {
        config_store_save_all(&cfg);  /* persist immediately */
    }
}

void config_factory_reset(void) {
    config_store_reset_defaults(&cfg);
    config_store_save_all(&cfg);
}
```

## Error codes

| Code | Value | Meaning |
|------|-------|---------|
| `CONFIG_STORE_OK` | 0 | Success |
| `CONFIG_STORE_ERR_PARAMS` | -1 | NULL pointer or invalid argument |
| `CONFIG_STORE_ERR_FULL` | -2 | Max entries reached |
| `CONFIG_STORE_ERR_NOT_FOUND` | -3 | Key not defined |
| `CONFIG_STORE_ERR_TYPE` | -4 | Type mismatch (e.g., set_int on float key) |
| `CONFIG_STORE_ERR_RANGE` | -5 | Value outside min/max range |
| `CONFIG_STORE_ERR_STORAGE` | -6 | Storage callback failed or not set |

## Features

- **Typed entries**: int32, uint32, float, bool, string
- **Defaults**: each entry has a compile-time default value
- **Range validation**: numeric types checked on set (min/max)
- **Type safety**: set/get with wrong type returns ERR_TYPE
- **Modified flag**: tracks whether current differs from default
- **Storage callbacks**: save/load via injected function pointers
  (wire to kvs, flash, EEPROM, or any backend)
- **Passive**: no auto-save, caller decides when to persist
- **Reset to defaults**: one call restores all entries
- **Configurable** via compile-time macros (max entries, string size)
- Pure C99, no heap, no OS dependency, fully reentrant per-context

## Why not an open-source library?

Checked common embedded config/settings libraries (2026-05-01):

- **EasyConfig** -- requires filesystem, POSIX I/O
- **libconfuse** -- C99 but malloc-heavy, parser-based (reads text files)
- **iniparser** -- file-based, uses malloc, POSIX-coupled
- **cJSON-based schemes** -- pull in JSON parser, overkill for typed KV

All violate our wheel contract: zero OS dependency, no malloc, no
globals. A typed key-value store with compile-time max entries, range
validation, and callback-injected persistence is straightforward
(~220 lines) and matches our platform conventions exactly.

**Conclusion**: no suitable open-source implementation. Self-contained
wheel with zero dependencies.

## API review

- Date: 2026-05-01
- Reviewer: @user (pending)
- Key decisions: pending first review

## Responsible human

@L-X-Yao
