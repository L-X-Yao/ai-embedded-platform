# Modbus Slave

Expose BMP280 sensor readings as Modbus RTU input registers over UART.
A Modbus master can poll temperature and pressure via standard FC 0x04
read requests.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `ring_buffer` | UART RX byte buffering (ISR to main loop) |
| L1 | `bmp280` | I2C temperature + pressure sensor |
| L2 | `modbus_rtu` | Modbus RTU frame codec + slave request handler |

## What It Demonstrates

1. **Protocol integration** -- The modbus_rtu slave handler parses
   incoming frames, dispatches to register callbacks, and builds
   response frames. The application only defines the register map.

2. **Register mapping** -- Sensor data is exposed as Modbus input
   registers (read-only via FC 0x04):
   - Register 0x0000: temperature (x100, e.g. 2512 = 25.12 C)
   - Register 0x0001: pressure (hPa integer, e.g. 1013)

3. **UART RX buffering** -- Bytes from the UART RX ISR flow into a
   ring_buffer. The main loop drains it and feeds complete frames to
   the Modbus processor. This decouples ISR timing from processing.

4. **Volatile shared data** -- Sensor readings written in the main
   loop and read by Modbus callbacks use `volatile` to ensure
   compiler does not cache stale values.

## Adapting for Your MCU

Replace HAL stubs:

- `hal_i2c_write_reg` / `hal_i2c_read_regs` -- I2C for BMP280
- `hal_uart_send` -- UART TX (assert RS-485 DE pin if needed)
- Wire UART RX ISR to `ring_buffer_write(&uart_rx_rb, &byte, 1)`
- `hal_delay_ms` -- blocking millisecond delay

For RS-485: toggle the DE (driver enable) pin before/after UART TX.

## Syntax Check

```bash
gcc -std=c99 -Wall -Wextra -fsyntax-only -I../../tooling/harness main.c
```

Designed for any Cortex-M with I2C + UART. Tested in host simulation
(syntax-only -- HAL stubs are no-ops).
