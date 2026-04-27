# xgzp6816d

**STATUS: draft**
**Layer: L1 (drivers)**

Platform-agnostic driver for the CFSensor XGZP6816D barometric pressure sensor
(30–110 kPa, 24-bit pressure + 16-bit temperature, I²C interface @ 0x78).

## Why not VSF native?

Searched `vsf/source/{component,service,hal/}` for `baro`, `pressure`,
`barometer`, `xgzp`, `bmp280`, `bmp388`, `spl06`, `ms5611`, `pressure_sens`
on 2026-04-27 — zero hits. VSF has no barometric / pressure sensor driver.

## API

```c
xgzp6816d_err_t xgzp6816d_probe  (const xgzp6816d_i2c_t *i2c);
xgzp6816d_err_t xgzp6816d_trigger(const xgzp6816d_i2c_t *i2c);
xgzp6816d_err_t xgzp6816d_read   (const xgzp6816d_i2c_t *i2c,
                                  xgzp6816d_data_t *out);
```

Caller provides I²C HAL via `xgzp6816d_i2c_t` callbacks (generic write/read
transactions, not the register-paired protocol that BMP280-family parts use).

The driver deliberately splits `trigger` and `read` so the caller picks its
own delay primitive (vTaskDelay, k_msleep, busy loop, …) for the
~25 ms conversion window. Wheel itself has zero RTOS dependency.

## Usage

```c
#include "xgzp6816d.h"

/* Adapt your platform's I2C primitives to the wheel's transaction shape */
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

24-bit raw counts span 30–110 kPa linearly:

| p_raw | p_kPa |
|-------|-------|
| 1 677 722 | 30 (low rail) |
| 8 388 608 | 70 (mid scale) |
| 15 099 494 | 110 (high rail) |

Raw counts outside [1 677 722, 15 099 494] are clamped — the sensor is out of
spec there. The pre-wheel flybox driver only clamped the low side, so a
saturated reading would surface as ~119 kPa garbage; this wheel adds the
high-side clamp.

Temperature: `(t_raw / 65536) × 190 − 40`, range −40 to ~150 °C.

## Origin

Extracted from `projects/flybox/src/drivers/baro/baro_driver.c` 2026-04-27.
HIL verified on GD32F405 + XGZP6816D over software bit-bang I²C @ ~50 kHz
(via flybox's PB6/PB7).

## Test

```bash
cd test && mkdir build && cd build
cmake -GNinja .. && ninja && ./test_xgzp6816d
```

13 host tests covering probe, trigger, read, pressure low/mid/high rails
+ clamping, temperature endpoints, and I²C error surfacing.
