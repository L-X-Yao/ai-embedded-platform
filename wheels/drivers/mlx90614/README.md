# mlx90614 — Melexis MLX90614 non-contact IR temperature sensor

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Melexis MLX90614 non-contact infrared temperature
sensor. Uses SMBus protocol via command-based I2C (write+read, not register-based).
Default address 0x5A. Supports object temperature, ambient temperature, and
emissivity read/write.
## Origin

Proactive build. MLX90614 is the most common non-contact IR temperature sensor
in embedded applications (fever detection, industrial monitoring, HVAC, food safety).


## Why self-written?

Searched GitHub (2026-05-02). Found libdriver/mlx90614 (MIT). SMBus, straightforward. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

I2C HAL (injected via `mlx90614_i2c_t` callbacks — command-based write/read). No other dependencies.

## API

```c
mlx90614_err_t mlx90614_init(mlx90614_t *dev, const mlx90614_i2c_t *i2c,
                             const mlx90614_cfg_t *cfg);
mlx90614_err_t mlx90614_read_object_temp(mlx90614_t *dev, float *temp_c);
mlx90614_err_t mlx90614_read_ambient_temp(mlx90614_t *dev, float *temp_c);
mlx90614_err_t mlx90614_read_emissivity(mlx90614_t *dev, float *emissivity);
mlx90614_err_t mlx90614_set_emissivity(mlx90614_t *dev, float emissivity);
```

I2C HAL uses command-based (SMBus) callbacks, not register-based:
```c
typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);  /* required for EEPROM writes */
    void *ctx;
} mlx90614_i2c_t;
```

## Configuration (compile-time)

None. I2C address is runtime-configurable via `mlx90614_cfg_t`.

## Supported platforms

- [x] Host (sim) — passed 2026-05-01
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against Melexis DOC#3901090614, Datasheet Rev 021 (Doc server Rev 012), 02-Jun-2026. Not run on hardware.

- **Matched:** Default address, RAM and EEPROM commands, byte order, the error flag, temperature and emissivity formulas, erase before write with 10 ms after each.
- **Fixed:** `smbus_write_word` always sent PEC 0x00, so the sensor would NACK every EEPROM write and `mlx90614_set_emissivity` could not change the cell. It now sends CRC-8 (polynomial 0x07) over address, command and data: 0x28 for the erase frame. The same CRC gives all four PECs in Figures 6 to 9. `mlx90614_set_emissivity` refuses a HAL without `delay_ms`, where it used to skip both waits and return OK.
- **Still open:** Every read uses separate `write` and `read` calls, so a HAL that ends each call with STOP puts a STOP where the manual requires a repeated START. The manual does not say what the sensor does then, so expect `mlx90614_init` and every read to fail or return bad data on hardware until the HAL gains a combined write-read call. The model and the mock serve a read after a separate write, which hides this, and the model takes a non-zero write to a cell that was not erased, so the erase step is untested. The read PEC is not checked, so a corrupted word passes. No Tobj2, read flags, sleep, other EEPROM cells or the SMBus request that leaves PWM mode.
- **Suspected:** `mlx90614_init` does not wait the 250 ms after POR, so a first reading may be undefined. Changing the emissivity cell alone may upset calibration, and the Melexis application note on it is not in the manual set. `mlx90614_set_emissivity` skips the recommended read-back and power cycle.

## Known issues

- PEC (SMBus CRC-8) is sent on writes, which the sensor requires, but it is
  not verified on reads.
- `set_emissivity` writes to EEPROM — limit write cycles (~100K per Melexis spec).
- Only single-zone (Tobj1) supported. Dual-zone MLX90614xCx Tobj2 not implemented.

## How to test

```bash
cd wheels/drivers/mlx90614/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mlx90614
```

## Test status

- Unit tests (sim): PASS (19 C tests)
- HIL: N/A

## API convention deviations

Uses command-based I2C HAL (write/read) instead of register-based (write_reg/read_regs)
because MLX90614 uses SMBus protocol where command byte is sent as a write, then
data is read separately.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Command-based I2C HAL (SMBus protocol requires write cmd + read data as separate transactions); context struct holds I2C HAL + config — fully reentrant for multi-sensor setups; emissivity range validated to 0.1-1.0; EEPROM erase-then-write sequence per datasheet; error flag bit (0x8000) checked on temperature reads.

## Responsible human
@user
