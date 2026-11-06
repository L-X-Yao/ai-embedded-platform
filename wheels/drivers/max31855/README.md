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

## Datasheet check

Checked on 2026-11-06 against Maxim 19-5793 Rev 5 (1/15). Not run on hardware.

- **Matched:** The 32-bit frame and its bit order, the 14-bit and 12-bit signed fields with all rows of Tables 4 and 5, the D16, D2, D1 and D0 fault bits.
- **Fixed:** The model and the test frame now give D31 = 0 and D[30:18] all 1 on an open circuit, and the model takes the frame when CS falls and drives SO only with CS low.
- **Still open:** There is no nonlinearity correction, so the output is the chip's linear value, which p.8 says to correct. Reads faster than tCONV (100 ms maximum) repeat the last result with no warning. D17 and D3 are not checked, so a stuck-low or floating SO goes undetected and 0x00000000 reads as 0 C. The HAL is not told the SPI limits, such as 5 MHz.
- **Suspected:** Nothing tells the caller to wait 200 ms after power-up, so an early read can be inaccurate. A frame with D16 set and D[2:0] clear returns `MAX31855_OK` and leaves the outputs unwritten.

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
