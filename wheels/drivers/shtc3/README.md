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

## Why not VSF native?

No VSF component for SHTC3. Self-written for zero-dependency I2C HAL
injection and platform conventions.

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

## Known issues

- Must call `shtc3_wakeup()` before `shtc3_read()` if sensor is sleeping.
  `shtc3_read()` does not auto-wake.

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

## Responsible human

@user
