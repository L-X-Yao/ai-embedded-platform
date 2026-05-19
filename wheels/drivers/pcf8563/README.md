# pcf8563 -- NXP PCF8563 I2C real-time clock driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the NXP PCF8563 I2C real-time clock.
I2C interface injected via callbacks. BCD-encoded time/date registers
in 24-hour mode. Supports alarm, countdown timer, and CLKOUT pin
configuration.

## Origin

Proactive build. PCF8563 is a widely used low-power RTC for embedded
systems needing battery-backed timekeeping (IoT, data loggers, alarms).

## Why self-written?

No existing open-source component for PCF8563. Self-written for zero-dependency I2C HAL
injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

I2C HAL (injected via `pcf8563_i2c_t` callbacks). No other dependencies.

## API

```c
pcf8563_err_t pcf8563_init(pcf8563_t *dev, const pcf8563_i2c_t *i2c);
pcf8563_err_t pcf8563_read_time(const pcf8563_t *dev, pcf8563_time_t *time);
pcf8563_err_t pcf8563_write_time(const pcf8563_t *dev,
                                 const pcf8563_time_t *time);
pcf8563_err_t pcf8563_read_alarm(const pcf8563_t *dev, pcf8563_alarm_t *alarm);
pcf8563_err_t pcf8563_write_alarm(const pcf8563_t *dev,
                                  const pcf8563_alarm_t *alarm);
pcf8563_err_t pcf8563_enable_timer(const pcf8563_t *dev,
                                   pcf8563_timer_clk_t clk, uint8_t countdown);
pcf8563_err_t pcf8563_disable_timer(const pcf8563_t *dev);
pcf8563_err_t pcf8563_set_clkout(const pcf8563_t *dev, bool enable,
                                 pcf8563_clkout_t freq);
```

## Usage

```c
pcf8563_t rtc;
pcf8563_init(&rtc, &i2c_hal);

pcf8563_time_t now;
pcf8563_read_time(&rtc, &now);
// now.year (0-99), now.month, now.day, now.hour, now.minute, now.second

pcf8563_time_t set = { .year = 26, .month = 5, .day = 18,
                       .hour = 14, .minute = 30, .second = 0 };
pcf8563_write_time(&rtc, &set);
```

## Configuration (compile-time)

None. Fixed I2C address 0x51.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- Year range limited to 2000-2099 (2-digit BCD).
- Interrupt pin configuration not implemented.

## How to test

```bash
cd wheels/drivers/pcf8563/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_pcf8563
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
