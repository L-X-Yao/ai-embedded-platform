# XGZP6816D — Barometric Pressure Sensor Driver

**STATUS: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the CFSensor XGZP6816D barometric pressure sensor
(30-110 kPa, 24-bit pressure + 16-bit temperature, I2C interface @ 0x78).

## Why not VSF native?

Searched `vsf/source/{component,service,hal/}` for `baro`, `pressure`,
`barometer`, `xgzp`, `bmp280`, `bmp388`, `spl06`, `ms5611`, `pressure_sens`
on 2026-04-27 — zero hits. VSF has no barometric / pressure sensor driver.

## Features

- Probe (bus connectivity check)
- Split trigger/read cycle — caller picks own delay primitive (vTaskDelay, k_msleep, busy loop), wheel has zero RTOS dependency
- 24-bit pressure with dual-rail clamp (30-110 kPa) — prevents garbage values from saturated readings
- 16-bit temperature (-40 to 150 C)
- Generic I2C transaction HAL (write/read, not register-paired — XGZP6816D uses command + stream protocol)

## API

```c
xgzp6816d_err_t xgzp6816d_probe  (const xgzp6816d_i2c_t *i2c);
xgzp6816d_err_t xgzp6816d_trigger(const xgzp6816d_i2c_t *i2c);
xgzp6816d_err_t xgzp6816d_read   (const xgzp6816d_i2c_t *i2c,
                                  xgzp6816d_data_t *out);
```

Caller provides I2C HAL via `xgzp6816d_i2c_t` callbacks (generic write/read
transactions, not the register-paired protocol that BMP280-family parts use).

## Usage

```c
#include "xgzp6816d.h"

static bool plat_write(void *ctx, uint8_t addr, const uint8_t *buf, uint16_t n) { ... }
static bool plat_read (void *ctx, uint8_t addr, uint8_t *buf, uint16_t n)       { ... }

xgzp6816d_i2c_t bus = { .write = plat_write, .read = plat_read, .ctx = NULL };

xgzp6816d_probe(&bus);

while (1) {
    xgzp6816d_trigger(&bus);
    sleep_ms(XGZP6816D_CONV_MS);
    xgzp6816d_data_t d;
    xgzp6816d_read(&bus, &d);
    /* d.pressure_pa, d.temperature_c */
}
```

## Pressure conversion

24-bit raw counts span 30-110 kPa linearly:

| p_raw | p_kPa |
|-------|-------|
| 1 677 722 | 30 (low rail) |
| 8 388 608 | 70 (mid scale) |
| 15 099 494 | 110 (high rail) |

Raw counts outside [1 677 722, 15 099 494] are clamped — the sensor is out of
spec there. The pre-wheel flybox driver only clamped the low side, so a
saturated reading would surface as ~119 kPa garbage; this wheel adds the
high-side clamp.

Temperature: `(t_raw / 65536) * 190 - 40`, range -40 to ~150 C.

## How to test

```bash
cd wheels/drivers/xgzp6816d/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Supported platforms

- [x] Host C99 (mock I2C) — 21 tests, 39 assertions, 2026-04-29
- [x] GD32F405 + XGZP6816D @ soft I2C ~50 kHz (PB6/PB7) — HIL verified 2026-04-27

## Dependencies

None. Pure C99 with injected I2C HAL callbacks.

## Known issues

- I2C HAL shape is generic write/read (not register-paired) because XGZP6816D uses bare command bytes + streamed data reads; adapters from register-paired I2C (like flybox's software I2C) must not check ACK on data bytes after the command byte (chip NACKs it — see flybox d4345b7 fix)
- Conversion time is ~20 ms typical (25 ms with margin); polling faster returns stale data

## Test status

- Host mock tests: PASS (21 tests, 39 assertions — probe, trigger, read, pressure low/mid/high rails + clamping, standard atmosphere interpolation, quarter/three-quarter pressure range, temperature endpoints + 0°C/55°C/100°C intermediate, I2C error surfacing, NULL safety, re-probe idempotency)
- HIL: PASS on GD32F405 + XGZP6816D (software bit-bang I2C, PB6/PB7), 2026-04-27

## API convention deviations

I2C HAL uses generic `write(addr, buf, len)` / `read(addr, buf, len)` instead of the standard `write_reg(addr, reg, val)` / `read_regs(addr, reg, buf, len)` pattern used by other driver wheels. Reason: XGZP6816D protocol is not register-addressed.

## Responsible human

@user

## Origin

Extracted from `projects/flybox/src/drivers/baro/baro_driver.c` 2026-04-27.

## API review

- Date: 2026-04-27
- Reviewer: @user
- Key decisions: split trigger/read (no RTOS dep); generic I2C HAL instead of register-paired (matches hardware protocol); dual-rail pressure clamp

## Upgrade to stable

- [ ] 2nd project consumer (currently only flybox)
- [ ] Performance reference (conversion time measurement, I2C transaction sizes)
- [ ] Change history
