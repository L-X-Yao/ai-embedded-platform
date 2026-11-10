# ARM Binary Size Reference

Cross-compiled with `arm-none-eabi-gcc 13.2.1`, flags: `-Os -mcpu=cortex-m4 -mthumb -ffreestanding -ffunction-sections -fdata-sections -std=c99`.

Sizes are per-wheel `.o` files (before linking). Actual linked size will be smaller with `--gc-sections`.

Generated: 2026-11-11 by `tooling/audit/arm_size_audit.sh`. Covers all 259 wheels.

## L0 — Primitives (77 wheels)

| Wheel | .text | .data | .bss | Total |
|-------|------:|------:|-----:|------:|
| log_uart | 32 | 0 | 0 | 32 |
| log_rtt | 40 | 0 | 0 | 40 |
| timeout | 82 | 0 | 0 | 82 |
| debounce | 120 | 0 | 0 | 120 |
| log_file | 125 | 0 | 0 | 125 |
| double_buffer | 130 | 0 | 0 | 130 |
| retry | 130 | 0 | 0 | 130 |
| endian | 148 | 0 | 0 | 148 |
| log_rate | 158 | 0 | 0 | 158 |
| crc8 | 189 | 0 | 0 | 189 |
| hash_fnv | 192 | 0 | 0 | 192 |
| ewma | 200 | 0 | 0 | 200 |
| median_filter | 202 | 0 | 0 | 202 |
| memory_pool | 214 | 0 | 0 | 214 |
| checksum | 222 | 0 | 0 | 222 |
| bcd | 224 | 0 | 0 | 224 |
| iir_lowpass | 226 | 0 | 0 | 226 |
| rle_codec | 226 | 0 | 0 | 226 |
| hysteresis | 232 | 0 | 0 | 232 |
| led_pattern | 240 | 0 | 0 | 240 |
| rate_limiter | 250 | 0 | 0 | 250 |
| bitstream | 260 | 0 | 0 | 260 |
| cobs | 276 | 0 | 0 | 276 |
| lookup_table | 276 | 0 | 0 | 276 |
| bitfield | 282 | 0 | 0 | 282 |
| kalman_1d | 288 | 0 | 0 | 288 |
| peak_detector | 294 | 0 | 0 | 294 |
| varint | 294 | 0 | 0 | 294 |
| event_queue | 302 | 0 | 0 | 302 |
| log_filter | 304 | 0 | 0 | 304 |
| log_color | 305 | 0 | 0 | 305 |
| signal_stats | 314 | 0 | 0 | 314 |
| log_ringbuf | 320 | 0 | 0 | 320 |
| state_machine | 326 | 0 | 0 | 326 |
| moving_rms | 332 | 0 | 0 | 332 |
| fifo_queue | 342 | 0 | 0 | 342 |
| timer_wheel | 346 | 0 | 0 | 346 |
| hex_codec | 352 | 0 | 0 | 352 |
| scale_map | 364 | 0 | 0 | 364 |
| log_dedup | 389 | 0 | 0 | 389 |
| hmac_sha256 | 398 | 0 | 0 | 398 |
| ring_buffer | 402 | 0 | 0 | 402 |
| soft_timer | 432 | 0 | 0 | 432 |
| log | 442 | 1 | 52 | 495 |
| moving_average | 446 | 0 | 0 | 446 |
| moving_window | 460 | 0 | 0 | 460 |
| uuid | 470 | 0 | 0 | 470 |
| fal | 498 | 0 | 1 | 499 |
| pid_controller | 516 | 0 | 0 | 516 |
| soft_spi | 540 | 0 | 0 | 540 |
| slip | 544 | 0 | 0 | 544 |
| byte_buffer | 546 | 0 | 0 | 546 |
| log_flash | 548 | 0 | 0 | 548 |
| utf8 | 570 | 0 | 0 | 570 |
| crc16 | 580 | 0 | 0 | 580 |
| bitmap | 600 | 0 | 0 | 600 |
| datetime | 609 | 0 | 0 | 609 |
| font_5x7 | 619 | 0 | 0 | 619 |
| onewire | 674 | 0 | 0 | 674 |
| cli_parser | 745 | 0 | 0 | 745 |
| fixed_point | 746 | 0 | 0 | 746 |
| interpolate | 764 | 0 | 0 | 764 |
| mag_cal | 766 | 0 | 0 | 766 |
| base64 | 791 | 0 | 0 | 791 |
| string_utils | 918 | 0 | 0 | 918 |
| sha256 | 1026 | 0 | 0 | 1026 |
| soft_i2c | 1030 | 0 | 0 | 1030 |
| crc32 | 1082 | 0 | 0 | 1082 |
| printf_tiny | 1182 | 0 | 0 | 1182 |
| gnss_filter | 1202 | 0 | 0 | 1202 |
| flash_ring | 1354 | 0 | 0 | 1354 |
| json_parser | 1384 | 0 | 0 | 1384 |
| framebuffer | 1452 | 0 | 0 | 1452 |
| heatshrink | 1678 | 0 | 0 | 1678 |
| aes128 | 1786 | 0 | 0 | 1786 |
| mahony_ahrs | 2436 | 0 | 0 | 2436 |
| kvs | 6881 | 0 | 130 | 7011 |

## L1 — Drivers (115 wheels)

| Wheel | .text | .data | .bss | Total |
|-------|------:|------:|-----:|------:|
| rotary_encoder | 148 | 0 | 0 | 148 |
| max6675 | 178 | 0 | 0 | 178 |
| servo | 206 | 0 | 0 | 206 |
| pct2075 | 220 | 0 | 0 | 220 |
| sk6812 | 250 | 0 | 0 | 250 |
| xgzp6816d | 266 | 0 | 0 | 266 |
| max31855 | 278 | 0 | 0 | 278 |
| ht16k33 | 284 | 0 | 0 | 284 |
| mcp3008 | 286 | 0 | 0 | 286 |
| pcf8574 | 290 | 0 | 0 | 290 |
| apa102 | 298 | 0 | 0 | 298 |
| drv8833 | 312 | 0 | 0 | 312 |
| lis2dh12 | 320 | 0 | 0 | 320 |
| mcp4728 | 320 | 0 | 0 | 320 |
| gpio_button | 322 | 0 | 0 | 322 |
| hcsr04 | 326 | 0 | 0 | 326 |
| cst816s | 328 | 0 | 0 | 328 |
| vl53l1x | 328 | 0 | 0 | 328 |
| tmp117 | 332 | 0 | 0 | 332 |
| qmc5883p | 344 | 0 | 0 | 344 |
| tca9548a | 344 | 0 | 0 | 344 |
| dht11 | 368 | 0 | 0 | 368 |
| ws2812 | 368 | 0 | 0 | 368 |
| ft6x36 | 372 | 0 | 0 | 372 |
| max7219 | 372 | 0 | 0 | 372 |
| sgp40 | 372 | 0 | 0 | 372 |
| as5600 | 388 | 0 | 0 | 388 |
| ads1015 | 392 | 0 | 0 | 392 |
| lm75 | 396 | 0 | 0 | 396 |
| adxl345 | 402 | 0 | 0 | 402 |
| bh1750 | 402 | 0 | 0 | 402 |
| ads1115 | 404 | 0 | 0 | 404 |
| sht30 | 404 | 0 | 0 | 404 |
| bmm150 | 416 | 0 | 0 | 416 |
| a4988 | 427 | 0 | 0 | 427 |
| gpio_led | 436 | 0 | 0 | 436 |
| tb6612 | 436 | 0 | 0 | 436 |
| mpr121 | 444 | 0 | 0 | 444 |
| opt3001 | 446 | 0 | 0 | 446 |
| bno055 | 448 | 0 | 0 | 448 |
| ina3221 | 450 | 0 | 0 | 450 |
| lis3mdl | 462 | 0 | 0 | 462 |
| keypad_matrix | 470 | 0 | 0 | 470 |
| mcp4725 | 472 | 0 | 0 | 472 |
| htu21d | 480 | 0 | 0 | 480 |
| qmi8658a | 488 | 0 | 0 | 488 |
| ccs811 | 492 | 0 | 0 | 492 |
| tmp102 | 500 | 0 | 0 | 500 |
| ltr390 | 532 | 0 | 0 | 532 |
| mlx90614 | 532 | 0 | 0 | 532 |
| veml7700 | 536 | 0 | 0 | 536 |
| drv2605 | 544 | 0 | 0 | 544 |
| at24cxx | 546 | 0 | 0 | 546 |
| stepper | 548 | 0 | 0 | 548 |
| bmi160 | 554 | 0 | 0 | 554 |
| aht20 | 558 | 0 | 0 | 558 |
| lps22hb | 559 | 0 | 0 | 559 |
| w25qxx | 560 | 0 | 0 | 560 |
| mcp23017 | 570 | 0 | 0 | 570 |
| hx711 | 578 | 0 | 0 | 578 |
| lis3dh | 582 | 0 | 0 | 582 |
| ds18b20 | 590 | 0 | 0 | 590 |
| vl53l0x | 604 | 0 | 0 | 604 |
| hmc5883l | 612 | 0 | 0 | 612 |
| shtc3 | 624 | 0 | 0 | 624 |
| sht40 | 632 | 0 | 0 | 632 |
| hd44780 | 648 | 0 | 0 | 648 |
| tsl2591 | 650 | 0 | 0 | 650 |
| ina219 | 654 | 0 | 0 | 654 |
| mpu6050 | 658 | 0 | 0 | 658 |
| bma400 | 660 | 0 | 0 | 660 |
| is31fl3731 | 670 | 0 | 0 | 670 |
| lsm6dso | 672 | 0 | 0 | 672 |
| sgp30 | 690 | 0 | 0 | 690 |
| buzzer | 704 | 0 | 0 | 704 |
| ina226 | 706 | 0 | 0 | 706 |
| sh1106 | 710 | 0 | 0 | 710 |
| tcs34725 | 738 | 0 | 0 | 738 |
| max31865 | 752 | 0 | 0 | 752 |
| scd40 | 770 | 0 | 0 | 770 |
| oled_ssd1306 | 778 | 0 | 0 | 778 |
| st7735 | 784 | 0 | 0 | 784 |
| pac1952 | 790 | 0 | 0 | 790 |
| ili9341 | 794 | 0 | 0 | 794 |
| gc9a01 | 813 | 0 | 0 | 813 |
| max17048 | 828 | 0 | 0 | 828 |
| apds9960 | 830 | 0 | 0 | 830 |
| aw9523 | 846 | 0 | 0 | 846 |
| bq27441 | 858 | 0 | 0 | 858 |
| si7021 | 872 | 0 | 0 | 872 |
| lcd_st7789 | 877 | 0 | 0 | 877 |
| ds3231 | 878 | 0 | 0 | 878 |
| sd_spi | 916 | 0 | 0 | 916 |
| icm42688 | 948 | 0 | 0 | 948 |
| bmi270 | 1030 | 0 | 0 | 1030 |
| tm1637 | 1036 | 0 | 0 | 1036 |
| cc1101 | 1048 | 0 | 0 | 1048 |
| ms5611 | 1053 | 0 | 0 | 1053 |
| pca9685 | 1064 | 0 | 0 | 1064 |
| dps310 | 1072 | 0 | 0 | 1072 |
| pcf8563 | 1128 | 0 | 0 | 1128 |
| bmp280 | 1200 | 0 | 0 | 1200 |
| max30102 | 1266 | 0 | 0 | 1266 |
| nrf24l01 | 1302 | 0 | 0 | 1302 |
| rc522 | 1336 | 0 | 0 | 1336 |
| w5500 | 1402 | 0 | 0 | 1402 |
| mcp2515 | 1475 | 0 | 0 | 1475 |
| bmp388 | 1508 | 0 | 0 | 1508 |
| enc28j60 | 1572 | 0 | 0 | 1572 |
| lora_sx1276 | 1860 | 0 | 0 | 1860 |
| bme280 | 1932 | 0 | 0 | 1932 |
| bme688 | 2182 | 0 | 0 | 2182 |
| air780eg | 2190 | 0 | 0 | 2190 |
| sx1262 | 2376 | 0 | 0 | 2376 |
| bme680 | 2616 | 0 | 0 | 2616 |

## L2 — Protocols (47 wheels)

| Wheel | .text | .data | .bss | Total |
|-------|------:|------:|-----:|------:|
| ncp5623_parser | 156 | 0 | 0 | 156 |
| j1939 | 182 | 0 | 0 | 182 |
| ibus | 204 | 0 | 0 | 204 |
| dmx512 | 286 | 0 | 0 | 286 |
| uds | 306 | 0 | 0 | 306 |
| crc_any | 350 | 0 | 0 | 350 |
| lin_bus | 384 | 0 | 0 | 384 |
| isotp | 394 | 0 | 0 | 394 |
| tlv_codec | 400 | 0 | 0 | 400 |
| crsf | 416 | 0 | 0 | 416 |
| ubx_parser | 434 | 0 | 0 | 434 |
| modbus_tcp | 438 | 0 | 0 | 438 |
| lorawan_packet | 466 | 0 | 0 | 466 |
| onewire_search | 478 | 0 | 0 | 478 |
| sbus | 506 | 0 | 0 | 506 |
| modbus_ascii | 527 | 0 | 0 | 527 |
| hdlc | 550 | 0 | 0 | 550 |
| midi_packet | 578 | 0 | 0 | 578 |
| i2c_scan | 584 | 0 | 0 | 584 |
| websocket_frame | 598 | 0 | 0 | 598 |
| intel_hex | 661 | 0 | 0 | 661 |
| s_record | 661 | 0 | 0 | 661 |
| mavlink | 666 | 0 | 0 | 666 |
| tftp | 672 | 0 | 0 | 672 |
| sntp_client | 698 | 0 | 0 | 698 |
| xmodem | 744 | 0 | 0 | 744 |
| ir_nec | 766 | 0 | 0 | 766 |
| coap_packet | 796 | 0 | 0 | 796 |
| dns_packet | 810 | 0 | 0 | 810 |
| http_request | 824 | 0 | 0 | 824 |
| dhcp_packet | 840 | 0 | 0 | 840 |
| ota_ab | 850 | 0 | 0 | 850 |
| canbus | 872 | 0 | 0 | 872 |
| ymodem | 925 | 0 | 0 | 925 |
| at_command | 974 | 0 | 0 | 974 |
| bencode | 994 | 0 | 0 | 994 |
| protobuf_lite | 1052 | 0 | 0 | 1052 |
| mqtt_packet | 1140 | 0 | 0 | 1140 |
| modbus_rtu | 1142 | 0 | 0 | 1142 |
| ppp_frame | 1148 | 0 | 0 | 1148 |
| syslog_packet | 1152 | 0 | 0 | 1152 |
| cbor_lite | 1198 | 0 | 0 | 1198 |
| json_rpc | 1285 | 0 | 0 | 1285 |
| http_parser | 1305 | 0 | 0 | 1305 |
| msgpack | 1622 | 0 | 0 | 1622 |
| nmea_parser | 1723 | 0 | 0 | 1723 |
| mqtt5_packet | 1913 | 0 | 0 | 1913 |

## L3 — Modules (20 wheels)

| Wheel | .text | .data | .bss | Total |
|-------|------:|------:|-----:|------:|
| version_info | 353 | 0 | 0 | 353 |
| command_queue | 358 | 0 | 0 | 358 |
| ring_log | 370 | 0 | 0 | 370 |
| power_manager | 374 | 0 | 0 | 374 |
| task_watchdog | 418 | 0 | 0 | 418 |
| device_info | 426 | 0 | 0 | 426 |
| error_handler | 452 | 0 | 0 | 452 |
| scheduler | 470 | 0 | 0 | 470 |
| event_bus | 526 | 0 | 0 | 526 |
| boot_counter | 528 | 0 | 0 | 528 |
| ota_manager | 556 | 0 | 0 | 556 |
| button_handler | 574 | 0 | 0 | 574 |
| sensor_hub | 594 | 0 | 0 | 594 |
| param_table | 656 | 0 | 0 | 656 |
| cli_shell | 811 | 0 | 0 | 811 |
| data_logger | 1108 | 0 | 0 | 1108 |
| telemetry | 1459 | 0 | 0 | 1459 |
| flash_journal | 1532 | 0 | 0 | 1532 |
| mqtt_client | 1550 | 0 | 0 | 1550 |
| config_store | 1720 | 0 | 0 | 1720 |

## Summary

| Layer | Count | .text range | Median |
|-------|------:|-------------|-------:|
| L0 Primitives | 77 | 32 -- 6881 B | 364 B |
| L1 Drivers | 115 | 148 -- 2616 B | 560 B |
| L2 Protocols | 47 | 156 -- 1913 B | 672 B |
| L3 Modules | 20 | 353 -- 1720 B | 542 B |
| **Total compiled** | **259** | **32 -- 6881 B** | **546 B** |

**Total footprint: 173.9 KB .text + 1 B .data + 183 B .bss = 174.1 KB** (before --gc-sections).

### MCU fit guide

The average wheel is 687 B of `.text`. The counts below leave about a third of the flash for the application and startup code.

| MCU Flash | Fits (approx.) |
|-----------|---------------|
| 16 KB | Any single wheel + app overhead |
| 32 KB | About 30 wheels of average size |
| 64 KB | About 60 wheels of average size |
| 128 KB | About 120 wheels of average size |
| 256 KB+ | All 259 wheels (174 KB) + app logic |
