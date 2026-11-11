# max31865 — SPI RTD-to-digital converter for PT100/PT1000

**Status: candidate**
**Layer: L1 (drivers)**

MAX31865 RTD-to-digital converter driver. Reads 15-bit RTD ratio via SPI
registers, converts to temperature using Callendar-Van Dusen linearization
(IEC 60751). Supports PT100 and PT1000 probes in 2/3/4-wire configurations.

## Origin

Proactive build for industrial temperature sensing (RTD applications).

## Why self-written?

Searched GitHub (2026-05-02). Referenced polymurph/MAX31865 (unclear license) for register sequences and compensation algorithms. Polynomial RTD approximation reference. Self-written for platform convention consistency; cross-validated against reference.

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

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Datasheet check

Checked on 2026-11-06 against Maxim 19-6478 Rev 3 (7/15). Not run on hardware.

- **Matched:** Register addresses and the write bit, Config bits, read and write frames, the RTD word and its fault flag, the resistance formula, coefficients a and b, the fault masks for D7, D6, D3 and D2.
- **Fixed:** The square root never converged, so every reading was a straight line: 376.4 C at 400 C and 743.2 C at 850 C. Below 0 C the c term was missing, which gave -208.5 C at -200 C. All 46 rows of Table 9 now read within 0.02 C, and every code from -200 C to 850 C solves the manual's equation. A read during the first conversion, up to 66 ms after init, took the reset word for a result and returned `MAX31865_OK` with about -242 C. It now returns `MAX31865_ERR_NOT_READY`. The model now starts at the POR values, runs conversions at the times of p.3 and latches the threshold faults of p.16.
- **Still open:** `MAX31865_FAULT_REFINLOW` (0x20, D5) means REFIN- is above the threshold and `MAX31865_FAULT_REFINHIGH` (0x10, D4) means it is below, the opposite of what the names suggest. The values stay as they are, because swapping them would silently change what callers test, and the header now gives each meaning. There is no fault detection cycle, so D5, D4 and D3 are never set. The fault thresholds stay at their power-up values, so an RTD shorted to near zero is not flagged and reads about -242 C. There is no 1-shot mode and no DRDY, so the driver cannot tell when a result is new. The HAL is not told the SPI limits: modes 1 or 3, 5 MHz. Re-init on a chip that stayed powered can change the notch in auto mode, which p.14 forbids. A missing chip is not detected at init.
- **Suspected:** MSB and LSB are read in separate frames, so a conversion between them could mix two results. Bias and auto mode start in one write, with no settling time for an input filter. `max31865_clear_fault` toggles auto mode and leaves D5, D3 and D2 as read, where the manual asks for 0. The `disc < 0` branch is commented as cold but is reached far over range, where it returns -240 C.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/max31865/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_max31865
```

## Test status

- Unit tests (sim): 19 tests, all passing
- HIL: N/A

## API convention deviations

None.

## Runtime configuration

| Field | Example | Purpose |
|-------|---------|---------|
| `r_ref` | 430.0 | Reference resistor value (ohm) |
| `r_nom` | 100.0 | Nominal RTD resistance at 0C |
| `wire` | `MAX31865_WIRE_3` | 2/3/4-wire RTD |
| `filter_50hz` | true | 50Hz vs 60Hz mains rejection |

## API review

- **Date**: 2026-05-01
- **Key decisions**: Context struct pattern (like bmp280); Callendar-Van Dusen for T>=0C, its full form by Newton steps for T<0C (linear until 2026-11-06); fault bit in LSB triggers ERR_FAULT on read.

## Responsible human

@L-X-Yao
