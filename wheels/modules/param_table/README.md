# param_table — runtime parameter table with CLI-friendly get/set/list

**Status: candidate**
**Layer: L3 (modules)**

Simple string key-value parameter table for embedded systems. All values are strings, readonly flag protects hardware constants, on_change callback enables reactive parameters. No persistence — application saves if needed.
## Origin

Proactive build for CLI-integrated parameter management across embedded projects.


## Why self-written?

Searched GitHub (2026-05-02). No suitable alternative found. String-keyed KV with readonly flags; too application-specific. Self-written as architecture-specific passive module per platform conventions.

## Dependencies

None.

## API

```c
param_err_t param_table_init(param_table_t *pt);
param_err_t param_table_define(param_table_t *pt, const char *key, const char *default_val, bool readonly);
param_err_t param_table_set(param_table_t *pt, const char *key, const char *value);
param_err_t param_table_get(const param_table_t *pt, const char *key, char *value, uint16_t size);
param_err_t param_table_set_callback(param_table_t *pt, const char *key, param_change_fn fn, void *ctx);
int         param_table_find(const param_table_t *pt, const char *key);
uint8_t     param_table_count(const param_table_t *pt);
int         param_table_list(const param_table_t *pt, char *buf, uint16_t size);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| PARAM_MAX_ENTRIES | 32 | Maximum number of parameters |
| PARAM_VALUE_SIZE  | 32 | Maximum value string length (including NUL) |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/modules/param_table/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_param_table
```

## Test status

- Unit tests (sim): 15 PASS
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: String-only values (no typed variants). Keys are caller-owned pointers (not copied). Readonly flag per entry. Change callback per entry.

## Responsible human

@user
