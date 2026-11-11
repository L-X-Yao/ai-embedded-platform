# mlx90614 — Melexis MLX90614 non-contact IR temperature sensor

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Melexis MLX90614 non-contact infrared temperature
sensor. Uses SMBus protocol via command-based I2C (write and write_read, not register-based).
Default address 0x5A. Supports object temperature, ambient temperature, and
emissivity read/write.
## Origin

Proactive build. MLX90614 is the most common non-contact IR temperature sensor
in embedded applications (fever detection, industrial monitoring, HVAC, food safety).


## Why self-written?

Searched GitHub (2026-05-02). Found libdriver/mlx90614 (MIT). SMBus, straightforward. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

I2C HAL (injected via `mlx90614_i2c_t` callbacks — command-based write and write_read). No other dependencies.

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
    bool (*read)(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);  /* unused */
    void (*delay_ms)(void *ctx, uint32_t ms);  /* required for EEPROM writes */
    void *ctx;
    /* Required: writes tx, sends a repeated START with no STOP, reads rx */
    bool (*write_read)(void *ctx, uint8_t dev_addr,
                       const uint8_t *tx, uint16_t tx_len,
                       uint8_t *rx, uint16_t rx_len);
} mlx90614_i2c_t;
```

A HAL without `write_read` fails `mlx90614_init` with `MLX90614_ERR_PARAM`.

## Configuration (compile-time)

None. I2C address is runtime-configurable via `mlx90614_cfg_t`.

## Supported platforms

- [x] Host (sim) — passed 2026-05-01
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against Melexis DOC#3901090614, Datasheet Rev 021 (Doc server Rev 012), 02-Jun-2026. Not run on hardware.

- **Matched:** Default address, RAM and EEPROM commands, byte order, the error flag, temperature and emissivity formulas, erase before write with 10 ms after each.
- **Fixed:** `smbus_write_word` always sent PEC 0x00, so the sensor would NACK every EEPROM write and `mlx90614_set_emissivity` could not change the cell. It now sends CRC-8 (polynomial 0x07) over address, command and data: 0x28 for the erase frame. The same CRC gives all four PECs in Figures 6 to 9. `mlx90614_set_emissivity` refuses a HAL without `delay_ms`, where it used to skip both waits and return OK. `mlx90614_init` and every read used separate `write` and `read` calls, which put a STOP where the manual requires a repeated START. They now go through `write_read`, as in Figure 6. The mock and the model served a read after a separate write, which hid this, and both now refuse it. Both also took a non-zero write over an unerased cell. They now spoil or refuse it, and the model stays busy for 5 ms after each write. The `write_read` fix has not run on hardware.
- **Still open:** The read PEC is not checked, so a corrupted word passes. No Tobj2, read flags, sleep, other EEPROM cells or the SMBus request that leaves PWM mode.
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

- Unit tests (sim): PASS (22 C tests)
- Model tests (sim): PASS (15 C tests)
- HIL: N/A

## API convention deviations

Uses command-based I2C HAL (write, write_read) instead of register-based
(write_reg/read_regs) because MLX90614 uses SMBus protocol: a read sends the command
byte, then a repeated START, then reads the word and its PEC.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Command-based I2C HAL (`write_read` added 2026-11-06: a read is one transaction with a repeated START, not two); context struct holds I2C HAL + config — fully reentrant for multi-sensor setups; emissivity range validated to 0.1-1.0; EEPROM erase-then-write sequence per datasheet; error flag bit (0x8000) checked on temperature reads.

## Responsible human
@L-X-Yao
