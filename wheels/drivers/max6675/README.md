# max6675 -- MAX6675 K-type thermocouple-to-digital converter driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the Maxim MAX6675 K-type thermocouple
interface IC. SPI interface injected via callbacks. Read-only 12-bit
temperature output (0.25 deg C resolution, range 0 to +1023.75 deg C)
with open-thermocouple detection.

## Origin

Proactive build. MAX6675 is a simple, low-cost thermocouple interface
used in ovens, 3D printers, and industrial temperature monitoring.

## Why self-written?

No existing open-source component for MAX6675. Self-written for zero-dependency SPI HAL
injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

SPI HAL (injected via `max6675_spi_t` callbacks). No other dependencies.

## API

```c
max6675_err_t max6675_init(max6675_t *dev, const max6675_spi_t *spi);
max6675_err_t max6675_read(max6675_t *dev, float *temp_c);
max6675_err_t max6675_read_raw(max6675_t *dev, uint16_t *raw);
```

- `read()` -- returns temperature in degrees Celsius; returns
  `MAX6675_ERR_OPEN` if thermocouple is disconnected
- `read_raw()` -- returns the raw 16-bit SPI frame for diagnostics

## Usage

```c
max6675_t tc;
max6675_init(&tc, &spi_hal);

float temp;
max6675_err_t err = max6675_read(&tc, &temp);
if (err == MAX6675_OK) {
    // temp is in degrees Celsius
} else if (err == MAX6675_ERR_OPEN) {
    // thermocouple disconnected
}
```

## Configuration (compile-time)

None.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected SPI HAL)

## Known issues

- K-type only (no cold-junction compensation in software).
- Minimum conversion time is 220 ms between reads (hardware limitation).

## How to test

```bash
cd wheels/drivers/max6675/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_max6675
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
