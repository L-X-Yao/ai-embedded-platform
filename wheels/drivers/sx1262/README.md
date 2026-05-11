# sx1262 -- SPI LoRa/FSK transceiver driver for Semtech SX1262

**Status: candidate**
**Layer: L1 (drivers)**

Command-based SPI driver for the Semtech SX1262 LoRa transceiver. Supports
LoRa modulation configuration (SF/BW/CR), packet TX/RX with polling, RSSI
readout, and sleep/standby power modes. All SPI, busy-pin, and timing access
is HAL-injected.

## Origin

Proactive build for next-gen LoRa projects. SX1262 is the successor to
SX1276 with better power efficiency and wider frequency range.

## Why self-written?

SX1262 uses a command-based SPI protocol (not register-based like SX1276).
Self-written for platform convention consistency and zero-dependency design.

## Dependencies

- `spi_hal` (callback injection, no concrete dependency)

## How to test

```bash
cd wheels/drivers/sx1262/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sx1262
```

## Supported platforms

- [x] Host (sim)
- [ ] Real hardware -- not yet verified

## Responsible human

@user