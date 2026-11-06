# Known Issues & Prior Fixes

This file is the single source of truth for audit agents. **Read this before reporting findings.**

Anything listed here has been evaluated and either fixed or documented as accepted.

## Fixed by the stateful fuzz targets (2026-11-05)

`mqtt_client`, `ota_manager`, `cli_shell` and `air780eg` got libFuzzer targets, which takes the total to 57. Each one runs the component through sequences the input picks and checks it against a model. Line coverage is 89 to 97 percent, and 22 injected bugs were all found. `cli_shell` had no findings.

- `mqtt_client`: `mqtt_client_feed` dropped what did not fit in the receive buffer, so a large read lost packets and the stream lost its place.
- `mqtt_client`: a packet larger than the buffer cleared the buffer, and the rest of it was read as new packets. It is now skipped whole.
- `mqtt_client`: a broken remaining length, or a PUBLISH that failed to parse, left the session open. Both now close it with reason 0xFD.
- `mqtt_client`: a refused CONNACK followed by more bytes in the same read still dispatched them.
- `mqtt_client`: `on_message` and `on_event` shared one context pointer, so the second registration replaced the first. Found by reading the code.
- `mqtt_packet`, `mqtt5_packet`: a PUBLISH with both QoS bits set parsed as QoS 3.
- `ota_manager`: the last chunk could write past the announced image size, a transport claiming more bytes than the buffer holds was believed, and an image of size 0 was accepted.
- `ota_manager`: progress fell back near 0 on images of 42949673 to 50331647 bytes, because the overflow guard started too late.
- `air780eg`: a long digit string in `+CSQ` overflowed an int, and any number was stored as the signal level.
- `air780eg`: a topic of 128 bytes or more was cut and delivered, so a message could reach the handler of a different topic. It is now dropped.

Left as they are, and listed in each README:

- `mqtt_client` does not report a refused subscription. Telling the caller needs a new callback or return value.
- `air780eg` cuts a line at 511 bytes and still processes it.

## Fixed after the encoder batch (2026-11-05)

These came out of reading the encoder results and the list of remaining work.

- `tftp`: the request builders and `tftp_parse` cut a filename of 128 bytes or more down to 127, so a request could name a different file. They now return `TFTP_ERR_OVERFLOW`.
- `intel_hex`: `intel_hex_decode` accepted record types above 05 and any text after the checksum, and `intel_hex_encode` wrote undefined types. All three are refused now.
- `sntp_client`: `sntp_client_unix_to_ntp` and `sntp_client_ntp_to_unix` both rounded down, so 992 of the 1000 millisecond values lost 1 ms on a round trip.
- `mqtt_client`: `mqtt_client_subscribe` reported an illegal QoS as `MQTT_CLIENT_ERR_SIZE`, and it sent QoS 2 although the client never answers a QoS 2 delivery with PUBREC. A QoS above 1 now returns `MQTT_CLIENT_ERR_PARAM`.
- `bencode`: a stray `e` at depth 0 came back as `LIST_END`. Past `BENCODE_MAX_DEPTH` a dict end came back as `LIST_END`, and 256 opens wrapped the depth counter. Both are errors now.
- `dhcp_packet`: DISCOVER was 250 bytes and REQUEST 262, under the 300-octet BOOTP minimum in RFC 1542 section 2.1. The builders now pad to 300, so `cap` must be at least `DHCP_PACKET_BOOTP_MIN_LEN`.

The 26 fixed-seed fuzz programs (`wheels/protocols/*/test/fuzz_<name>.c`) and `tooling/test/run_fuzz_protocols.sh` are gone. Replaying each libFuzzer corpus under gcov against the old program showed 10 lines reached only by the old one, all NULL guards that `test_null_*.c` covers.

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

## How to read the datasheet check counts (2026-11-06)

The three batches below confirmed 385 items. 228 are in driver code, across 65 drivers. 157 are in test models or mocks, and 12 more drivers had confirmed items only there: `ads1015`, `ads1115`, `cst816s`, `ht16k33`, `ina3221`, `lis2dh12`, `lis3mdl`, `max31855`, `max7219`, `sht40`, `st7735` and `tsl2591`. Model faults are fixed where a driver fix needed them, and the rest are under Still open in each README.

Six suspects from the last batch were confirmed later from a second source (`ms5611`, `ft6x36` and `bme688`, two each), so those counts differ by six from the batch figures below. 162 suspects remain open.

## Datasheet check of the last 49 drivers (2026-11-06)

This batch covers every driver with registers not yet reviewed in November: 186 confirmed, 103 suspect. Only `pcf8574` had no finding. `icm42688` could not be checked, because its manual could not be downloaded. Each README lists what was checked, fixed and left open. None of this has run on hardware.

- Confirmed driver faults in constants, formulas, timing and checks are fixed, each with a failing test first. The worst were: `lsm6dso` and `tcs34725` read every axis or channel from one register, `bme688` was a BME680 driver and its gas path could not work, `max31865` was 107 C out at 850 C, `mcp4725` power-down modes did nothing, `ms5611` failed its CRC on real parts, and `scd40` took not-ready for ready.
- Not usable on hardware until a larger change lands: `vl53l0x` has no device initialisation or calibration. `apds9960_read_gesture` needs its direction logic rewritten.
- Waiting on a HAL change: `mlx90614` needs a repeated START on reads, `ms5611` a command with no data byte, and `tca9548a` a read with no register byte.
- Left for a decision: `is31fl3731_set_led_enable` reads a frame register while the chip runs, and the fix adds 144 bytes to `is31fl3731_t`. `mcp4728` writes move a channel to the VDD reference, and keeping 2.048 V needs new fields in its config.
- The drivers reviewed only in May have not been redone, apart from the three rechecks in the batch below, and all three of those found faults. Treat that review as weaker than this one.
- Some calls now refuse what they used to accept, or take longer. `delay_ms` is required by `bmi160`, `ms5611`, `scd40`, `sgp30`, `shtc3`, `si7021` and `tcs34725`. `hmc5883l` init takes about 3 output periods when `delay_ms` is set, `dps310` init waits for its ready bits, and `pcf8563` stops the clock while setting the time. Each README says what changed.

Running `wheel-gate` on every wheel also found 8 that had been failing its UBSan stage, which `make check` does not run: negative shifts in `bmp280` and in the `pct2075`, `tmp102` and `ina3221` models, a NULL passed to memcpy in `byte_buffer`, overflow in `scale_map`, a bad width in `crc_any`, and a shift in vendored FlashDB. All are fixed. `audit_sanitizer.py` only runs each wheel's main test target, which is why it reported them clean.

## Datasheet check of 24 more drivers (2026-11-06)

This batch covers 20 unreviewed drivers, rechecks of `ina219`, `ds3231` and `nrf24l01` from May, and `sx1262` against the datasheet the first batch could not download: 122 confirmed, 55 suspect. Only `tm1637` had no finding. Each README lists what was checked, fixed and left open. None of this has run on hardware.

- Confirmed driver faults in constants, formulas, timing and checks are fixed, each with a failing test first. `lora_sx1276` took 13 commits, `nrf24l01` 8, and `sd_spi`, `rc522`, `mcp2515` and `sx1262` 4 each.
- The May rechecks all found faults those reviews had passed. In `nrf24l01` the register map was right, but sequencing, timing and registers never written were not.
- Waiting on a HAL change: `ccs811` needs address-only and multi-byte writes, as APP_START and ENV_DATA go out in a form the manual does not allow. `max17048` needs a 16-bit write, as its writes change nothing today. `mcp2515` needs a delay.
- Model faults are fixed where a driver fix needed them. The rest are in each README under Still open.
- Of the 23 suspect items from the first batch, 4 are now confirmed, 9 were dismissed and 10 stay open.
- Some calls now refuse what they used to accept, or take longer: 1 Mbps on `mcp2515`, SF6 on `lora_sx1276`, rates above 800 Hz on `adxl345`, a NULL `delay_ms` on `bmm150`, and about 100 ms in `nrf24_init`.

`make test-examples` used to build the examples without running them. It runs them now, and the first real run caught a mock that a `lora_sx1276` fix had left behind.

## Datasheet check of ten drivers (2026-11-05)

All ten had defects: 67 confirmed, 23 suspect. Each driver's README lists what was checked, fixed and left open. None had run on hardware, and these fixes have not either, so they match the documents and nothing more.

- Fixed in full or nearly so: `sx1262`, `cc1101`, `bme680`, `bma400`, `bno055`.
- Not usable on hardware until a larger change lands: `bmi270` (8 KB config upload), `vl53l1x` (port ST's driver), `pac1952` (HAL needs Send Byte, Block Write and a delay).
- Limited: `bmm150` (no trim compensation, so uT values are approximate), `bq27441` (reads work, design capacity needs block data access).

Six test models repeated the driver's mistakes while claiming to come from the datasheet, which is why the tests were green. A passing mock test says nothing about a register value. Take expected values from the document.


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
