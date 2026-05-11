# Sensor Dashboard

Read BMP280 (temperature + pressure) and SHT30 (humidity) sensors,
output structured log messages with timestamps and severity levels.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `log` | Leveled, module-tagged logging with configurable sinks |
| L1 | `bmp280` | I2C temperature + barometric pressure sensor |
| L1 | `sht30` | I2C temperature + humidity sensor |

## What It Demonstrates

1. **I2C HAL injection** -- Each sensor receives its own I2C callback
   table. BMP280 uses register-based I2C (write_reg/read_regs), while
   SHT30 uses raw byte I2C (write/read). Both share the same physical
   I2C bus on different addresses.

2. **Log sink pattern** -- The log wheel routes formatted messages
   through a callback. Swap `uart_log_sink` for an RTT, USB-CDC, or
   ring-buffer sink without changing any sensor code.

3. **Periodic polling** -- Simple main-loop polling at 1 Hz. In an
   RTOS system, replace the delay with `vTaskDelay()`.

## Adapting for Your MCU

Replace the HAL stub functions with real peripheral calls:

- `hal_bmp_write_reg` / `hal_bmp_read_regs` -- I2C memory write/read
- `hal_sht_write` / `hal_sht_read` -- I2C raw transmit/receive
- `hal_delay_ms` -- blocking millisecond delay
- `uart_log_sink` -- UART transmit for log output

## Syntax Check

```bash
gcc -std=c99 -Wall -Wextra -fsyntax-only -I../../tooling/harness main.c
```

Designed for any Cortex-M with I2C + UART. Tested in host simulation
(syntax-only -- HAL stubs are no-ops).
