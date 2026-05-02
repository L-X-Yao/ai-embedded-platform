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
    void *ctx;
} mlx90614_i2c_t;
```

## Configuration (compile-time)

None. I2C address is runtime-configurable via `mlx90614_cfg_t`.

## Supported platforms

- [x] Host (sim) — passed 2026-05-01
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Known issues

- PEC (SMBus CRC-8) is not verified on reads or computed on writes. For
  production use on noisy buses, add PEC checking.
- `set_emissivity` writes to EEPROM — limit write cycles (~100K per Melexis spec).
- Only single-zone (Tobj1) supported. Dual-zone MLX90614xCx Tobj2 not implemented.

## How to test

```bash
cd wheels/drivers/mlx90614/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mlx90614
```

## Test status

- Unit tests (sim): PASS (14 C tests)
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
