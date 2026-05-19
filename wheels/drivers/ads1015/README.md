# ads1015 — TI ADS1015 12-bit 4-channel I2C ADC

**Status: candidate**
**Layer: L1 (drivers)**

12-bit delta-sigma ADC with I2C interface. Supports 4 single-ended or
2 differential input channels, programmable gain amplifier (PGA) from
0.256V to 6.144V, and configurable data rates (128-3300 SPS). Same
register layout as ADS1115 but 12-bit resolution (left-justified in
16-bit conversion register).

## Origin

Proactive build for general-purpose analog sensing. Sibling of the
existing ads1115 wheel (shared register layout, 12-bit vs 16-bit).

## Why self-written?

Searched GitHub (2026-05-02). Found ElliottWaterman/ADS1015 (unclear license). HAL injection pattern; no license for vendoring. Self-written for unified HAL injection pattern, error codes, and test framework; cross-validated against reference.

## Dependencies

None (I2C HAL injected at init).

## API

```c
ads1015_err_t ads1015_init(ads1015_t *dev, const ads1015_i2c_t *i2c,
                           const ads1015_cfg_t *cfg);
ads1015_err_t ads1015_read_single(ads1015_t *dev, ads1015_mux_t channel,
                                  float *voltage_mv);
ads1015_err_t ads1015_read_raw(ads1015_t *dev, ads1015_mux_t channel,
                               int16_t *raw);
ads1015_err_t ads1015_set_pga(ads1015_t *dev, ads1015_pga_t pga);
ads1015_err_t ads1015_set_rate(ads1015_t *dev, ads1015_rate_t rate);
```

## Configuration (compile-time)

No compile-time defines required. All configuration via `ads1015_cfg_t`.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/ads1015/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ads1015
```

## Test status

- Unit tests (sim): 18 tests, all passing
- HIL: N/A

## API convention deviations

None.

## I2C addresses

| ADDR pin | Address |
|----------|---------|
| GND      | 0x48    |
| VDD      | 0x49    |
| SDA      | 0x4A    |
| SCL      | 0x4B    |

## API review

- **Date**: 2026-05-01
- **Key decisions**: Channel selection per-call (not stored in cfg) to match
  typical multi-channel scan patterns. 12-bit raw value is sign-extended
  (not the raw left-justified 16-bit register content).

## Responsible human

@user
