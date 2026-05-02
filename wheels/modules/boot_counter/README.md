# boot_counter -- persistent boot count and reset reason tracker

**Status: candidate**
**Layer: L3 (modules)**

Tracks cumulative boot count, last reset reason code, and runtime uptime.
Storage is abstracted via read/write callbacks so it can wire to kvs, flash,
EEPROM, or any persistent backend. Passive design -- no OS calls, no heap.

## Origin

Proactive build for embedded device lifecycle tracking (boot counting,
reset-reason logging, uptime monitoring).


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None. Storage injected via callbacks.

## API

```c
boot_counter_err_t boot_counter_init(boot_counter_t *ctx,
                                     const boot_counter_storage_t *storage);
boot_counter_err_t boot_counter_load(boot_counter_t *ctx);
boot_counter_err_t boot_counter_increment(boot_counter_t *ctx);
uint32_t           boot_counter_get_count(const boot_counter_t *ctx);
uint32_t           boot_counter_get_reset_reason(const boot_counter_t *ctx);
boot_counter_err_t boot_counter_set_reset_reason(boot_counter_t *ctx,
                                                  uint32_t reason);
uint32_t           boot_counter_get_uptime_seconds(const boot_counter_t *ctx);
void               boot_counter_tick_second(boot_counter_t *ctx);
```

## Configuration (compile-time)

None. All configuration via runtime callbacks.

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/modules/boot_counter/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_boot_counter
```

## Test status

- Unit tests (sim): 16 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Storage via callback injection (not direct kvs/flash dep).
  Record struct serialized as-is (endian-safe on same MCU).

## Responsible human

@user
