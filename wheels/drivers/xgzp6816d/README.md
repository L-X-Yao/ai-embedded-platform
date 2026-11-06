# XGZP6816D — Barometric Pressure Sensor Driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the CFSensor XGZP6816D barometric pressure sensor
(30-110 kPa, 24-bit pressure + 16-bit temperature, I2C interface @ 0x78).

## Origin

Reimplemented for the needs of an internal project, 2026-04-27.

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. Obscure CFSensor chip; no driver found anywhere. Self-written with HAL injection per platform conventions.

## Dependencies

None. Pure C99 with injected I2C HAL callbacks.

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

static bool plat_write(void *ctx, uint8_t addr, const uint8_t *buf, uint16_t n)
    { (void)ctx; (void)addr; (void)buf; (void)n; return true; }
static bool plat_read(void *ctx, uint8_t addr, uint8_t *buf, uint16_t n)
    { (void)ctx; (void)addr; (void)buf; (void)n; return true; }

static void example(void) {
    xgzp6816d_i2c_t bus = { .write = plat_write, .read = plat_read, .ctx = NULL };

    xgzp6816d_probe(&bus);

    xgzp6816d_trigger(&bus);
    /* sleep_ms(XGZP6816D_CONV_MS); -- caller provides delay */
    xgzp6816d_data_t d;
    xgzp6816d_read(&bus, &d);
    /* d.pressure_pa, d.temperature_c */
}
```

## Supported platforms

- [x] Host C99 (mock I2C) — 21 tests, 39 assertions, 2026-04-29
- [x] GD32F405 + XGZP6816D @ soft I2C ~50 kHz (PB6/PB7) — HIL verified 2026-04-27

## Datasheet check

Checked on 2026-11-06 against the Sencoch GZP6816D product specification V1.4, 2022.03.16. That is a sibling part, not the CFSensor XGZP6816D this driver names, so every difference below is only suspected. This check was not run on hardware. Nothing was changed: the owner decided this driver is checked but not fixed, because no board is available to retest it.

- **Matched:** On the sibling part: address 0x78, command 0xAC, the 6-byte read frame, the pressure formula for 30 to 110 kPa, the temperature formula.
- **Suspected:** Finding X1, the most serious: since 2026-05-06 the flybox adapter `baro_i2c_write` goes through `soft_i2c_write`, which fails on a NACKed data byte. The manual shows the chip NACKing 0xAC, and commit an April fix recorded the real part doing so, with every read failing. So `xgzp6816d_trigger` may fail every cycle and the barometer may no longer read on the board. The HIL run listed above (2026-04-27) predates that commit, and the git log records no later hardware run. The fault would be in the adapter, which does what Known issues below warns against. Other suspicions: `XGZP6816D_CONV_MS` is 25 ms where the sibling manual gives 203 ms at its default oversampling and 19 ms at 8x, so reads may return old data. `xgzp6816d_read` ignores the status byte, so it also returns data while bit 5 says busy. A raw pressure of 0 is clamped to a plausible 30 kPa, while the manual's example code treats it as no reading. The model and mock return status 0x00 although bit 2 is fixed at 1, and the model never shows a conversion in progress.

## Known issues

- I2C HAL shape is generic write/read (not register-paired) because XGZP6816D uses bare command bytes + streamed data reads; adapters from register-paired I2C (like flybox's software I2C) must not check ACK on data bytes after the command byte (chip NACKs it — see flybox d4345b7 fix)
- Conversion time is ~20 ms typical (25 ms with margin); polling faster returns stale data

## How to test

```bash
cd wheels/drivers/xgzp6816d/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Host mock tests: PASS (21 tests, 39 assertions — probe, trigger, read, pressure low/mid/high rails + clamping, standard atmosphere interpolation, quarter/three-quarter pressure range, temperature endpoints + 0°C/55°C/100°C intermediate, I2C error surfacing, NULL safety, re-probe idempotency)
- HIL: PASS on GD32F405 + XGZP6816D (software bit-bang I2C, PB6/PB7), 2026-04-27

## API convention deviations

I2C HAL uses generic `write(addr, buf, len)` / `read(addr, buf, len)` instead of the standard `write_reg(addr, reg, val)` / `read_regs(addr, reg, buf, len)` pattern used by other driver wheels. Reason: XGZP6816D protocol is not register-addressed.

## Upgrade to stable

- [ ] 2nd project consumer (currently only flybox)
- [ ] Performance reference (conversion time measurement, I2C transaction sizes)
- [ ] Change history

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

## Features

- Probe (bus connectivity check)
- Split trigger/read cycle — caller picks own delay primitive (vTaskDelay, k_msleep, busy loop), wheel has zero RTOS dependency
- 24-bit pressure with dual-rail clamp (30-110 kPa) — prevents garbage values from saturated readings
- 16-bit temperature (-40 to 150 C)
- Generic I2C transaction HAL (write/read, not register-paired — XGZP6816D uses command + stream protocol)

## API review

- Date: 2026-04-27
- Reviewer: @user
- Key decisions: split trigger/read (no RTOS dep); generic I2C HAL instead of register-paired (matches hardware protocol); dual-rail pressure clamp

## Responsible human

@user
