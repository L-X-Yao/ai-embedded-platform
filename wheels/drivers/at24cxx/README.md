# at24cxx --- I2C EEPROM driver for AT24Cxx series

**Status: candidate**
**Layer: L1 (drivers)**

I2C EEPROM driver supporting AT24C01 through AT24C256.  Handles both
8-bit address chips (AT24C01-AT24C16) and 16-bit address chips
(AT24C32-AT24C256).  Page-boundary splitting is done automatically
in `at24cxx_write()`, with caller-provided delay for write cycles.

## Origin

Proactive build for common I2C EEPROM storage use case (parameter
storage, calibration data, configuration persistence).


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None (I2C HAL injected via callbacks).

## API

```c
at24cxx_err_t at24cxx_init(at24cxx_t *dev, const at24cxx_i2c_t *i2c,
                           uint8_t dev_addr, at24cxx_model_t model);

at24cxx_err_t at24cxx_read(const at24cxx_t *dev, uint16_t addr,
                           uint8_t *buf, uint16_t len);

at24cxx_err_t at24cxx_write(const at24cxx_t *dev, uint16_t addr,
                            const uint8_t *data, uint16_t len);

at24cxx_err_t at24cxx_write_byte(const at24cxx_t *dev, uint16_t addr,
                                 uint8_t val);
```

### Supported models

| Model    | Size     | Page size | Address mode |
|----------|----------|-----------|--------------|
| AT24C01  | 128 B    | 8 B       | 8-bit        |
| AT24C02  | 256 B    | 8 B       | 8-bit        |
| AT24C04  | 512 B    | 16 B      | 8-bit + addr bits in dev_addr |
| AT24C08  | 1 KB     | 16 B      | 8-bit + addr bits in dev_addr |
| AT24C16  | 2 KB     | 16 B      | 8-bit + addr bits in dev_addr |
| AT24C32  | 4 KB     | 32 B      | 16-bit       |
| AT24C64  | 8 KB     | 32 B      | 16-bit       |
| AT24C128 | 16 KB    | 64 B      | 16-bit       |
| AT24C256 | 32 KB    | 64 B      | 16-bit       |

### I2C HAL callbacks

For 8-bit address chips (AT24C01-AT24C16), provide `write_reg` and
`read_regs`.  For 16-bit address chips (AT24C32-AT24C256), provide
`write_regs` and `read_regs16`.  The `delay_ms` callback is used
for EEPROM write-cycle delays (~5 ms per page write).

## Configuration (compile-time)

| Define               | Default | Purpose                          |
|----------------------|---------|----------------------------------|
| AT24CXX_WRITE_CYCLE_MS | 5    | Write-cycle delay in ms          |

## Supported platforms

- [x] Host (sim) --- 2026-05-01
- [ ] Real hardware target --- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/at24cxx/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_at24cxx
```

## Test status

- Unit tests (sim): 19 pass / 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Separate 8-bit/16-bit HAL callbacks rather than
  a unified interface, to avoid forcing 8-bit-only users to implement
  16-bit callbacks.  Page-boundary splitting handled internally.

## Responsible human

@user
