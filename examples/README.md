# Integration Examples

27 runnable examples demonstrating wheel composition patterns. Each example combines 2-8 wheels into a realistic embedded scenario with `main.c` + `CMakeLists.txt` ready for host simulation.

## By Domain

### Sensors & Environment

| Example | Description | Key Wheels |
|---------|-------------|------------|
| [air_quality](air_quality/) | Six environmental sensors with cross-validation and AQI computation | ccs811, sgp40, bme680, sht40, aht20, veml7700 |
| [env_monitor](env_monitor/) | Temperature/humidity/pressure polling with CSV logging | bmp280, sht30, sensor_hub, data_logger |
| [filter_comparison](filter_comparison/) | Compare median, IIR, and Kalman filters on noisy data | median_filter, iir_lowpass, kalman_1d |
| [sensor_dashboard](sensor_dashboard/) | Structured log output from BMP280 + SHT30 | log, bmp280, sht30 |

### Communication & Networking

| Example | Description | Key Wheels |
|---------|-------------|------------|
| [mqtt_telemetry_demo](mqtt_telemetry_demo/) | MQTT client lifecycle with mock TCP transport | bmp280, sensor_hub, mqtt_packet, mqtt_client |
| [remote_telemetry](remote_telemetry/) | JSON + MQTT + AT command transport pipeline | mqtt_packet, ring_buffer, at_command, crc16, json_parser |
| [lora_sensor_node](lora_sensor_node/) | COBS-framed accelerometer data over LoRa | adxl345, sx1262, cobs |
| [wireless_mesh](wireless_mesh/) | Dual-radio mesh (nRF24 + LoRa) with compression | slip, nrf24l01, lora_sx1276, lorawan_packet |
| [modbus_slave](modbus_slave/) | Sensor readings exposed as Modbus RTU registers | bmp280, modbus_rtu |
| [can_gateway](can_gateway/) | CAN bus gateway with ISO-TP and J1939 decoding | mcp2515, canbus, isotp, j1939 |

### Display & UI

| Example | Description | Key Wheels |
|---------|-------------|------------|
| [oled_display](oled_display/) | Sensor readings on 128x64 OLED with heap-free formatting | printf_tiny, font_5x7, oled_ssd1306, bmp280 |
| [display_hub](display_hub/) | Six display types: OLED, LCD, 7-segment, LED matrix, TFTs | sh1106, hd44780, tm1637, max7219, lcd_st7789, ili9341 |

### Motor & Motion

| Example | Description | Key Wheels |
|---------|-------------|------------|
| [motor_control](motor_control/) | Stepper via A4988 with UART CLI commands | a4988, cli_parser, state_machine |
| [motion_control_adv](motion_control_adv/) | Multi-actuator with IMU feedback, servos, haptics | mpu6050, servo, stepper, pca9685, drv8833, drv2605 |

### Power & Industrial

| Example | Description | Key Wheels |
|---------|-------------|------------|
| [power_monitor](power_monitor/) | Battery fuel gauge + current measurement + alerts | max17048, ina226, gpio_button, power_manager |
| [lowpower_sensor_node](lowpower_sensor_node/) | Duty-cycling with RTC wake, motion detect, battery gauge | max17048, ds3231, adxl345, bme280, power_manager |
| [industrial_monitor](industrial_monitor/) | CAN bus + PID control + voltage/current + task watchdog | pid_controller, ina219, ads1115, canbus, task_watchdog |

### Storage & Security

| Example | Description | Key Wheels |
|---------|-------------|------------|
| [storage_pipeline](storage_pipeline/) | Sensor-to-flash pipeline with KVS and FIFO buffering | fal, fifo_queue, byte_buffer, hash_fnv, at24cxx |
| [secure_ota](secure_ota/) | Encrypted OTA with SHA256 + AES128 + boot counter | aes128, sha256, hmac_sha256, ota_manager, flash_journal |
| [crypto_pipeline](crypto_pipeline/) | Hash, sign, encrypt, encode a sensor payload | sha256, hmac_sha256, aes128, base64, hex_codec |
| [smart_lock](smart_lock/) | RFID + PIN multi-factor lock with encrypted audit log | aes128, rc522, cst816s, buzzer, flash_journal |

### System & Shell

| Example | Description | Key Wheels |
|---------|-------------|------------|
| [device_shell](device_shell/) | Configuration terminal with CLI, config store, params | cli_shell, config_store, param_table, version_info |
| [command_queue_demo](command_queue_demo/) | ASCII command dispatcher with UART buffering | ring_buffer, cli_parser, string_utils, command_queue |
| [device_info_demo](device_info_demo/) | Boot banner with device identity and version | log, uuid, version_info, device_info |
| [timer_retry](timer_retry/) | Software timers + exponential backoff retry | soft_timer, retry |

### IoT & Smart Home

| Example | Description | Key Wheels |
|---------|-------------|------------|
| [smart_home](smart_home/) | Environmental sensors + RGB LEDs + scheduler + event bus | bme280, bh1750, ws2812, cbor_lite, scheduler, event_bus |
| [gps_tracker](gps_tracker/) | GPS + RTC + SD card logging with Kalman filtering | kalman_1d, ds3231, sd_spi, sntp_client, ubx_parser |
