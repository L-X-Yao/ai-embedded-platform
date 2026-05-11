# cc1101 -- TI CC1101 Sub-GHz transceiver driver

**Status: candidate**
**Layer: L1 (drivers)**

Register-based SPI driver for the TI CC1101 Sub-GHz transceiver. Supports
frequency configuration (300-928 MHz), variable-length packet TX/RX, RSSI
readout, and power control. Header byte protocol with R/W and burst bits.

## Origin

Proactive build for Sub-GHz ISM band projects. CC1101 is widely used for
433/868/915 MHz applications.

## Why self-written?

Self-written for platform convention consistency. Referenced TI CC1101
datasheet (SWRS061I) for register map and SPI protocol.

## Dependencies

- `spi_hal` (callback injection, no concrete dependency)

## How to test

```bash
cd wheels/drivers/cc1101/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_cc1101
```

## Supported platforms

- [x] Host (sim)
- [ ] Real hardware -- not yet verified

## Responsible human

@user