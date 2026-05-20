# ARM Binary Size Reference

Cross-compiled with `arm-none-eabi-gcc 13.2.1`, flags: `-Os -mcpu=cortex-m4 -mthumb -ffreestanding -ffunction-sections -fdata-sections -std=c99`.

Sizes are per-wheel `.o` files (before linking). Actual linked size will be smaller with `--gc-sections`.

Generated: 2026-05-20 by `tooling/audit/arm_size_audit.sh`. Covers all 259 wheels.

## L0 — Primitives (77 wheels)

| Wheel | .text | .data | .bss | Total |
|-------|------:|------:|-----:|------:|
| log_uart | 32 | 0 | 0 | 32 |
| log_rtt | 40 | 0 | 0 | 40 |
| timeout | 82 | 0 | 0 | 82 |
| ewma | 86 | 0 | 0 | 86 |
| double_buffer | 114 | 0 | 0 | 114 |
| debounce | 120 | 0 | 0 | 120 |
| log_file | 125 | 0 | 0 | 125 |
| retry | 130 | 0 | 0 | 130 |
| endian | 148 | 0 | 0 | 148 |
| log_rate | 158 | 0 | 0 | 158 |
| crc8 | 189 | 0 | 0 | 189 |
| median_filter | 202 | 0 | 0 | 202 |
| memory_pool | 206 | 0 | 0 | 206 |
| scale_map | 208 | 0 | 0 | 208 |
| checksum | 222 | 0 | 0 | 222 |
| iir_lowpass | 226 | 0 | 0 | 226 |
| rle_codec | 226 | 0 | 0 | 226 |
| hysteresis | 232 | 0 | 0 | 232 |
| led_pattern | 240 | 0 | 0 | 240 |
| rate_limiter | 250 | 0 | 0 | 250 |
| bitstream | 260 | 0 | 0 | 260 |
| moving_rms | 262 | 0 | 0 | 262 |
| kalman_1d | 266 | 0 | 0 | 266 |
| lookup_table | 266 | 0 | 0 | 266 |
| signal_stats | 270 | 0 | 0 | 270 |
| peak_detector | 272 | 0 | 0 | 272 |
| cobs | 276 | 0 | 0 | 276 |
| bitfield | 282 | 0 | 0 | 282 |
| fifo_queue | 282 | 0 | 0 | 282 |
| timer_wheel | 284 | 0 | 0 | 284 |
| varint | 284 | 0 | 0 | 284 |
| bcd | 224 | 0 | 0 | 224 |
| state_machine | 302 | 0 | 0 | 302 |
| log_color | 305 | 0 | 0 | 305 |
| log_filter | 304 | 0 | 0 | 304 |
| ring_buffer | 312 | 0 | 0 | 312 |
| event_queue | 314 | 0 | 0 | 314 |
| log_ringbuf | 320 | 0 | 0 | 320 |
| hex_codec | 352 | 0 | 0 | 352 |
| soft_timer | 368 | 0 | 0 | 368 |
| hmac_sha256 | 374 | 0 | 0 | 374 |
| pid_controller | 382 | 0 | 0 | 382 |
| fal | 404 | 0 | 1 | 405 |
| moving_average | 410 | 0 | 0 | 410 |
| log | 442 | 1 | 52 | 495 |
| moving_window | 440 | 0 | 0 | 440 |
| log_dedup | 451 | 0 | 0 | 451 |
| uuid | 470 | 0 | 0 | 470 |
| slip | 472 | 0 | 0 | 472 |
| byte_buffer | 542 | 0 | 0 | 542 |
| soft_spi | 540 | 0 | 0 | 540 |
| log_flash | 548 | 0 | 0 | 548 |
| flash_ring | 552 | 0 | 0 | 552 |
| utf8 | 570 | 0 | 0 | 570 |
| crc16 | 580 | 0 | 0 | 580 |
| bitmap | 600 | 0 | 0 | 600 |
| onewire | 600 | 0 | 0 | 600 |
| datetime | 609 | 0 | 0 | 609 |
| font_5x7 | 619 | 0 | 0 | 619 |
| mag_cal | 642 | 0 | 0 | 642 |
| cli_parser | 685 | 0 | 0 | 685 |
| gnss_filter | 704 | 0 | 0 | 704 |
| fixed_point | 734 | 0 | 0 | 734 |
| interpolate | 764 | 0 | 0 | 764 |
| base64 | 770 | 0 | 0 | 770 |
| string_utils | 894 | 0 | 0 | 894 |
| soft_i2c | 944 | 0 | 0 | 944 |
| hash_fnv | 192 | 0 | 0 | 192 |
| sha256 | 1008 | 0 | 0 | 1008 |
| crc32 | 1082 | 0 | 0 | 1082 |
| printf_tiny | 1156 | 0 | 0 | 1156 |
| json_parser | 1308 | 0 | 0 | 1308 |
| framebuffer | 1472 | 0 | 0 | 1472 |
| heatshrink | 1774 | 0 | 0 | 1774 |
| aes128 | 1786 | 0 | 0 | 1786 |
| mahony_ahrs | 2296 | 0 | 0 | 2296 |
| kvs | 6864 | 0 | 130 | 6994 |

## L1 — Drivers (115 wheels)

| Wheel | .text | .data | .bss | Total |
|-------|------:|------:|-----:|------:|
| rotary_encoder | 142 | 0 | 0 | 142 |
| max6675 | 152 | 0 | 0 | 152 |
| servo | 158 | 0 | 0 | 158 |
| pct2075 | 168 | 0 | 0 | 168 |
| mcp4728 | 194 | 0 | 0 | 194 |
| sk6812 | 214 | 0 | 0 | 214 |
| bh1750 | 248 | 0 | 0 | 248 |
| mcp3008 | 256 | 0 | 0 | 256 |
| max31855 | 258 | 0 | 0 | 258 |
| hcsr04 | 264 | 0 | 0 | 264 |
| pcf8574 | 266 | 0 | 0 | 266 |
| xgzp6816d | 266 | 0 | 0 | 266 |
| gpio_button | 272 | 0 | 0 | 272 |
| ht16k33 | 272 | 0 | 0 | 272 |
| tca9548a | 278 | 0 | 0 | 278 |
| tmp117 | 286 | 0 | 0 | 286 |
| apa102 | 288 | 0 | 0 | 288 |
| drv8833 | 296 | 0 | 0 | 296 |
| bmm150 | 298 | 0 | 0 | 298 |
| max7219 | 298 | 0 | 0 | 298 |
| lis2dh12 | 302 | 0 | 0 | 302 |
| cst816s | 312 | 0 | 0 | 312 |
| vl53l1x | 316 | 0 | 0 | 316 |
| mcp4725 | 318 | 0 | 0 | 318 |
| adxl345 | 330 | 0 | 0 | 330 |
| as5600 | 332 | 0 | 0 | 332 |
| ft6x36 | 334 | 0 | 0 | 334 |
| ads1115 | 336 | 0 | 0 | 336 |
| gpio_led | 328 | 0 | 0 | 328 |
| ws2812 | 340 | 0 | 0 | 340 |
| qmc5883p | 344 | 0 | 0 | 344 |
| sht30 | 346 | 0 | 0 | 346 |
| sgp40 | 352 | 0 | 0 | 352 |
| lis3mdl | 352 | 0 | 0 | 352 |
| drv2605 | 356 | 0 | 0 | 356 |
| ads1015 | 360 | 0 | 0 | 360 |
| ltr390 | 362 | 0 | 0 | 362 |
| dht11 | 364 | 0 | 0 | 364 |
| lm75 | 366 | 0 | 0 | 366 |
| tmp102 | 374 | 0 | 0 | 374 |
| mpr121 | 394 | 0 | 0 | 394 |
| opt3001 | 404 | 0 | 0 | 404 |
| a4988 | 407 | 0 | 0 | 407 |
| hmc5883l | 424 | 0 | 0 | 424 |
| ccs811 | 428 | 0 | 0 | 428 |
| tb6612 | 430 | 0 | 0 | 430 |
| bno055 | 434 | 0 | 0 | 434 |
| lps22hb | 438 | 0 | 0 | 438 |
| veml7700 | 438 | 0 | 0 | 438 |
| ina3221 | 444 | 0 | 0 | 444 |
| hx711 | 454 | 0 | 0 | 454 |
| htu21d | 462 | 0 | 0 | 462 |
| mlx90614 | 466 | 0 | 0 | 466 |
| stepper | 474 | 0 | 0 | 474 |
| keypad_matrix | 476 | 0 | 0 | 476 |
| qmi8658a | 488 | 0 | 0 | 488 |
| lis3dh | 496 | 0 | 0 | 496 |
| mcp23017 | 502 | 0 | 0 | 502 |
| mpu6050 | 506 | 0 | 0 | 506 |
| vl53l0x | 514 | 0 | 0 | 514 |
| at24cxx | 528 | 0 | 0 | 528 |
| aht20 | 530 | 0 | 0 | 530 |
| tsl2591 | 530 | 0 | 0 | 530 |
| bma400 | 540 | 0 | 0 | 540 |
| bmi160 | 548 | 0 | 0 | 548 |
| w25qxx | 560 | 0 | 0 | 560 |
| shtc3 | 574 | 0 | 0 | 574 |
| sht40 | 584 | 0 | 0 | 584 |
| max31865 | 592 | 0 | 0 | 592 |
| ina219 | 602 | 0 | 0 | 602 |
| hd44780 | 604 | 0 | 0 | 604 |
| scd40 | 618 | 0 | 0 | 618 |
| bmp280 | 620 | 0 | 0 | 620 |
| ina226 | 622 | 0 | 0 | 622 |
| is31fl3731 | 634 | 0 | 0 | 634 |
| pac1952 | 650 | 0 | 0 | 650 |
| lsm6dso | 660 | 0 | 0 | 660 |
| sh1106 | 684 | 0 | 0 | 684 |
| buzzer | 686 | 0 | 0 | 686 |
| sgp30 | 690 | 0 | 0 | 690 |
| ds3231 | 690 | 0 | 0 | 690 |
| icm42688 | 702 | 0 | 0 | 702 |
| bq27441 | 706 | 0 | 0 | 706 |
| max17048 | 712 | 0 | 0 | 712 |
| tcs34725 | 724 | 0 | 0 | 724 |
| oled_ssd1306 | 776 | 0 | 0 | 776 |
| st7735 | 788 | 0 | 0 | 788 |
| ili9341 | 798 | 0 | 0 | 798 |
| ds18b20 | 796 | 0 | 0 | 796 |
| si7021 | 806 | 0 | 0 | 806 |
| sd_spi | 816 | 0 | 0 | 816 |
| aw9523 | 816 | 0 | 0 | 816 |
| gc9a01 | 821 | 0 | 0 | 821 |
| lcd_st7789 | 851 | 0 | 0 | 851 |
| cc1101 | 862 | 0 | 0 | 862 |
| bmi270 | 956 | 0 | 0 | 956 |
| pca9685 | 960 | 0 | 0 | 960 |
| pcf8563 | 978 | 0 | 0 | 978 |
| dps310 | 988 | 0 | 0 | 988 |
| ms5611 | 993 | 0 | 0 | 993 |
| tm1637 | 1016 | 0 | 0 | 1016 |
| rc522 | 1088 | 0 | 0 | 1088 |
| max30102 | 1098 | 0 | 0 | 1098 |
| nrf24l01 | 1112 | 0 | 0 | 1112 |
| w5500 | 1184 | 0 | 0 | 1184 |
| enc28j60 | 1242 | 0 | 0 | 1242 |
| lora_sx1276 | 1242 | 0 | 0 | 1242 |
| mcp2515 | 1246 | 0 | 0 | 1246 |
| bmp388 | 1224 | 0 | 0 | 1224 |
| air780eg | 1277 | 0 | 0 | 1277 |
| sx1262 | 1328 | 0 | 0 | 1328 |
| bme280 | 1494 | 0 | 0 | 1494 |
| bme680 | 1838 | 0 | 0 | 1838 |
| bme688 | 1898 | 0 | 0 | 1898 |

## L2 — Protocols (47 wheels)

| Wheel | .text | .data | .bss | Total |
|-------|------:|------:|-----:|------:|
| ncp5623_parser | 156 | 0 | 0 | 156 |
| j1939 | 182 | 0 | 0 | 182 |
| ibus | 200 | 0 | 0 | 200 |
| dmx512 | 286 | 0 | 0 | 286 |
| crc_any | 298 | 0 | 0 | 298 |
| uds | 306 | 0 | 0 | 306 |
| lin_bus | 384 | 0 | 0 | 384 |
| isotp | 388 | 0 | 0 | 388 |
| tlv_codec | 400 | 0 | 0 | 400 |
| crsf | 418 | 0 | 0 | 418 |
| onewire_search | 434 | 0 | 0 | 434 |
| ubx_parser | 434 | 0 | 0 | 434 |
| modbus_tcp | 440 | 0 | 0 | 440 |
| lorawan_packet | 458 | 0 | 0 | 458 |
| xmodem | 498 | 0 | 0 | 498 |
| sbus | 506 | 0 | 0 | 506 |
| modbus_ascii | 527 | 0 | 0 | 527 |
| hdlc | 530 | 0 | 0 | 530 |
| intel_hex | 567 | 0 | 0 | 567 |
| midi_packet | 574 | 0 | 0 | 574 |
| websocket_frame | 578 | 0 | 0 | 578 |
| sntp_client | 582 | 0 | 0 | 582 |
| i2c_scan | 584 | 0 | 0 | 584 |
| mavlink | 586 | 0 | 0 | 586 |
| s_record | 655 | 0 | 0 | 655 |
| ota_ab | 650 | 0 | 0 | 650 |
| tftp | 666 | 0 | 0 | 666 |
| ymodem | 705 | 0 | 0 | 705 |
| http_request | 720 | 0 | 0 | 720 |
| at_command | 743 | 0 | 0 | 743 |
| ir_nec | 766 | 0 | 0 | 766 |
| coap_packet | 772 | 0 | 0 | 772 |
| dhcp_packet | 806 | 0 | 0 | 806 |
| dns_packet | 814 | 0 | 0 | 814 |
| canbus | 872 | 0 | 0 | 872 |
| json_rpc | 892 | 0 | 0 | 892 |
| bencode | 982 | 0 | 0 | 982 |
| protobuf_lite | 996 | 0 | 0 | 996 |
| http_parser | 1033 | 0 | 0 | 1033 |
| nmea_parser | 1093 | 0 | 0 | 1093 |
| mqtt_packet | 1098 | 0 | 0 | 1098 |
| ppp_frame | 1140 | 0 | 0 | 1140 |
| modbus_rtu | 1142 | 0 | 0 | 1142 |
| cbor_lite | 1142 | 0 | 0 | 1142 |
| syslog_packet | 1152 | 0 | 0 | 1152 |
| mqtt5_packet | 1594 | 0 | 0 | 1594 |
| msgpack | 1618 | 0 | 0 | 1618 |

## L3 — Modules (20 wheels)

| Wheel | .text | .data | .bss | Total |
|-------|------:|------:|-----:|------:|
| boot_counter | 210 | 0 | 0 | 210 |
| version_info | 353 | 0 | 0 | 353 |
| command_queue | 358 | 0 | 0 | 358 |
| power_manager | 368 | 0 | 0 | 368 |
| ring_log | 370 | 0 | 0 | 370 |
| task_watchdog | 404 | 0 | 0 | 404 |
| device_info | 426 | 0 | 0 | 426 |
| scheduler | 436 | 0 | 0 | 436 |
| error_handler | 446 | 0 | 0 | 446 |
| event_bus | 486 | 0 | 0 | 486 |
| ota_manager | 492 | 0 | 0 | 492 |
| sensor_hub | 552 | 0 | 0 | 552 |
| button_handler | 558 | 0 | 0 | 558 |
| param_table | 656 | 0 | 0 | 656 |
| data_logger | 687 | 0 | 0 | 687 |
| cli_shell | 767 | 0 | 0 | 767 |
| telemetry | 1114 | 0 | 0 | 1114 |
| mqtt_client | 1354 | 0 | 0 | 1354 |
| flash_journal | 1378 | 0 | 0 | 1378 |
| config_store | 1660 | 0 | 0 | 1660 |

## Summary

| Layer | Count | .text range | Median |
|-------|------:|-------------|-------:|
| L0 Primitives | 77 | 32 -- 6864 B | 320 B |
| L1 Drivers | 115 | 142 -- 1898 B | 502 B |
| L2 Protocols | 47 | 156 -- 1618 B | 586 B |
| L3 Modules | 20 | 210 -- 1660 B | 489 B |
| **Total compiled** | **259** | **32 -- 6864 B** | **434 B** |

**Total footprint: 157.5 KB .text + 1 B .data + 183 B .bss = 157.7 KB** (before --gc-sections).

### MCU fit guide

| MCU Flash | Fits (approx.) |
|-----------|---------------|
| 16 KB | Any single wheel + app overhead |
| 32 KB | 30+ typical wheels |
| 64 KB | 100+ wheels comfortably |
| 128 KB | All 259 wheels + app logic |
| 256 KB+ | All wheels + VSF kernel + app |
