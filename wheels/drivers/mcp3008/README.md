# mcp3008

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Microchip MCP3008 10-bit 8-channel SPI ADC.
Supports single-ended and differential modes. SPI interface injected
via callbacks.

## Origin

Proactive build. MCP3008 is the most popular low-cost SPI ADC in
embedded applications (sensor acquisition, data logging, IoT).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-12). The MCP3008 SPI protocol
is a simple 3-byte exchange. Self-written for zero-dependency wheel
convention compliance and platform portability.

## Dependencies

SPI HAL (injected via `mcp3008_spi_t` callbacks). No other dependencies.

## API

```c
mcp3008_err_t mcp3008_init(mcp3008_t *dev, const mcp3008_spi_t *spi);
mcp3008_err_t mcp3008_read_single(mcp3008_t *dev, uint8_t channel,
                                  uint16_t *raw);
mcp3008_err_t mcp3008_read_diff(mcp3008_t *dev, uint8_t diff_pair,
                                uint16_t *raw);
mcp3008_err_t mcp3008_read_voltage(mcp3008_t *dev, uint8_t channel,
                                   float vref, float *voltage);
```

## Configuration (compile-time)

None. All configuration is runtime via API parameters.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-12
- [ ] ARM Cortex-M -- not yet verified (uses injected SPI HAL)

## Datasheet check

Checked on 2026-11-06 against Microchip DS21295D. Not run on hardware.

- **Matched:** The 24-clock frame, single-ended channel codes, result parsing, SPI mode 0,0, CS held low across all 3 bytes.
- **Fixed:** `mcp3008_read_diff` put the pair number in D2:D0, so pairs 1-3 read the wrong channels and CH4-CH7 were unreachable. `mcp3008_read_voltage` divided by 1023, so readings came out up to 1 LSB high. The model decoded pairs the same wrong way. `mcp3008_init` never drove CS high, so a chip powered up with CS low ignored the first read. The model fills the bits the manual calls unknown and checks CS.
- **Still open:** No call selects the swapped polarity codes or converts a differential reading to volts. The SPI clock limits are not stated for HAL authors: 3.6 MHz at 5 V, 1.35 MHz at 2.7 V, and rates below 10 kHz can affect linearity.
- **Suspected:** Nothing enforces the 270 ns CS-high time between conversions.

## Known issues

- No DMA or burst read mode. Each channel read is a separate 3-byte SPI
  transaction.
- Differential mode returns unsigned 10-bit value (MCP3008 does not return
  signed results for differential).

## How to test

```bash
cd wheels/drivers/mcp3008/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mcp3008
```

## Test status

- Unit tests (sim): PASS (27 tests + 8 hw_model tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-12
- **Key decisions**: Bulk transfer HAL (3-byte tx/rx) instead of byte-at-a-time xfer for better SPI DMA compatibility; cs_select returns bool for error propagation; separate single/diff/voltage APIs for clarity; 1024.0 divisor per DS21295D Equation 4-2.

## Responsible human
@user
