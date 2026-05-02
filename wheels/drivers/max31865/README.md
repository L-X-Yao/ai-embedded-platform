# max31865 — SPI RTD-to-digital converter for PT100/PT1000

**Status: candidate**
**Layer: L1 (drivers)**

MAX31865 RTD-to-digital converter driver. Reads 15-bit RTD ratio via SPI
registers, converts to temperature using Callendar-Van Dusen linearization
(IEC 60751). Supports PT100 and PT1000 probes in 2/3/4-wire configurations.

## Origin

Proactive build for industrial temperature sensing (RTD applications).

## Why not VSF native?

No MAX31865 driver exists in VSF `source/service/`, `source/utilities/`, or
`source/component/`. Verified via grep 2026-05-01.

## Dependencies

None (SPI HAL injected via callbacks).

## API

```c
max31865_err_t max31865_init(max31865_t *dev, const max31865_spi_t *spi,
                             const max31865_cfg_t *cfg);
max31865_err_t max31865_read_temperature(max31865_t *dev, float *temp_c);
max31865_err_t max31865_read_rtd_raw(max31865_t *dev, uint16_t *rtd_raw);
max31865_err_t max31865_read_fault(max31865_t *dev, uint8_t *fault);
max31865_err_t max31865_clear_fault(max31865_t *dev);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| (none) | — | All config via `max31865_cfg_t` at runtime |

## Runtime configuration

| Field | Example | Purpose |
|-------|---------|---------|
| `r_ref` | 430.0 | Reference resistor value (ohm) |
| `r_nom` | 100.0 | Nominal RTD resistance at 0C |
| `wire` | `MAX31865_WIRE_3` | 2/3/4-wire RTD |
| `filter_50hz` | true | 50Hz vs 60Hz mains rejection |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/max31865/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_max31865
```

## Test status

- Unit tests (sim): 15 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Context struct pattern (like bmp280); Callendar-Van Dusen for T>=0C, linear approximation for T<0C; fault bit in LSB triggers ERR_FAULT on read.

## Responsible human

@user
