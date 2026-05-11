# sx1262 — SPI LoRa radio transceiver driver for Semtech SX1262

**Status: candidate**
**Layer: L1 (drivers)**

Command-based SPI driver for Semtech SX1262 LoRa transceiver. Handles
frequency, modulation, packet TX/RX, RSSI, and power mode control. Uses
busy-pin polling between commands per SX1262 protocol requirements.

## Origin

Proactive build for modern LoRa IoT projects. SX1262 is the successor to
SX1276 with improved sensitivity, lower power, and command-based interface.

## Why self-written?

Searched GitHub (2026-05). Referenced SX126x datasheets and RadioLib (MIT)
for command sequences. Self-written for platform convention consistency
(zero-dependency, DI pattern, no heap).

## Dependencies

- `spi_hal` (callback injection, no concrete dependency)

## API

```c
sx1262_err_t sx1262_init(sx1262_t *dev, const sx1262_hal_t *hal);
sx1262_err_t sx1262_set_frequency(sx1262_t *dev, uint32_t freq_hz);
sx1262_err_t sx1262_configure_lora(sx1262_t *dev, sx1262_sf_t sf, sx1262_bw_t bw, sx1262_cr_t cr);
sx1262_err_t sx1262_transmit(sx1262_t *dev, const uint8_t *data, uint8_t len);
sx1262_err_t sx1262_receive(sx1262_t *dev, uint8_t *buf, uint8_t cap, uint8_t *out_len);
int16_t      sx1262_get_rssi(sx1262_t *dev);
sx1262_err_t sx1262_sleep(sx1262_t *dev);
sx1262_err_t sx1262_standby(sx1262_t *dev);
uint8_t      sx1262_get_status(sx1262_t *dev);
```

## Configuration (compile-time)

None. All parameters set at runtime via API calls.

## Supported platforms

- [x] Host (sim) -- 2026-05-11
- [ ] Real hardware (SX1262 module) -- not yet verified

## Known issues

- TX/RX uses polling (delay_ms loop), not interrupt-driven.
- No DIO-based IRQ callbacks yet; suitable for polling-based applications.

## How to test

```bash
cd wheels/drivers/sx1262/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sx1262
```

## Test status

- Unit tests (sim): 30 pass
- Null tests: 13 pass
- HIL: N/A

## API convention deviations

None.

## API Review

Pending.

## Responsible human

@user
