# bq27441 — TI BQ27441-G1 I2C smart fuel gauge driver

**Status: candidate**
**Layer: L1 (drivers)**

I2C driver for the TI BQ27441-G1 single-cell Li-Ion fuel gauge. Reads voltage, current,
state of charge, temperature, remaining/full-charge capacity, and average power. Supports
config mode for setting design capacity, plus seal/unseal for device security.

## Origin

Proactive build for battery-powered embedded projects (ADR-011 ecosystem-driven).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-06):

- SparkFun_BQ27441_Arduino_Library: Arduino/C++ only, heavy class hierarchy, not portable C99.
- No suitable zero-dependency C99 implementation found matching wheel contract (no heap, no OS, HAL injected).

Reference: TI BQ27441-G1 Technical Reference Manual (SLUUAC9A).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
/* Error codes */
typedef enum {
    BQ27441_OK, BQ27441_ERR_I2C, BQ27441_ERR_ID,
    BQ27441_ERR_PARAM, BQ27441_ERR_TIMEOUT
} bq27441_err_t;

/* Data struct */
typedef struct {
    uint16_t voltage_mv;
    int16_t  current_ma;       /* negative = discharge */
    uint16_t soc_pct;          /* 0-100% */
    int16_t  temperature_01c;  /* 0.1 degC units */
    uint16_t remaining_mah;
    uint16_t full_charge_mah;
    int16_t  avg_power_mw;     /* negative = discharge */
} bq27441_data_t;

/* Core */
bq27441_err_t bq27441_init(bq27441_t *dev, const bq27441_i2c_t *i2c);
bq27441_err_t bq27441_read(bq27441_t *dev, bq27441_data_t *out);

/* Individual reads */
bq27441_err_t bq27441_read_voltage(bq27441_t *dev, uint16_t *voltage_mv);
bq27441_err_t bq27441_read_soc(bq27441_t *dev, uint16_t *soc_pct);
bq27441_err_t bq27441_read_current(bq27441_t *dev, int16_t *current_ma);
bq27441_err_t bq27441_read_temperature(bq27441_t *dev, int16_t *temperature_01c);

/* Config mode */
bq27441_err_t bq27441_enter_config_mode(bq27441_t *dev);
bq27441_err_t bq27441_exit_config_mode(bq27441_t *dev);
bq27441_err_t bq27441_set_design_capacity(bq27441_t *dev, uint16_t mah);

/* Security */
bq27441_err_t bq27441_unseal(bq27441_t *dev);
bq27441_err_t bq27441_seal(bq27441_t *dev);
```

## Configuration (compile-time)

No compile-time defines required. All configuration is runtime via API calls.

## Supported platforms

- [x] Host (sim) — 2026-05-06
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/bq27441/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bq27441
```

## Test status

- Unit tests (sim): 42 tests, all passing
- Null parameter tests: 17 auto-generated, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Integer-only API (no float); temperature in 0.1 degC units; signed types for current/power/temperature to represent charge/discharge direction.

## Responsible human

@user
