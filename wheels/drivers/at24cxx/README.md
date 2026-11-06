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

Searched GitHub (2026-05-02). Referenced libdriver/at24cxx (MIT) for register sequences and compensation algorithms. Covers full family; our impl covers our needs. Self-written for platform convention consistency; cross-validated against reference.

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
for EEPROM write-cycle delays (`AT24CXX_WRITE_CYCLE_MS` per page write).

## Configuration (compile-time)

| Define               | Default | Purpose                          |
|----------------------|---------|----------------------------------|
| AT24CXX_WRITE_CYCLE_MS | 20   | Write-cycle delay in ms          |

20 ms is the longest tWR in the AT24C128/256 manual. That manual gives 10 ms at
2.5 V and above, and 5 ms for parts with process letter B, so a build for a known
part can define it lower to write faster.

## Supported platforms

- [x] Host (sim) --- 2026-05-01
- [ ] Real hardware target --- not yet verified

## Datasheet check

Checked on 2026-11-06 against AT24C128/256, 0670T-SEEPR-3/07. That manual covers only those two parts, so the sizes, page sizes and block bits of AT24C01 to AT24C64 were not checked. Not run on hardware.

- **Matched:** Default address, AT24C128 and AT24C256 sizes, the 64-byte page, two word-address bytes, page splitting in `at24cxx_write`, reads across pages, the end-of-memory check.
- **Fixed:** `AT24CXX_WRITE_CYCLE_MS` defaulted to 5 ms for every part, but the manual gives 5 ms only to parts with process letter B and up to 20 ms to the others. On those the next page got no acknowledge and a multi-page write stopped half done with `AT24CXX_ERR_I2C`. The default is now 20 ms, so a page write takes four times as long. The 16-bit mock took transfers during a write cycle and now refuses them. None of this has run on hardware.
- **Still open:** The model and the mocks run a page write past the page end, read 0xFF past the last byte instead of rolling over, and ignore the device address, so a driver that stopped splitting pages or used the wrong address would pass. The AT24C02 model and the 8-bit mock have no write cycle, and the 16-bit parts have no hw_model. There is no acknowledge polling, so every page pays the fixed wait. There is no memory reset after an interrupted transfer, and writes inhibited by WP at VCC go unreported.
- **Suspected:** Without `write_regs8`, the 8-bit fallback writes byte by byte with one wait per page, so the second byte would reach a busy chip and fail or be lost. A NULL `delay_ms` skips the wait altogether, so the next transfer gets no acknowledge. `at24cxx_write_byte` is tagged ISR-safe although it blocks for the write cycle.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/at24cxx/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_at24cxx
```

## Test status

- Unit tests (sim): 20 pass / 0 fail
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
