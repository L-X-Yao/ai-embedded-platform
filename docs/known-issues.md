# Known Issues & Prior Fixes

This file is the single source of truth for audit agents. **Read this before reporting findings.**

Anything listed here has been evaluated and either fixed or documented as accepted.

## Fixed in R3-R8 (do NOT re-report)

### L0 Primitives
- `bitfield`: overflow-safe range check (subtraction form), UINT32_C(1) shifts
- `crc16`: NULL guard in crc16_modbus
- `byte_buffer`: uint16 signed promotion
- `checksum`: fletcher-16 mod-255 final normalization
- `cli_parser`: CRLF double-prompt fix (last_was_cr flag)
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
- `bmp280`: STATUS polling + timeout + osrs mask + calibration 24 bytes (not 26)
- `sht30`: delay_ms callback; NULL check on HAL write/read
- `hmc5883l`/`mpu6050`: const on read functions
- `mpu6050`: accel/gyro range enum validation
- `qmi8658a`: 7-bit I2C address; ODR/range validation
- `gpio_led`: uint32_t timing fields + uint32_t locals
- `bh1750`: enum validation
- `qmc5883p`: NULL check on HAL function pointers
- `w25qxx`: BUSY_TIMEOUT promoted to configurable macro
- `rotary_encoder`: renamed to rotary_encoder_* prefix
- `ads1115`: PGA scale 32767→32768
- `ws2812`: validate 4 HAL function pointers
- `oled_ssd1306`: validate write_cmd/write_data in init

### L2 Protocols
- `websocket_frame`: SIZE_MAX overflow (parse+build), RSV bits, control frame validation, reserved opcodes 3-7/0xB-0xF, WS_ERR_OVERFLOW for oversized, NULL payload+nonzero len
- `coap_packet`: delta=14 uint32, subtraction-safe bounds, opt_num overflow, C++ keyword (class→cls), ascending order guard, value_len UINT16_MAX, NULL options, bare payload marker rejection
- `mqtt_packet`: payload_len size_t, 4th-byte continuation, topic_len guards (publish+subscribe+put_str), QoS overflow with extra, size_t total, QoS>2 rejection, parse_publish payload_len wrap, password-without-username, remaining_length>268M rejection
- `nmea_parser`: checksum verification in parse, 16 test checksums fixed
- `http_parser`: HTTP_ERR_NOT_FOUND, version digit+dot validation, OWS tab stripping
- `ota_ab`: flash fn NULL checks, max_boot_count=0, params_size minimum
- `at_command`: empty expected guard, strlen UINT16_MAX guard
- `modbus_rtu`: byte_count>250, register address wrap guard (read+write)
- `ncp5623_parser`: idx sentinel prevents stale re-trigger

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
- `log_flash`: init only erases sector 0 (caller should call erase_all for clean slate)
- `log_dedup`: same-length lines >LOG_DEDUP_LINE_MAX differing only past truncation point may false-dedup (architectural limit of truncated storage)
- `log_ringbuf`: read output not NUL-terminated (returns byte count; documented contract)
- `checksum`: fletcher-16 seeded variant has {0x00}=={0xFF} collision (inherent to seed=0xFF)
- `event_queue`: capacity=1 is accepted but queue is permanently full (documented)
- `fal` (vendor): addr+size uint32 overflow on 32-bit targets (vendor code)
- `modbus_rtu`: 251-byte stack allocation in slave_read_regs/master_write_regs
- `ota_ab`: ota_ab_crc32 publicly exported (should be static, design-level)
- `commit_suggest.sh`: unbound variable under set -u when MAX_TYPE != wheel
- `batch_fix.sh`: dry-run sed detection breaks with existing flags

## Remaining test gaps (low priority)

- `coap_packet`: COAP_ERR_TOO_MANY (>8 options) untested
- `http_parser`: HTTP_ERR_TOO_MANY (>16 headers) untested
- `modbus_rtu`: MODBUS_EX_ILLEGAL_DATA_ADDR never emitted/tested
- `ota_ab`: OTA_AB_ERR_CRC and OTA_AB_ERR_PARAMS untested
- `nmea_parser`: parse_time/parse_date digit validation (MINOR)
- `mqtt_packet`: build_connect per-string UINT16_MAX guards (theoretical in embedded)
