# LoRa Sensor Node

Read ADXL345 3-axis accelerometer over I2C, COBS-encode the data for
reliable framing, and transmit wirelessly via SX1262 LoRa radio over SPI.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `cobs` | Byte-stuffing codec for zero-delimited framing |
| L1 | `adxl345` | I2C 3-axis accelerometer (2/4/8/16g) |
| L1 | `sx1262` | SPI LoRa radio transceiver (SX1262) |

## What It Demonstrates

1. **I2C + SPI on the same MCU** -- ADXL345 uses I2C (write_reg /
   read_regs callbacks), while SX1262 uses SPI (cs_select / transfer /
   is_busy callbacks). Each wheel gets its own HAL table.

2. **COBS framing** -- Raw accelerometer data (12 bytes of floats) is
   COBS-encoded before transmission. The receiver uses 0x00 as a frame
   delimiter, and COBS guarantees no 0x00 appears inside the payload.

3. **Packet structure** -- Simple fixed-size packets: 3 floats
   (x_g, y_g, z_g) packed into 12 bytes, COBS-encoded to max 14 bytes.

## Adapting for Your MCU

Replace HAL stubs with real peripheral calls:

- `hal_i2c_write_reg` / `hal_i2c_read_regs` -- I2C for ADXL345
- `hal_spi_cs_select` / `hal_spi_cs_deselect` -- SX1262 chip select
- `hal_spi_transfer` -- SPI full-duplex transfer
- `hal_spi_is_busy` -- SX1262 BUSY pin polling
- `hal_delay_ms` -- blocking millisecond delay

## Syntax Check

```bash
gcc -std=c99 -Wall -Wextra -fsyntax-only -I../../tooling/harness main.c
```

Designed for any Cortex-M with I2C + SPI. Tested in host simulation
(syntax-only -- HAL stubs are no-ops).
