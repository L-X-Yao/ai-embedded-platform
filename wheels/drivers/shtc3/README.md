# shtc3 -- Sensirion SHTC3 humidity and temperature sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the Sensirion SHTC3 low-power humidity and
temperature sensor. I2C interface injected via callbacks. Uses 16-bit
command words with CRC-8 protected responses. Supports wakeup/sleep
power management for ultra-low-power operation.

## Origin

Proactive build. SHTC3 is a popular low-power humidity/temperature sensor
for wearables, IoT, and HVAC applications.

## Why self-written?

No existing open-source component for SHTC3. Self-written for zero-dependency I2C HAL
injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

I2C HAL (injected via `shtc3_i2c_t` callbacks). No other dependencies.

## API

```c
shtc3_err_t shtc3_init(shtc3_t *dev, const shtc3_i2c_t *i2c);
shtc3_err_t shtc3_read(shtc3_t *dev, shtc3_data_t *out);
shtc3_err_t shtc3_sleep(shtc3_t *dev);
shtc3_err_t shtc3_wakeup(shtc3_t *dev);
shtc3_err_t shtc3_reset(shtc3_t *dev);
uint8_t     shtc3_crc8(const uint8_t *data, uint16_t len);
```

Output: `shtc3_data_t { float temperature_c; float humidity_pct; }`

## Usage

```c
shtc3_t sensor;
shtc3_init(&sensor, &i2c_hal);

shtc3_data_t data;
shtc3_read(&sensor, &data);
// data.temperature_c, data.humidity_pct (0-100%)

shtc3_sleep(&sensor);   /* enter low-power mode */
```

## Configuration (compile-time)

None. Fixed I2C address 0x70.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against Sensirion SHTC3 Datasheet, Version 0.3, October 2017, watermarked PRELIMINARY: its maximum timings are tbd. Not run on hardware.

- **Matched:** I2C address, command codes, ID mask and value, read layout, the wakeup, measure, read, sleep sequence, CRC-8. The 1 ms and 13 ms waits exceed the typical 230 us and 10.8 ms, but with the maximums tbd the margin cannot be shown.
- **Fixed:** Both conversions divided by 65535 where the manual divides by 2^16, so readings ran up to 0.0027 C and 0.0015 %RH high. `shtc3_init` refuses a HAL without `delay_ms`, because `shtc3_read` then sent its read header during the measurement and failed every time.
- **Still open:** After an I2C or CRC error `shtc3_read` returns without sending Sleep, so the sensor stays idle at 42 uA instead of 0.3 uA until the next read. No low-power measurement mode, and no RH-first or clock-stretching commands. The model starts asleep, where the sensor starts idle, accepts every command while asleep and has no measurement period, so tests cannot see a missing Wakeup or a missing wait.
- **Suspected:** `shtc3_reset` sends no Wakeup, so it may fail on a sleeping sensor, which is the state after init and read. `shtc3_wakeup` and `shtc3_reset` return at once, so the next command may arrive before the sensor is idle.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/shtc3/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_shtc3
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
