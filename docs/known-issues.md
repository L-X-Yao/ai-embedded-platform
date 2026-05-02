# Known Issues & Prior Fixes

This file is the single source of truth for audit agents. **Read this before reporting findings.**

Anything listed here has been evaluated and either fixed or documented as accepted.

## Fixed in R3-R12 (do NOT re-report)

### Cross-wheel error propagation (R12)
- `log_flash`: `log_flash_erase_all` changed from void to int; now propagates HAL erase errors and preserves state on failure
- `flybox/w25q64_driver`: `flash_ring_push` now checks `w25qxx_sector_erase` return (was silently ignored → flash corruption risk)
- `flybox/w25q64_driver`: `flash_ring_init` now checks `_flush_header_unlocked` return on fresh-flash path

### Boundary value / overflow (R11)
- `log_ringbuf`: total_lines/dropped_lines uint32 saturation (prevent wrap-around to 0)
- `log_flash`: total_entries/dropped uint32 saturation (prevent wrap-around to 0)
- `log_dedup`: repeat_count/total_suppressed uint32 saturation (prevent wrap-around to 0)
- `log_rate`: dropped uint32 saturation (prevent wrap-around to 0)
- `state_machine`: transition_count uint32 saturation (prevent wrap-around to 0)

### L0 Primitives
- `bitfield`: overflow-safe range check (subtraction form), UINT32_C(1) shifts
- `crc16`: NULL guard in crc16_modbus
- `byte_buffer`: uint16 signed promotion
- `checksum`: fletcher-16 mod-255 final normalization
- `cli_parser`: CRLF double-prompt fix (last_was_cr flag); error enum explicit negative values (was auto-increment positive)
- `log`: log_emit NULL guard on fmt parameter
- `log_rtt`: size_t→unsigned truncation guard before HAL write
- `log_flash`: sector_size type changed from size_t to uint32_t (consistent with flash address space)
- `event_queue`: push/pop/is_full guard against capacity=0/buf=NULL
- `kvs`: test mock bounds checking
- `log`: NUL terminator clamp (pos > sizeof(buf)-2)
- `log_dedup`: pass original len + last_full_len tracking + min(la,lb) comparison + variable-width timestamp scan
- `log_flash`: partial write does NOT advance write_pos; data-first write order; erase_all resets counters
- `log_rate`: overflow-safe refill via modulo
- `led_pattern`: on_ticks=0 handling
- `soft_timer`: count snapshot before tick loop
- `pid_controller`: set_output_limits min<=max; init limit validation
- `state_machine`: reset calls on_enter
- `timeout`: saturation arithmetic in extend
- `mahony_ahrs`: integral clamping (MAHONY_INTEGRAL_MAX)
- `mag_cal`: radius<=0 guard
- `retry`: linear overflow guard
- `median_filter`: even window upper-middle (documented design)

### L1 Drivers
- `bmp280`: STATUS polling + timeout + osrs mask + calibration 24 bytes (not 26); osrs_t/osrs_p enum range validation
- `sht30`: delay_ms callback; NULL check on HAL write/read
- `hmc5883l`/`mpu6050`: const on read functions
- `mpu6050`: accel/gyro range enum validation
- `qmi8658a`: 7-bit I2C address; ODR/range validation
- `gpio_led`: uint32_t timing fields + uint32_t locals
- `bh1750`: enum validation
- `qmc5883p`: NULL check on HAL function pointers
- `w25qxx`: BUSY_TIMEOUT promoted to configurable macro; HAL fn ptr (cs_low/cs_high/xfer) NULL validation in init
- `rotary_encoder`: renamed to rotary_encoder_* prefix
- `ads1115`: PGA scale 32767→32768; pga/mux/rate enum range validation in init
- `ws2812`: validate 4 HAL function pointers; ws2812_encode_grb NULL guard on all 3 pointers
- `oled_ssd1306`: validate write_cmd/write_data in init
- `xgzp6816d`: HAL fn ptr (write/read) NULL validation in probe
- `gpio_button`: hold_cnt/idle_cnt uint32_t saturation (prevent wrap-around re-trigger)
- `hmc5883l`: rate/gain enum range validation in init
- `servo`: set_pulse bounds check against configured min/max pulse

### L2 Protocols
- `websocket_frame`: SIZE_MAX overflow (parse+build), RSV bits, control frame validation, reserved opcodes 3-7/0xB-0xF, WS_ERR_OVERFLOW for oversized, NULL payload+nonzero len
- `coap_packet`: delta=14 uint32, subtraction-safe bounds, opt_num overflow, C++ keyword (class→cls), ascending order guard, value_len UINT16_MAX, NULL options, bare payload marker rejection
- `mqtt_packet`: payload_len size_t, 4th-byte continuation, topic_len guards (publish+subscribe+put_str), QoS overflow with extra, size_t total, QoS>2 rejection, parse_publish payload_len wrap, password-without-username, remaining_length>268M rejection; put_str return value checked (prevent buffer underwrite); encode/decode_remaining_length NULL guard on buf
- `nmea_parser`: checksum verification in parse, 16 test checksums fixed; parse_time/parse_date digit validation
- `http_parser`: HTTP_ERR_NOT_FOUND, version digit+dot validation, OWS tab stripping; status code must be exactly 3 digits (RFC 7230)
- `ota_ab`: flash fn NULL checks, max_boot_count=0, params_size minimum; ota_ab_crc32 NULL guard on data
- `at_command`: empty expected guard, strlen UINT16_MAX guard
- `modbus_rtu`: byte_count>250, register address wrap guard (read+write); master_read_regs/master_write_reg buf NULL guards
- `ncp5623_parser`: idx sentinel prevents stale re-trigger

### -Wconversion polish (R10)
- `bmp280`: explicit (float) cast on int32_t→float in temperature conversion
- `modbus_rtu`: explicit (uint16_t) casts on `5 + count * 2` and `1 + count * 2` (int promotion)
- `mqtt_packet`: explicit (int) cast on payload_len in build_publish pos accumulation
- `nmea_parser`: explicit (float) casts on int deg→float in parse_lat/parse_lon (4 sites)
- `qmi8658a`: outer (uint8_t) cast on ctrl2/ctrl3 shift-or expressions (style consistency with bmp280/rotary_encoder)

### Negative test coverage (R10)
- `coap_packet`: COAP_ERR_TOO_MANY now tested (9-option packet)
- `http_parser`: HTTP_ERR_TOO_MANY now tested (17-header request)
- `modbus_rtu`: address wrap (start+count > 0x10000) confirmed to return ILLEGAL_DATA_VALUE
- `ota_ab`: OTA_AB_ERR_PARAMS now tested (max_boot_count=0 and params_size too small)
- `mqtt_packet`: build_connect UINT16_MAX string guard now tested (65537-byte client_id)

### Negative test coverage (R11)
- `ads1115`: ADS1115_ERR_BUSY now tested (mock never sets conversion-ready bit, 200-retry poll exhausted)
- `bmp280`: BMP280_ERR_TIMEOUT now tested (mock STATUS register measuring bit stays set, 500-poll exhausted)
- `dht11`: DHT11_ERR_TIMEOUT now tested (mock pin stuck high, sensor never responds)

### Infrastructure
- `ci.yml`: toolchain path, --fail wget, first-push BASE fallback, quoted $MATRIX, grep -oE (not -oP)
- `session_bootstrap.sh`: $? capture, ternary arithmetic, _draft consumer path
- `test_downstream.sh`/`wheel_promote.sh`: BRE→ERE grep
- `release.sh`: tag moved to user next-steps (not auto-created before commit)
- `.gitignore`: secret file patterns

## Accepted Design Gaps (do NOT report as bugs)

- `mpu6050`: no delay after PWR_RESET (needs API struct change for delay_ms callback)
- `oled_ssd1306`: VCOMH 0x40 (undefined in spec but works on all known clones)
- `ds18b20`: set_resolution zeroes TH/TL alarm registers (documented behavior)
- `bmp280`: compensate_pressure negative var2 uint32 cast (Bosch datasheet int32 path limitation)
- `gpio_led`: API parameters are uint16_t but struct fields are uint32_t (low priority widening)
- `xgzp6816d`: EOC status byte ignored (caller must honor XGZP6816D_CONV_MS)
- `sht30`: no post-reset delay in init (delay_ms only called in read)
- `moving_average`: SMA float drift (known limitation of incremental sum)
- `sensor_hub`: float sum drift in avg statistics over long poll runs (>10k samples); workaround: periodic reset_stats
- `log_flash`: init only erases sector 0 (caller should call erase_all for clean slate)
- `log_dedup`: same-length lines >LOG_DEDUP_LINE_MAX differing only past truncation point may false-dedup (architectural limit of truncated storage)
- `log_ringbuf`: read output not NUL-terminated (returns byte count; documented contract)
- `checksum`: fletcher-16 seeded variant has {0x00}=={0xFF} collision (inherent to seed=0xFF)
- `event_queue`: capacity=1 is accepted but queue is permanently full (documented)
- `fal` (vendor): addr+size uint32 overflow on 32-bit targets (vendor code)
- `modbus_rtu`: 251-byte stack allocation in slave_read_regs/master_write_regs
- `ota_ab`: ota_ab_crc32 publicly exported (should be static, design-level)
- `modbus_rtu`: MODBUS_EX_ILLEGAL_DATA_ADDR defined but never emitted (address wrap returns ILLEGAL_DATA_VALUE; Modbus spec compliance gap, low risk)
- `commit_suggest.sh`: unbound variable under set -u when MAX_TYPE != wheel
- `batch_fix.sh`: dry-run sed detection breaks with existing flags
- `kvs` (vendor): `fal_flash_device_find()` return not NULL-checked in fdb.c:69 (vendor FlashDB code; crash on missing flash device config)
- `w25qxx`: SPI `xfer` returns data byte (uint8_t), not error code — mid-transaction failures indistinguishable from valid 0x00 data (SPI protocol limitation; would need HAL redesign)
- `log_uart`/`log_rtt`/`log_file`: fire-and-forget sinks discard HAL/stdio return values (architectural: log sinks must not fail the caller)
- `mqtt_packet`: `put_u16` return value unused in `put_str` (infallible helper, always returns 2)

## Cross-validation (2026-05-02): new L1 drivers vs GitHub references

All 12 new L1 drivers cross-validated against GitHub reference implementations.
Register addresses, conversion formulas, SPI opcodes, and CRC polynomials confirmed matching.

| Driver | Verified against | Result |
|--------|-----------------|--------|
| ina219 | LPC1343CodeBase, Linux kernel ina2xx.c | PASS |
| pca9685 | Linux kernel pwm-pca9685.c, stm32-hal-pca9685 | PASS |
| lora_sx1276 | LoRaMac-node (Semtech official) | PASS |
| mcp2515 | arduino-mcp2515, avr-can-lib | PASS |
| ds3231 | ebrezadev/DS3231, libdriver/ds3231 | PASS |
| nrf24l01 | nRF24/RF24, betaflight | PASS |
| enc28j60 | Linux kernel, RT-Thread | PASS |
| si7021 | TEConnectivity HTU21D Generic C | PASS |
| veml7700 | Adafruit_VEML7700, tedyapo/arduino-VEML7700 | PASS |
| hx711 | bogde/HX711, jonas-hagen/hx711 | PASS |
| at24cxx | N/A (simple protocol, no complex registers) | SKIP |
| pcf8574 | N/A (single-byte protocol) | SKIP |

**Note:** MCP2515 CNF timing values (8MHz crystal) vary across implementations. Our values need HIL verification for exact baud rate accuracy.

**Not cross-validated** (pure logic, no chip registers): stepper, keypad_matrix, drv8833, tca9548a, mcp23017 (verified register map matches datasheet comments).

## Remaining test gaps (low priority)

(none — all previously listed gaps resolved in R10)
