# ARM Binary Size Reference

Cross-compiled with `arm-none-eabi-gcc 13.2.1`, flags: `-Os -mcpu=cortex-m4 -mthumb -ffreestanding -ffunction-sections -fdata-sections -std=c99`.

Sizes are per-wheel `.o` files (before linking). Actual linked size will be smaller with `--gc-sections`.

Generated: 2026-04-30 by `tooling/audit/arm_size_audit.sh`.

## L0 — Primitives

| Wheel | .text | .data | .bss | Total | Notes |
|-------|------:|------:|-----:|------:|-------|
| log_uart | 32 | 0 | 0 | 32 | Thinnest sink |
| log_rtt | 40 | 0 | 0 | 40 | SEGGER RTT sink |
| timeout | 76 | 0 | 0 | 76 | |
| debounce | 120 | 0 | 0 | 120 | |
| retry | 120 | 0 | 0 | 120 | |
| log_file | 125 | 0 | 0 | 125 | stdio-based |
| log_rate | 152 | 0 | 0 | 152 | |
| median_filter | 202 | 0 | 0 | 202 | |
| checksum | 206 | 0 | 0 | 206 | |
| led_pattern | 232 | 0 | 0 | 232 | |
| bitfield | 270 | 0 | 0 | 270 | |
| state_machine | 278 | 0 | 0 | 278 | |
| event_queue | 294 | 0 | 0 | 294 | |
| soft_timer | 296 | 0 | 0 | 296 | |
| log_color | 305 | 0 | 0 | 305 | |
| log_filter | 304 | 0 | 0 | 304 | |
| log_ringbuf | 308 | 0 | 0 | 308 | |
| pid_controller | 310 | 0 | 0 | 310 | |
| log_dedup | 321 | 0 | 0 | 321 | |
| moving_average | 372 | 0 | 0 | 372 | |
| log | 438 | 1 | 52 | 491 | Core: 4 sink slots + level char |
| log_flash | 514 | 0 | 0 | 514 | |
| byte_buffer | 542 | 0 | 0 | 542 | |
| crc16 | 572 | 0 | 0 | 572 | Table-based |
| mag_cal | 586 | 0 | 0 | 586 | Float math |
| cli_parser | 651 | 0 | 0 | 651 | |
| crc32 | 1082 | 0 | 0 | 1082 | 1KB lookup table |
| mahony_ahrs | 2192 | 0 | 0 | 2192 | Heaviest L0: trig + sqrt |
| fal | — | — | — | — | Vendor, needs project config |
| kvs | — | — | — | — | Vendor, needs fal + project config |

## L1 — Drivers

| Wheel | .text | .data | .bss | Total | Notes |
|-------|------:|------:|-----:|------:|-------|
| rotary_encoder | 98 | 0 | 0 | 98 | Lightest driver |
| servo | 142 | 0 | 0 | 142 | |
| bh1750 | 224 | 0 | 0 | 224 | |
| xgzp6816d | 260 | 0 | 0 | 260 | |
| gpio_button | 262 | 0 | 0 | 262 | |
| ws2812 | 284 | 0 | 0 | 284 | |
| max7219 | 298 | 0 | 0 | 298 | |
| ads1115 | 312 | 0 | 0 | 312 | |
| gpio_led | 328 | 0 | 0 | 328 | |
| sht30 | 330 | 0 | 0 | 330 | |
| qmc5883p | 332 | 0 | 0 | 332 | |
| dht11 | 354 | 0 | 0 | 354 | |
| hmc5883l | 368 | 0 | 0 | 368 | |
| qmi8658a | 450 | 0 | 0 | 450 | |
| mpu6050 | 498 | 0 | 0 | 498 | |
| w25qxx | 546 | 0 | 0 | 546 | |
| bmp280 | 550 | 0 | 0 | 550 | Float compensation |
| ds18b20 | 702 | 0 | 0 | 702 | |
| oled_ssd1306 | 710 | 0 | 0 | 710 | Heaviest L1 |

## L2 — Protocols

| Wheel | .text | .data | .bss | Total | Notes |
|-------|------:|------:|-----:|------:|-------|
| ncp5623_parser | 150 | 0 | 0 | 150 | Lightest protocol |
| ota_ab | 460 | 0 | 0 | 460 | |
| websocket_frame | 494 | 0 | 0 | 494 | |
| at_command | 593 | 0 | 0 | 593 | |
| coap_packet | 710 | 0 | 0 | 710 | |
| nmea_parser | 967 | 0 | 0 | 967 | strtol/strtof |
| http_parser | 971 | 0 | 0 | 971 | |
| mqtt_packet | 992 | 0 | 0 | 992 | |
| modbus_rtu | 1050 | 0 | 0 | 1050 | Includes CRC16 |

## Summary

| Layer | Count | .text range | Median |
|-------|------:|-------------|-------:|
| L0 | 28 | 32 — 2192 B | 305 B |
| L1 | 19 | 98 — 710 B | 332 B |
| L2 | 9 | 150 — 1050 B | 593 B |
| **All** | **56** | **32 — 2192 B** | **310 B** |

**Total footprint if all 56 wheels are linked: 24.4 KB .text + 53 B .data/.bss** (before --gc-sections).

### MCU fit guide

| MCU Flash | Fits (approx.) |
|-----------|---------------|
| 16 KB | Any single wheel + app overhead |
| 32 KB | 10-15 wheels comfortably |
| 64 KB | All 56 wheels + headroom |
| 256 KB+ | All wheels + VSF + app |
