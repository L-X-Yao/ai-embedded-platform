# ds3231 — DS3231 high-precision I2C RTC driver

**Status: candidate**
**Layer: L1 (drivers)**

DS3231 real-time clock driver. 24-hour mode only, BCD conversion handled
internally, temperature readout at 0.25C resolution. Fixed I2C address 0x68.
HAL-injected I2C, zero allocation, pure C99.

## Origin

Proactive build for embedded projects needing battery-backed RTC with
high accuracy (+-2 ppm, +-3.5 min/year).

## Dependencies

None (I2C HAL injected via callbacks).

## API

```c
typedef enum {
    DS3231_OK         =  0,
    DS3231_ERR_I2C    = -1,
    DS3231_ERR_PARAMS = -2,
} ds3231_err_t;

typedef struct {
    uint8_t year;     /* 0-99 (2000-2099) */
    uint8_t month;    /* 1-12 */
    uint8_t day;      /* 1-31 */
    uint8_t weekday;  /* 1-7 (1=Monday) */
    uint8_t hour;     /* 0-23 */
    uint8_t minute;   /* 0-59 */
    uint8_t second;   /* 0-59 */
} ds3231_time_t;

ds3231_err_t ds3231_init(ds3231_t *dev, const ds3231_i2c_t *i2c);
ds3231_err_t ds3231_get_time(const ds3231_t *dev, ds3231_time_t *time);
ds3231_err_t ds3231_set_time(const ds3231_t *dev, const ds3231_time_t *time);
ds3231_err_t ds3231_get_temperature(const ds3231_t *dev, float *temp_c);
```

## Configuration (compile-time)

No compile-time configuration needed. DS3231 has a fixed I2C address (0x68).

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/ds3231/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ds3231
```

## Test status

- Unit tests (sim): 28 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: 24-hour mode only (no AM/PM ambiguity), BCD conversion
  internal, temperature read-only (updated every 64s by DS3231 hardware),
  init forces 24h mode if chip is in 12h mode.

## Responsible human

@user
