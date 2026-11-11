# ds3231 — DS3231 high-precision I2C RTC driver

**Status: candidate**
**Layer: L1 (drivers)**

DS3231 real-time clock driver. 24-hour mode only, BCD conversion handled
internally, temperature readout at 0.25C resolution. Fixed I2C address 0x68.
HAL-injected I2C, zero allocation, pure C99.

## Origin

Proactive build for embedded projects needing battery-backed RTC with
high accuracy (+-2 ppm, +-3.5 min/year).


## Why self-written?

Searched GitHub (2026-05-02). Found ebrezadev/DS3231 (MIT). Straightforward I2C RTC. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

None (I2C HAL injected via callbacks).

## API

```c
typedef enum {
    DS3231_OK         =  0,
    DS3231_ERR_I2C    = -1,
    DS3231_ERR_PARAM  = -2,
    DS3231_ERR_TIME_INVALID = -3,   /* the oscillator stopped: set the time */
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

## Datasheet check

Checked on 2026-11-06 against Maxim 19-5170 Rev 10, as a recheck of the May review. Not run on hardware.

- **Matched:** Address, register map, BCD fields, 24-hour handling, the temperature format.
- **Fixed:** `ds3231_set_time` accepted days the month does not have, such as 31 February. The model resets to 01/01/00 with OSF and EN32kHz set and Control at 0x1C, and its status flags only clear. OSF was never read, so the time after a battery loss came back as valid. `ds3231_get_time` now returns `DS3231_ERR_TIME_INVALID`, and `ds3231_set_time` clears the flag once all of the time is written.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/ds3231/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ds3231
```

## Test status

- Unit tests (sim): 66 tests (34 main + 21 hw_model + 8 null + 3 prop), all passing
- HIL: N/A

## Migration

From the fixes of 2026-11.

| Change | What a caller has to do | If they do nothing |
|--------|-------------------------|--------------------|
| `ds3231_get_time` returns the new `DS3231_ERR_TIME_INVALID` while OSF is set, which includes a new chip and a flat battery. `time` is still filled in | Set the time with `ds3231_set_time`. On this code do not use the time as real | A caller that treats any error as fatal stops there until the time is set. One that ignores the return gets the same bad time as before |
| `ds3231_set_time` clears OSF after writing the time, keeping EN32kHz, A1F and A2F | Nothing | Nothing changes for the caller |
| `ds3231_get_time` makes one more I2C read, of the status register, and `ds3231_set_time` one or two more | Nothing, unless a mock has fewer than 16 registers | A HAL that refuses register 0x0F gives `DS3231_ERR_I2C` |

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: 24-hour mode only (no AM/PM ambiguity), BCD conversion
  internal, temperature read-only (updated every 64s by DS3231 hardware),
  init forces 24h mode if chip is in 12h mode.

## Responsible human

@L-X-Yao
