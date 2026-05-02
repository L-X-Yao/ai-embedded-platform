# max31855 — MAX31855 SPI thermocouple-to-digital converter

**Status: candidate**
**Layer: L1 (drivers)**

Read K/J/N/T-type thermocouple temperature via SPI. The MAX31855 outputs a
32-bit read-only frame containing 14-bit thermocouple temperature (0.25 C/LSB)
and 12-bit internal cold-junction reference (0.0625 C/LSB), plus fault flags
for open circuit, short-to-GND, and short-to-VCC.
## Origin

Proactive build for thermocouple sensing use cases (ovens, industrial temp
monitoring, 3D printer hotends).


## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. SPI read-only 32 bits, trivially simple. Self-written with HAL injection per platform conventions.

## Dependencies

None (SPI HAL injected via callbacks).

## API

```c
typedef enum {
    MAX31855_OK            =  0,
    MAX31855_ERR_SPI       = -1,
    MAX31855_ERR_PARAMS    = -2,
    MAX31855_ERR_OPEN      = -3,   /* thermocouple open circuit */
    MAX31855_ERR_SHORT_GND = -4,
    MAX31855_ERR_SHORT_VCC = -5,
} max31855_err_t;

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} max31855_spi_t;

typedef struct {
    const max31855_spi_t *spi;
} max31855_t;

max31855_err_t max31855_init(max31855_t *dev, const max31855_spi_t *spi);
max31855_err_t max31855_read(max31855_t *dev, float *thermocouple_c, float *internal_c);
max31855_err_t max31855_read_raw(max31855_t *dev, uint32_t *raw);
```

## Configuration (compile-time)

None. The driver has no compile-time options.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/max31855/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_max31855
```

## Test status

- Unit tests (sim): 17 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Read-only device, no register writes. Fault detection
  returns specific error codes rather than a bitmask, matching platform
  error-code conventions.

## Responsible human

@user
