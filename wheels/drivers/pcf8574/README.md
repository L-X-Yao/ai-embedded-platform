# pcf8574 — I2C 8-bit GPIO expander driver

**Status: candidate**
**Layer: L1 (drivers)**

NXP PCF8574 / PCF8574A I2C 8-bit quasi-bidirectional I/O expander.
Command-based I2C (no registers) — read or write a single byte for all 8 pins.
Pin-level operations use read-modify-write on a cached output state.
## Origin

Proactive build for I2C GPIO expansion use cases (LCD backpacks, keypad matrices,
relay boards).


## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. 1-byte I2C device, trivially simple. Self-written with HAL injection per platform conventions.

## Dependencies

None (I2C transport injected via callbacks).

## API

```c
typedef enum {
    PCF8574_OK         =  0,
    PCF8574_ERR_I2C    = -1,
    PCF8574_ERR_PARAMS = -2,
} pcf8574_err_t;

typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr,
                  const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr,
                 uint8_t *data, uint16_t len);
    void *ctx;
} pcf8574_i2c_t;

typedef struct {
    const pcf8574_i2c_t *i2c;
    uint8_t dev_addr;
    uint8_t output_state;
} pcf8574_t;

pcf8574_err_t pcf8574_init(pcf8574_t *dev, const pcf8574_i2c_t *i2c,
                           uint8_t dev_addr);
pcf8574_err_t pcf8574_write(pcf8574_t *dev, uint8_t value);
pcf8574_err_t pcf8574_read(pcf8574_t *dev, uint8_t *value);
pcf8574_err_t pcf8574_write_pin(pcf8574_t *dev, uint8_t pin, bool level);
pcf8574_err_t pcf8574_read_pin(pcf8574_t *dev, uint8_t pin, bool *level);
pcf8574_err_t pcf8574_toggle_pin(pcf8574_t *dev, uint8_t pin);
```

### Notes

- **Address**: 0x20-0x27 (PCF8574) or 0x38-0x3F (PCF8574A), set by A0-A2 pins.
- **I2C interface**: command-based (no register address), unlike most I2C sensors.
  Uses `write`/`read` callbacks, not the register-based `write_reg`/`read_regs`.
- **Pin write**: read-modify-write on cached `output_state` — does not re-read
  the bus before writing.
- **Init**: writes 0xFF (all high / input mode) to verify I2C link.

## Configuration (compile-time)

None. All configuration is runtime via `pcf8574_init()`.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/pcf8574/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_pcf8574
```

## Test status

- Unit tests (sim): 14 pass, 0 fail
- HIL: N/A

## API convention deviations

I2C transport uses command-based `write`/`read` (raw byte transfer) instead of
the standard register-based `write_reg`/`read_regs` because the PCF8574 has no
register map.

## API review

- **Date**: 2026-05-01
- **Key decisions**: command-based I2C interface (no registers); cached
  output_state for read-modify-write; no address validation (user controls
  A0-A2 hardware pins)

## Responsible human

@user
