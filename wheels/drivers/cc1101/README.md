# cc1101 — TI CC1101 Sub-GHz transceiver driver (SPI)

**Status: candidate**
**Layer: L1 (drivers)**

Register-based SPI driver for Texas Instruments CC1101 Sub-GHz transceiver.
Handles frequency/power configuration, variable-length packet TX/RX,
RSSI readout, and command strobes.

## Origin

Proactive build for Sub-GHz IoT projects (433/868/915 MHz ISM bands).
CC1101 is widely used in low-cost wireless sensor networks.

## Why self-written?

Searched GitHub (2026-05). Referenced TI CC1101 datasheet (SWRS061I) and
LSatan/SmartRC-CC1101-Driver-Lib (LGPL). Self-written for platform convention
consistency (zero-dependency, MIT, DI pattern).

## Dependencies

- `spi_hal` (callback injection, no concrete dependency)

## API

```c
cc1101_err_t cc1101_init(cc1101_t *dev, const cc1101_hal_t *hal);
cc1101_err_t cc1101_set_frequency(cc1101_t *dev, uint32_t freq_hz);
cc1101_err_t cc1101_set_power(cc1101_t *dev, uint8_t pa_value);
cc1101_err_t cc1101_transmit(cc1101_t *dev, const uint8_t *data, uint8_t len);
cc1101_err_t cc1101_receive(cc1101_t *dev, uint8_t *buf, uint8_t cap, uint8_t *out_len);
int16_t      cc1101_get_rssi(cc1101_t *dev);
cc1101_err_t cc1101_idle(cc1101_t *dev);
```

## Configuration (compile-time)

None. All parameters set at runtime via API calls.

## Supported platforms

- [x] Host (sim) -- 2026-05-11
- [ ] Real hardware (CC1101 module) -- not yet verified

## Datasheet check

Checked on 2026-11-05 against TI SWRS061I. The errata notes were not available. Not run on hardware.

- **Matched:** Strobes, register and status addresses, SPI header bits, FREQ word, RSSI conversion, packet format, state flow.
- **Fixed:** The synthesizer was never calibrated; the received CRC was never checked, so `CC1101_ERR_CRC` is new; PKTLEN allowed packets the FIFO cannot hold; unsupported frequency gaps and the forbidden PATABLE values 0x61-0x6F were accepted; any VERSION other than 0x14 was rejected, though the datasheet says it can change; GDO0 kept its clock output; the RSSI comment was wrong. MARCSTATE is now read until two reads agree, as p.32 warns one read can be corrupt.
- **Still open:** Bitrate, deviation, bandwidth and modulation stay at reset values, with no API to set them.

## Known issues

- TX/RX uses polling, not interrupt-driven.
- FIFO limited to 61 bytes payload (64 FIFO - length byte - 2 status).

## How to test

```bash
cd wheels/drivers/cc1101/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_cc1101
```

## Test status

- Unit tests (sim): 29 pass
- Null tests: 11 pass
- HIL: N/A

## API convention deviations

None.

## API Review

Pending.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
