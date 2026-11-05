# Known Issues & Prior Fixes

This file is the single source of truth for audit agents. **Read this before reporting findings.**

Anything listed here has been evaluated and either fixed or documented as accepted.

## Fixed by the encoder fuzz targets (2026-11-05)

Eleven protocol fuzz targets only parsed input. They now also build frames from the fuzz input in exact-size buffers and read them back, which lifted their line coverage from 35-56% to 94-100%. Writing and running them found:

- `mqtt5_packet`: `mqtt5_build_connect` counted the variable header as 11 bytes, not 10, so every CONNECT announced one byte more than it carried. The unit test had set the field aside with `(void)rl`.
- `mqtt_packet`: `mqtt_packet_build_publish` passed -1 to `memcpy` when the remaining length went past 268435455. `mqtt_packet_build_connect` could do the same with very long strings and now checks each string first.
- `mqtt_packet`: `mqtt_packet_build_subscribe` accepted a QoS above 2.
- `mqtt_packet`, `mqtt5_packet`: a PUBLISH with a NULL payload and a non-zero length announced bytes it did not hold.
- `websocket_frame`: `websocket_frame_build` accepted fragmented control frames and control frames over 125 bytes, which its own parser rejects.
- `coap_packet`: a NULL token or option value with a non-zero length shifted the rest of the frame, and a NULL payload with a length was dropped. All three now return `COAP_PACKET_ERR_PARAM`.
- `msgpack`: `msgpack_enc_str` and `msgpack_enc_bin` passed NULL to `memcpy` for empty values.

A scan of open work then found that `make check` never ran `make test-property`, and 122 of the 193 `prop_*.c` files are not CMake targets, so they ran only by hand. `pac1952` had failed since its VSENSE fix because the property still read VSENSE as signed. `make check`, `make ci-local` and `make wheel-gate` now run property tests.

## Datasheet check of ten drivers (2026-11-05)

All ten had defects: 67 confirmed, 23 suspect. Each driver's README lists what was checked, fixed and left open. None had run on hardware, and these fixes have not either, so they match the documents and nothing more.

- Fixed in full or nearly so: `sx1262`, `cc1101`, `bme680`, `bma400`, `bno055`.
- Not usable on hardware until a larger change lands: `bmi270` (8 KB config upload), `vl53l1x` (port ST's driver), `pac1952` (HAL needs Send Byte, Block Write and a delay).
- Limited: `bmm150` (no trim compensation, so uT values are approximate), `bq27441` (reads work, design capacity needs block data access).

Six test models repeated the driver's mistakes while claiming to come from the datasheet, which is why the tests were green. A passing mock test says nothing about a register value. Take expected values from the document.

The other 70 drivers with registers have not been reviewed. This batch suggests they hold defects of the same kind.

## Fixed by the fuzz target expansion (2026-11-05)

Growing libFuzzer from 25 to 53 targets, with harnesses that compute check fields and build frames, found these:

- `base64`: past 49149 input bytes the encoded size wrapped in `uint16_t`, so `base64_encode` passed its size check and wrote past the buffer.
- `cobs`: `cobs_max_encoded_size` wrapped to 0 and 1 for 65278 and 65279 bytes, and a saturated bound let the encoder run past a 65535-byte buffer. The bound now saturates from 65278 and the encoder checks capacity as it writes.
- `slip`: `slip_max_encoded_size` saturates past 32766 bytes, so the encoder could run past a 65535-byte buffer. It now checks capacity per byte.
- `heatshrink`: a full output buffer left `out_bit_idx` at 8 (undefined shift on the next poll) and sent the current token twice, corrupting the stream. The token now resumes where it stopped. The encoder also refused all data after `window_size` bytes; it now restarts the window once it is encoded.
- `crsf`: `crsf_encode_frame` read past the 64-byte payload array when `payload_len` was larger.
- `s_record`: a count of 0 read `binary[-1]`, and a count below the address size plus checksum took the address from uninitialized stack.
- `json_rpc`: `json_rpc_extract_id` overflowed `int32_t` on long digit strings and on `-2147483648`. Values that do not fit now return `JSON_RPC_ERR_OVERFLOW`.
- `ota_ab`: `fw_size` from a loaded record was not bounded, so verification could read past the slot, and size 0 with CRC 0 passed as verified. Both now return `OTA_AB_ERR_SIZE`.
- `isotp`: a Flow Control frame returned `ISOTP_OK`, the code for a complete message. It now returns `ISOTP_INCOMPLETE`.
- `at_command` fuzz harness: registered its URC with a NULL callback, which is rejected, so URC dispatch never ran (harness bug, wheel unaffected).
- `tooling/test/run_libfuzzer.sh`: fuzz builds did not define `CONTRACT_ENABLED`, so contracts were compiled out under fuzzing.
- `tooling/audit/audit_deep.sh`: `grep -c ... || echo 0` gave two lines, so the init/deinit and `_err_t` checks could never fire. `_deinit` is optional for wheels, so the init/deinit count is now information, not an issue.

## Fixed after the first hosted nightly run (2026-11-05)

The public mirror's nightly workflow (libFuzzer + coverage gate on GitHub-hosted runners) found these on its first run:

- `modbus_tcp`: `6 + header.length` was computed in `uint16_t`, so a length field of 0xFFFF wrapped to 5 and an 8-byte frame passed the size check with `data_len = 0xFFFD`; `modbus_tcp_get_regs` then read past the buffer (ASan heap-buffer-overflow). Fixed with a 32-bit comparison; the fuzzer input is now `test_parse_length_field_wraps_u16`.
- `nmea_parser`: `strtof` accepts `9e99`, `nan` and `inf`, and `(int)(v / 100.0f)` on those is undefined behaviour. `parse_lat`/`parse_lon` now reject values outside the ddmm.mmmm / dddmm.mmmm range and return 0; regression test `test_rmc_non_finite_lat_lon_rejected`.
- `syslog_packet` fuzz harness: `data[5] << 24` promoted to `int` and overflowed for bytes >= 0x80 (harness bug, wheel unaffected).
- `tooling/test/run_libfuzzer.sh`: the `xmodem` and `ymodem` targets had no `crc_any` source or include path and never compiled.
- `tooling/test/coverage_wheel.sh`: `gcov-tool merge` prints "mismatched profiles" but exits 0 when test targets compile the wheel with different defines (`CONTRACT_ENABLED` adds a function), leaving empty counters; `mpr121` was reported at 0% although its tests cover 100% of lines. Replaced by `tooling/test/gcov_union.py`, a line-level union of the per-target gcov reports. Branch percentage now means "taken at least once".

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

## Fixed in super audit 2026-05-02 (85 new wheels)

### CRITICAL (8, all fixed)
- `protobuf_lite`: length-delimited field >UINT16_MAX silently truncated
- `mqtt5_packet`: remaining length uint32→uint16 truncation in 3 build functions
- `lorawan_packet`: unsigned subtraction underflow with large fopts_len
- `canbus`: signal extract start_bit+length overflow past 64 bits
- `modbus_tcp`: get_u16_be operator precedence (cosmetic but inconsistent)
- `mcp2515`: extended ID SIDL register bit-field encode/decode incorrect
- `mlx90614`: EEPROM write missing mandatory 5ms erase-write delay
- `lora_sx1276`: SNR signed integer division truncation

### MODERATE (28, all fixed)
- L0: base64 trailing bits, cobs/slip/hex_codec size overflow, endian NULL guards, fixed_point add/sub saturation + FP_MIN UB, printf_tiny size=0 underflow, rate_limiter token overflow, string_utils INT32_MIN parse
- L1: bme680 heater float truncation, enc28j60 register classification (verified correct), at24cxx batch page write, stepper sqrt overflow, vl53l0x polling delay, nrf24l01 receive max_len, keypad_matrix get_event state, apds9960 chip ID variant
- L2: cbor_lite length overflow, dns_packet name max 64→253, ymodem file_size overflow, i2c_scan count cap, syslog_packet facility/severity validation
- L3: telemetry snprintf overflow, sensor_hub float drift (documented), config_store load range validation, flash_journal read_nth size ordering

## Accepted design decisions (new wheels)

- `sensor_hub`: float sum accumulator drift over millions of polls (documented, use reset_stats periodically)
- `timer_wheel`: O(max_timers) tick scan, not O(1) as originally intended (documented)
- `moving_window`: running sum/sum_sq float drift (same class as sensor_hub)

## Remaining test gaps (low priority)

(none — all previously listed gaps resolved in R10)
