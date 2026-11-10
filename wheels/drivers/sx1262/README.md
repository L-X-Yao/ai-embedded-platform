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

## Datasheet check

Checked on 2026-11-05 against Semtech `sx126x_driver` and LoRaMac-node, and on 2026-11-06 against DS.SX1261-2.W.APP Rev. 1.2. Not run on hardware.

- **Matched:** 21 opcodes, IRQ bits, SF, BW and CR codes, 16 command frames, frequency word, buffer limits.
- **Fixed:** Every packet went out as 255 bytes, as the length was never updated; the "3 s" timeout was 300 ms; PA and TX power were never configured; RSSI used the instantaneous value; init did not check that a chip answered; `SX1262_ERR_CRC` now reports damaged packets, which also set RxDone; RX stops the RTC as required; added the BW500 and PA clamp workarounds, low data rate optimization, the TX timeout IRQ, a BW check and SPI error checks on reads.
- **Still open:** TX power is fixed at 14 dBm. SetRegulatorMode is not sent, so DC-DC boards stay on the LDO. There is no TCXO or RF switch control, so boards that need either will not run. Pulsing NRESET is left to the caller.
- **Fixed on 2026-11-06:** After `sx1262_sleep` every command failed on BUSY, and `sx1262_standby` now wakes the chip. Time on air left out 12 symbols, which could abort long packets at SF10 to SF12 and narrow bandwidths. The preamble is now 12 symbols at SF5 and SF6.
- **Suspected:** CalibrateImage is never sent, so bands outside 902-928 MHz keep the power-on image calibration. The 0x0889 workaround runs at configure time and not before each packet. There is no 600 ns guard before the first BUSY read.

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

## Migration

### Oversized packets are refused (2026-11)

- **What changed:** A packet longer than the buffer is dropped with the new `SX1262_ERR_SIZE`, where it used to return OK with a cut copy that looked whole. Nothing is copied, and `*out_len` holds the real length.
- **What a caller has to do:** Pass a buffer that fits the largest packet, or handle the new error. Code that wanted only the start of a long packet now has to take all of it. A `cap` of 255 fits any packet.
- **If they do nothing:** A long packet is lost instead of arriving cut. On that return `*out_len` is more than the buffer holds, so check the return value before using it.

## API convention deviations

None.

## API Review

Pending.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
