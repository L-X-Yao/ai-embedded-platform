# GPS Tracker -- Integration Recipe

Demonstrates seven wheels working together as a GPS tracking device
with time synchronization and SD card data logging. Runs entirely on
the host with mock I2C, SPI, and UART.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `kalman_1d` | Filter noisy GPS latitude/longitude readings |
| L0 | `base64` | Encode binary GPS fix records for storage/transmission |
| L0 | `uuid` | Generate unique tracking session IDs |
| L1 | `ds3231` | RTC for timestamping GPS fixes |
| L1 | `sd_spi` | SD card block storage for logged data |
| L2 | `sntp_client` | NTP time sync request framing |
| L2 | `ubx_parser` | u-blox UBX binary protocol GPS message parsing |

## What It Demonstrates

1. **Mock HAL injection** -- DS3231 receives mock I2C callbacks with
   BCD-encoded registers; SD card receives a mock SPI HAL that
   simulates the full init handshake (CMD0/CMD8/ACMD41/CMD58) and
   block read/write via RAM buffer; UUID gets a mock RNG callback.

2. **Protocol parsing** -- A UBX NAV-PVT binary message is built with
   `ubx_parser_build` (correct checksums), then parsed byte-by-byte
   through the incremental `ubx_parser_feed` state machine to extract
   lat/lon/alt.

3. **Signal filtering** -- Raw GPS coordinates pass through two
   `kalman_1d` instances (one for latitude, one for longitude),
   demonstrating noise reduction from 5 noisy readings converging
   toward the true position.

4. **Time sync framing** -- An SNTP v4 client request packet is built
   and its structure verified (48 bytes, version=4, mode=3).

5. **Data encoding round-trip** -- A GPS fix record is base64-encoded,
   written to an SD card block, read back, base64-decoded, and every
   field verified against the original.

6. **End-to-end data flow** -- The full tracking pipeline runs in a
   single test: RTC read, GPS parse, Kalman filter, record build,
   base64 encode, SD write, SD read, base64 decode, field verify.

## Build and Run

```bash
cd examples/gps_tracker
cmake -GNinja -B build && ninja -C build && ./build/gps_tracker
```

Expected output: all 8 integration tests pass, with GPS coordinates,
Kalman filter convergence data, and round-trip verification printed.

## Data Flow

```
  GPS (UBX) ---[ubx_parser]--> lat/lon/alt
                                    |
                              [kalman_1d x2]
                                    |
  DS3231 ---[ds3231]--> timestamp   |
                            |       |
                            v       v
                       gps_fix_record_t
                              |
  UUID ---[uuid]-------> session_id  |
                              |      |
                              v      v
  SNTP ---[sntp_client]    [base64_encode]
   (time sync framing)         |
                               v
                         [sd_spi_write]
                               |
                               v
                          SD card block
                               |
                               v
                         [sd_spi_read]
                               |
                               v
                        [base64_decode]
                               |
                               v
                       gps_fix_record_t (verified)
```
