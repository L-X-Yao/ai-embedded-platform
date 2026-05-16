# Component Catalog

**264** reusable C99 embedded components. Zero heap, zero OS dependencies,
dependency-injected via function pointers. Copy the source into your project,
include the header, wire the HAL callbacks.

| Layer | Directory | Count | Description |
|-------|-----------|------:|-------------|
| L0 Primitives | `primitives/` | 78 | Pure software utilities |
| L1 Drivers | `drivers/` | 116 | Hardware peripheral drivers |
| L2 Protocols | `protocols/` | 48 | Protocol parsers and builders |
| L3 Modules | `modules/` | 21 | Higher-level application modules |

All components are MIT-licensed, tested on host (gcc/clang), and
verified by a 63-check automated audit. See [wheel-contract.md](../docs/wheel-contract.md)
for quality criteria.

---

## L1 Drivers (116)

### Temperature and Humidity

| Component | Description |
|-----------|-------------|
| [sht30](drivers/sht30/) | Sensirion SHT30 temperature/humidity (I2C) |
| [sht40](drivers/sht40/) | Sensirion SHT40 temperature/humidity (I2C) |
| [si7021](drivers/si7021/) | Silicon Labs Si7021 temperature/humidity (I2C) |
| [aht20](drivers/aht20/) | ASAIR AHT20 temperature/humidity (I2C) |
| [dht11](drivers/dht11/) | DHT11 temperature/humidity (single-wire GPIO) |
| [tmp102](drivers/tmp102/) | TI TMP102 temperature sensor (I2C) |
| [ds18b20](drivers/ds18b20/) | Maxim DS18B20 temperature (1-Wire) |

### Pressure and Altitude

| Component | Description |
|-----------|-------------|
| [bmp280](drivers/bmp280/) | Bosch BMP280 barometric pressure/temperature (I2C) |
| [bme280](drivers/bme280/) | Bosch BME280 temperature/humidity/pressure (I2C) |
| [bme680](drivers/bme680/) | Bosch BME680 environmental sensor with gas (I2C) |
| [ms5611](drivers/ms5611/) | TE MS5611 precision barometer (I2C) |
| [lps22hb](drivers/lps22hb/) | ST LPS22HB barometric pressure (I2C) |
| [xgzp6816d](drivers/xgzp6816d/) | CFSensor XGZP6816D pressure sensor (I2C) |

### Light and Color

| Component | Description |
|-----------|-------------|
| [bh1750](drivers/bh1750/) | ROHM BH1750 ambient light sensor (I2C) |
| [veml7700](drivers/veml7700/) | Vishay VEML7700 ambient light sensor (I2C) |
| [opt3001](drivers/opt3001/) | TI OPT3001 ambient light sensor (I2C) |
| [tsl2591](drivers/tsl2591/) | AMS TSL2591 high-dynamic-range light sensor (I2C) |
| [tcs34725](drivers/tcs34725/) | ams TCS34725 color sensor (I2C) |
| [apds9960](drivers/apds9960/) | Broadcom APDS-9960 gesture/color/proximity (I2C) |

### Gas and Air Quality

| Component | Description |
|-----------|-------------|
| [sgp30](drivers/sgp30/) | Sensirion SGP30 TVOC/eCO2 air quality (I2C) |
| [sgp40](drivers/sgp40/) | Sensirion SGP40 VOC raw signal sensor (I2C) |
| [ccs811](drivers/ccs811/) | AMS CCS811 eCO2/TVOC gas sensor (I2C) |

### IMU and Accelerometer

| Component | Description |
|-----------|-------------|
| [mpu6050](drivers/mpu6050/) | InvenSense MPU-6050 6-axis IMU (I2C) |
| [qmi8658a](drivers/qmi8658a/) | QST QMI8658A 6-axis IMU (I2C) |
| [lis3dh](drivers/lis3dh/) | ST LIS3DH 3-axis accelerometer (I2C/SPI) |
| [icm42688](drivers/icm42688/) | TDK ICM-42688-P 6-axis IMU (SPI) |
| [bmi270](drivers/bmi270/) | Bosch BMI270 6-axis IMU (SPI) |
| [adxl345](drivers/adxl345/) | Analog Devices ADXL345 3-axis accelerometer (I2C) |
| [bno055](drivers/bno055/) | Bosch BNO055 9-axis IMU with sensor fusion (I2C) |

### Magnetometer

| Component | Description |
|-----------|-------------|
| [qmc5883p](drivers/qmc5883p/) | QST QMC5883P 3-axis magnetometer (I2C) |
| [hmc5883l](drivers/hmc5883l/) | Honeywell HMC5883L 3-axis magnetometer (I2C) |

### Health

| Component | Description |
|-----------|-------------|
| [max30102](drivers/max30102/) | Maxim MAX30102 pulse oximeter/heart rate (I2C) |

### Display

| Component | Description |
|-----------|-------------|
| [oled_ssd1306](drivers/oled_ssd1306/) | Solomon SSD1306 128x64 OLED (I2C) |
| [sh1106](drivers/sh1106/) | SH1106 132x64 OLED display (I2C) |
| [lcd_st7789](drivers/lcd_st7789/) | ST7789 240x320 TFT LCD (SPI, RGB565) |
| [gc9a01](drivers/gc9a01/) | GC9A01 240x240 round TFT LCD (SPI, RGB565) |
| [ili9341](drivers/ili9341/) | ILI9341 240x320 TFT LCD (SPI, RGB565) |
| [max7219](drivers/max7219/) | Maxim MAX7219 LED matrix/7-segment (SPI) |
| [tm1637](drivers/tm1637/) | TM1637 4-digit 7-segment LED display (bit-bang) |
| [hd44780](drivers/hd44780/) | HD44780 character LCD via PCF8574 I2C backpack |

### Storage

| Component | Description |
|-----------|-------------|
| [w25qxx](drivers/w25qxx/) | Winbond W25Q series SPI NOR flash |
| [at24cxx](drivers/at24cxx/) | Atmel AT24C series EEPROM (I2C) |
| [sd_spi](drivers/sd_spi/) | SD card SPI-mode driver |

### Communication

| Component | Description |
|-----------|-------------|
| [nrf24l01](drivers/nrf24l01/) | Nordic nRF24L01+ 2.4 GHz transceiver (SPI) |
| [lora_sx1276](drivers/lora_sx1276/) | Semtech SX1276 LoRa transceiver (SPI) |
| [sx1262](drivers/sx1262/) | Semtech SX1262 LoRa transceiver (SPI) |
| [cc1101](drivers/cc1101/) | TI CC1101 sub-GHz transceiver (SPI) |
| [enc28j60](drivers/enc28j60/) | Microchip ENC28J60 Ethernet controller (SPI) |
| [w5500](drivers/w5500/) | Wiznet W5500 SPI Ethernet controller |
| [mcp2515](drivers/mcp2515/) | Microchip MCP2515 CAN controller (SPI) |
| [air780eg](drivers/air780eg/) | Quectel Air780EG 4G/GNSS AT modem |

### Input

| Component | Description |
|-----------|-------------|
| [gpio_button](drivers/gpio_button/) | GPIO button with debounce/long-press/multi-click |
| [rotary_encoder](drivers/rotary_encoder/) | Quadrature rotary encoder (GPIO) |
| [keypad_matrix](drivers/keypad_matrix/) | Matrix keypad scanner (GPIO) |
| [as5600](drivers/as5600/) | ams AS5600 magnetic rotary encoder (I2C) |
| [ft6x36](drivers/ft6x36/) | FocalTech FT6236/FT6336 capacitive touch (I2C) |

### Output

| Component | Description |
|-----------|-------------|
| [gpio_led](drivers/gpio_led/) | GPIO LED control with blink/breathe patterns |
| [ws2812](drivers/ws2812/) | WorldSemi WS2812 addressable RGB LED (bit-bang) |
| [sk6812](drivers/sk6812/) | WorldSemi SK6812 addressable RGBW LED (bit-bang) |
| [apa102](drivers/apa102/) | APA102 DotStar addressable RGB LED (SPI) |
| [servo](drivers/servo/) | PWM servo with angle-to-pulse mapping |
| [stepper](drivers/stepper/) | Stepper motor driver (DIR+STEP GPIO) |
| [pca9685](drivers/pca9685/) | NXP PCA9685 16-channel PWM driver (I2C) |
| [drv8833](drivers/drv8833/) | TI DRV8833 dual H-bridge motor driver (GPIO+PWM) |
| [drv2605](drivers/drv2605/) | TI DRV2605 haptic feedback driver (I2C) |
| [mcp4725](drivers/mcp4725/) | Microchip MCP4725 12-bit DAC (I2C) |
| [buzzer](drivers/buzzer/) | Passive buzzer with PWM frequency/melody playback |
| [tb6612](drivers/tb6612/) | Toshiba TB6612FNG dual H-bridge motor driver (GPIO+PWM) |
| [a4988](drivers/a4988/) | Allegro A4988 stepper motor driver (GPIO) |

### I/O Expander

| Component | Description |
|-----------|-------------|
| [mcp23017](drivers/mcp23017/) | Microchip MCP23017 16-bit I/O expander (I2C) |
| [pcf8574](drivers/pcf8574/) | NXP PCF8574 8-bit I/O expander (I2C) |
| [tca9548a](drivers/tca9548a/) | TI TCA9548A I2C multiplexer |
| [aw9523](drivers/aw9523/) | AW9523B 16-bit I/O expander + LED driver (I2C) |

### Power and Current Monitoring

| Component | Description |
|-----------|-------------|
| [ina219](drivers/ina219/) | TI INA219 current/power monitor (I2C) |
| [ina226](drivers/ina226/) | TI INA226 precision current/power monitor (I2C) |
| [max17048](drivers/max17048/) | MAX17048 LiPo fuel gauge (I2C) |
| [bq27441](drivers/bq27441/) | TI BQ27441-G1 battery fuel gauge (I2C) |
| [pac1952](drivers/pac1952/) | Microchip PAC1952 dual-channel power monitor (I2C) |

### ADC

| Component | Description |
|-----------|-------------|
| [ads1115](drivers/ads1115/) | TI ADS1115 16-bit 4-channel ADC (I2C) |
| [ads1015](drivers/ads1015/) | TI ADS1015 12-bit 4-channel ADC (I2C) |

### RTC

| Component | Description |
|-----------|-------------|
| [ds3231](drivers/ds3231/) | Maxim DS3231 high-precision RTC (I2C) |
| [pcf8563](drivers/pcf8563/) | NXP PCF8563 RTC (I2C) |

### RFID

| Component | Description |
|-----------|-------------|
| [rc522](drivers/rc522/) | NXP MFRC522 RFID/NFC reader (SPI) |

### Distance and Proximity

| Component | Description |
|-----------|-------------|
| [vl53l0x](drivers/vl53l0x/) | ST VL53L0X time-of-flight distance sensor (I2C) |

### IR Temperature

| Component | Description |
|-----------|-------------|
| [mlx90614](drivers/mlx90614/) | Melexis MLX90614 infrared thermometer (I2C) |

### Thermocouple

| Component | Description |
|-----------|-------------|
| [max31855](drivers/max31855/) | Maxim MAX31855 thermocouple-to-digital (SPI) |
| [max31865](drivers/max31865/) | Maxim MAX31865 RTD-to-digital (SPI) |

### Load Cell

| Component | Description |
|-----------|-------------|
| [hx711](drivers/hx711/) | Avia HX711 24-bit load cell ADC (GPIO) |

---

## L0 Primitives (78)

### Logging

| Component | Description |
|-----------|-------------|
| [log](primitives/log/) | Leveled logging with module tags, multi-sink, compile-time filtering |
| [log_color](primitives/log_color/) | ANSI color log sink |
| [log_dedup](primitives/log_dedup/) | Deduplicating log sink |
| [log_file](primitives/log_file/) | FILE* log sink |
| [log_filter](primitives/log_filter/) | Per-module filter log sink |
| [log_flash](primitives/log_flash/) | Flash-persistent log sink |
| [log_rate](primitives/log_rate/) | Token-bucket rate-limited log sink |
| [log_ringbuf](primitives/log_ringbuf/) | RAM ring-buffer log sink |
| [log_rtt](primitives/log_rtt/) | SEGGER RTT log sink |
| [log_uart](primitives/log_uart/) | UART log sink |

### CRC

| Component | Description |
|-----------|-------------|
| [crc8](primitives/crc8/) | CRC-8 checksum (table-based) |
| [crc16](primitives/crc16/) | CRC-16/MODBUS checksum (table-based) |
| [crc32](primitives/crc32/) | CRC-32 IEEE 802.3 (table-based) |

### Data Structures

| Component | Description |
|-----------|-------------|
| [ring_buffer](primitives/ring_buffer/) | Byte-level circular buffer |
| [fifo_queue](primitives/fifo_queue/) | Typed FIFO queue |
| [bitmap](primitives/bitmap/) | Bit manipulation (set/clear/test/find) |
| [byte_buffer](primitives/byte_buffer/) | Linear byte buffer with big-endian put/get |
| [event_queue](primitives/event_queue/) | SPSC event queue (lock-free volatile head/tail) |
| [double_buffer](primitives/double_buffer/) | Ping-pong double buffer for DMA/producer-consumer |

### Math and DSP

| Component | Description |
|-----------|-------------|
| [fixed_point](primitives/fixed_point/) | Fixed-point arithmetic (Q16.16) |
| [moving_average](primitives/moving_average/) | SMA sliding average + EMA exponential average |
| [moving_window](primitives/moving_window/) | Sliding window statistics (min/max/mean) |
| [median_filter](primitives/median_filter/) | Sliding-window median filter (impulse rejection) |
| [pid_controller](primitives/pid_controller/) | Discrete PID controller with anti-windup |
| [mahony_ahrs](primitives/mahony_ahrs/) | Mahony AHRS attitude estimator (6/9-axis) |
| [mag_cal](primitives/mag_cal/) | Magnetometer calibration (hard/soft iron) |
| [iir_lowpass](primitives/iir_lowpass/) | First-order IIR low-pass filter (EMA) |
| [kalman_1d](primitives/kalman_1d/) | 1D scalar Kalman filter |

### Encoding

| Component | Description |
|-----------|-------------|
| [base64](primitives/base64/) | Base64 encode/decode |
| [hex_codec](primitives/hex_codec/) | Hexadecimal encode/decode |
| [json_parser](primitives/json_parser/) | Zero-allocation JSON parser |
| [printf_tiny](primitives/printf_tiny/) | Minimal printf (no float, no heap) |
| [cobs](primitives/cobs/) | Consistent Overhead Byte Stuffing codec |
| [slip](primitives/slip/) | SLIP serial line framing (RFC 1055) |
| [rle_codec](primitives/rle_codec/) | Run-length encoding/decoding |

### Timing

| Component | Description |
|-----------|-------------|
| [soft_timer](primitives/soft_timer/) | Software timer manager (one-shot/repeat) |
| [timeout](primitives/timeout/) | Deadline tracker (32-bit tick wraparound-safe) |
| [retry](primitives/retry/) | Retry with fixed/linear/exponential backoff |
| [debounce](primitives/debounce/) | Software debounce (rising/falling edge) |
| [timer_wheel](primitives/timer_wheel/) | Hashed timing wheel for many timers |
| [rate_limiter](primitives/rate_limiter/) | Token-bucket rate limiter |

### State Management

| Component | Description |
|-----------|-------------|
| [state_machine](primitives/state_machine/) | Table-driven finite state machine |
| [led_pattern](primitives/led_pattern/) | LED blink pattern engine (one-shot/loop) |

### Crypto

| Component | Description |
|-----------|-------------|
| [aes128](primitives/aes128/) | AES-128 symmetric encryption |
| [sha256](primitives/sha256/) | SHA-256 hash |
| [hmac_sha256](primitives/hmac_sha256/) | HMAC-SHA256 message authentication (RFC 2104) |

### Storage Abstraction

| Component | Description |
|-----------|-------------|
| [fal](primitives/fal/) | Flash abstraction layer (FAL-compatible API) |
| [kvs](primitives/kvs/) | Key-value store (vendor: FlashDB KVDB) |
| [flash_ring](primitives/flash_ring/) | Flash-backed ring FIFO with persistent head pointer |

### Soft Peripherals

| Component | Description |
|-----------|-------------|
| [soft_i2c](primitives/soft_i2c/) | GPIO bit-bang I2C master with bus recovery |
| [soft_spi](primitives/soft_spi/) | GPIO bit-bang SPI master (mode 0-3, MSB/LSB) |
| [onewire](primitives/onewire/) | 1-Wire bus protocol (Dallas/Maxim) |

### Memory

| Component | Description |
|-----------|-------------|
| [memory_pool](primitives/memory_pool/) | Fixed-size block allocator (O(1) alloc/free) |

### GNSS

| Component | Description |
|-----------|-------------|
| [gnss_filter](primitives/gnss_filter/) | GNSS position EMA filter with quality gating + ZUPT |

### Font

| Component | Description |
|-----------|-------------|
| [font_5x7](primitives/font_5x7/) | 5x7 pixel bitmap font (ASCII 0x20-0x7E) |

### Utility

| Component | Description |
|-----------|-------------|
| [uuid](primitives/uuid/) | UUID generation and formatting |
| [endian](primitives/endian/) | Byte-order conversion utilities |
| [hash_fnv](primitives/hash_fnv/) | FNV-1a hash |
| [string_utils](primitives/string_utils/) | Embedded-safe string utility functions |
| [lookup_table](primitives/lookup_table/) | Lookup table with linear interpolation |
| [checksum](primitives/checksum/) | Checksum collection (XOR/sum8/Fletcher-16/LRC/Internet) |
| [bitfield](primitives/bitfield/) | Bitfield pack/unpack (MSB-first) |
| [cli_parser](primitives/cli_parser/) | Minimal CLI parser for debug shells |

---

## L2 Protocols (48)

### IoT

| Component | Description |
|-----------|-------------|
| [mqtt_packet](protocols/mqtt_packet/) | MQTT 3.1.1 stateless serialize/deserialize |
| [mqtt5_packet](protocols/mqtt5_packet/) | MQTT 5.0 packet codec |
| [coap_packet](protocols/coap_packet/) | CoAP message codec (RFC 7252) |
| [http_parser](protocols/http_parser/) | HTTP/1.1 zero-copy request/response parser |
| [http_request](protocols/http_request/) | HTTP/1.1 request builder (zero-allocation) |
| [websocket_frame](protocols/websocket_frame/) | WebSocket frame codec (RFC 6455) |

### Industrial

| Component | Description |
|-----------|-------------|
| [modbus_rtu](protocols/modbus_rtu/) | Modbus RTU master/slave frame codec |
| [modbus_tcp](protocols/modbus_tcp/) | Modbus TCP frame codec |
| [canbus](protocols/canbus/) | CAN bus frame encode/decode |

### Serial Transfer

| Component | Description |
|-----------|-------------|
| [at_command](protocols/at_command/) | AT command parser (URC + timeout + multi-match) |
| [nmea_parser](protocols/nmea_parser/) | NMEA 0183 GPS/GNSS sentence parser |
| [ubx_parser](protocols/ubx_parser/) | u-blox UBX binary GNSS protocol parser/builder |
| [xmodem](protocols/xmodem/) | XMODEM file transfer protocol |
| [ymodem](protocols/ymodem/) | YMODEM file transfer protocol |

### Network

| Component | Description |
|-----------|-------------|
| [dns_packet](protocols/dns_packet/) | DNS packet codec |
| [dhcp_packet](protocols/dhcp_packet/) | DHCP packet builder/parser (RFC 2131) |
| [sntp_client](protocols/sntp_client/) | SNTP v4 time sync codec (RFC 4330) |
| [ppp_frame](protocols/ppp_frame/) | PPP HDLC-like framing codec (RFC 1662) |
| [tftp](protocols/tftp/) | TFTP file transfer codec (RFC 1350) |

### Encoding Formats

| Component | Description |
|-----------|-------------|
| [protobuf_lite](protocols/protobuf_lite/) | Protocol Buffers lightweight codec |
| [cbor_lite](protocols/cbor_lite/) | CBOR lightweight codec (RFC 7049) |
| [msgpack](protocols/msgpack/) | MessagePack encode/decode |
| [tlv_codec](protocols/tlv_codec/) | TLV (Tag-Length-Value) codec |
| [bencode](protocols/bencode/) | Bencode codec (BitTorrent format) |
| [json_rpc](protocols/json_rpc/) | JSON-RPC 2.0 frame builder/validator |

### Domain-Specific

| Component | Description |
|-----------|-------------|
| [lorawan_packet](protocols/lorawan_packet/) | LoRaWAN packet codec |
| [mavlink](protocols/mavlink/) | MAVLink v2 frame parser/serializer |
| [sbus](protocols/sbus/) | Futaba S.Bus RC receiver protocol codec |
| [midi_packet](protocols/midi_packet/) | MIDI message codec |
| [dmx512](protocols/dmx512/) | DMX512 lighting protocol data layer |
| [syslog_packet](protocols/syslog_packet/) | Syslog packet codec (RFC 5424) |
| [ncp5623_parser](protocols/ncp5623_parser/) | NCP5623 I2C LED driver protocol parser |
| [i2c_scan](protocols/i2c_scan/) | I2C bus scanner utility |
| [ota_ab](protocols/ota_ab/) | A/B firmware slot management with rollback |
| [crc_any](protocols/crc_any/) | Generic CRC calculator (configurable polynomial/width) |

---

## L3 Modules (21)

| Component | Description |
|-----------|-------------|
| [cli_shell](modules/cli_shell/) | Command-line shell with command registration and history |
| [data_logger](modules/data_logger/) | Sensor data acquisition, storage, and upload pipeline |
| [sensor_hub](modules/sensor_hub/) | Multi-sensor unified polling and aggregation |
| [config_store](modules/config_store/) | Persistent configuration storage |
| [error_handler](modules/error_handler/) | Error handling framework (error codes + callbacks) |
| [device_info](modules/device_info/) | Device information query (model/version/serial) |
| [task_watchdog](modules/task_watchdog/) | Multi-task watchdog monitor |
| [ota_manager](modules/ota_manager/) | OTA firmware update manager |
| [telemetry](modules/telemetry/) | Telemetry data packaging and reporting |
| [button_handler](modules/button_handler/) | Multi-button event handler with combo detection |
| [scheduler](modules/scheduler/) | Cooperative task scheduler |
| [param_table](modules/param_table/) | Runtime-tunable parameter table |
| [ring_log](modules/ring_log/) | Fixed-size RAM ring log |
| [event_bus](modules/event_bus/) | Publish/subscribe event bus |
| [flash_journal](modules/flash_journal/) | Append-only flash journal with wear leveling |
| [command_queue](modules/command_queue/) | Asynchronous command dispatch queue |
| [boot_counter](modules/boot_counter/) | Boot counter with flash persistence |
| [version_info](modules/version_info/) | Compile-time version information |
| [mqtt_client](modules/mqtt_client/) | MQTT 3.1.1 stateful client (feed/poll, no OS required) |
| [power_manager](modules/power_manager/) | Cooperative sleep/wake orchestrator (no OS required) |

---

## Using a Component

Every component follows the same pattern:

```c
// 1. Copy the component directory into your project
//    e.g. wheels/drivers/bmp280/ -> your_project/lib/bmp280/

// 2. Include the header
#include "bmp280.h"

// 3. Wire your platform HAL via the init struct
bmp280_t sensor;
bmp280_cfg_t cfg = {
    .i2c_write = my_i2c_write,   // your platform's I2C write
    .i2c_read  = my_i2c_read,    // your platform's I2C read
    .delay_ms  = my_delay_ms,    // your platform's delay
};
bmp280_init(&sensor, &cfg);

// 4. Use it
bmp280_data_t data;
bmp280_read(&sensor, &data);
```

Components never call `malloc`, never create threads, and never touch
hardware directly. All platform interaction goes through function pointers
you provide at init time.

For CMake integration details, see [integrating-wheels.md](../docs/integrating-wheels.md).
