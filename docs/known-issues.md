# Known Issues & Prior Fixes

This file is the single source of truth for audit agents. **Read this before reporting findings.**

Anything listed here has been evaluated and either fixed or documented as accepted.

## State at the close of the November round (2026-11-10)

Work on new features stopped on this date, and only fixes are planned. This section lists what is known to be open. Read it before relying on a component.

**Nothing changed since 2026-04-27 has run on hardware.** Every fix in this file dated November matches a manual, a test or a proof on the host, and nothing more. That includes the later changes to the five components of the April soak (`crc16`, `w25qxx`, `qmi8658a`, `xgzp6816d`, `qmc5883p`) and to `air780eg`, which now ends commands with CR where the board was tested with CR LF. The GD32F4xx HAL port under `vsf-addon/` compiles and has host models, but no firmware calls it: the April soak ran the flybox firmware through its own compatibility layer.

Drivers that cannot do their job as they are, with no further work planned:

- `bmi270` has no 8 KB config upload, `vl53l1x` lacks the configuration ST's driver loads, and `vl53l0x` has no device initialisation or calibration.
- `w5500` has no API to connect, listen or set a peer. `apds9960_read_gesture` needs its direction logic rewritten. `gc9a01` needs a per-panel init table. `rc522_halt` cannot halt a card, because the driver never selects one. `tcs34725_get_lux` needs gain and integration time passed in.
- Limited: `bmm150` has no trim compensation and `bq27441` cannot set the design capacity. The READMEs of `sx1262`, `cc1101`, `bme680`, `bma400` and `bno055` list what they lack.
- The suspect items of the datasheet check are still open, under Still open in each driver README. So are the test model faults that no driver fix needed.

Open by decision, or because nothing here can test it:

- `flash_ring` keeps one header copy, so a power cut between the header's erase and its program resets the ring to empty with the frames still in flash (W1A-9). A second copy needs a spare sector and a config field. `flash_ring_peek` takes an absolute slot, so a caller reads `read_idx` from the struct to find the oldest frame (W1A-10).
- The Nations headers under `vsf/` and the local changes to the FlashDB copy in `kvs` stay as they are (Q2-1, Q2-2, the owner's decision).
- All 259 wheels carry the candidate label, although the contract asks for a board run for drivers and modules and a recorded API approval (D1-5). The READMEs carry stock review text, which the audit accepts (T1-29). They name `@L-X-Yao` as the responsible human since 2026-11-11, in place of the scaffold's `@user`.
- `make verify` finishes 11 of its 15 CBMC proofs: `cobs`, `soft_timer`, `bitmap` and `fixed_point` hit the 300 s limit. Frama-C is not installed, so the `ring_buffer` ACSL contracts were not proved again after its rewrite.
- `xmodem` without a timer can still take a stray 0x04 for EOT. `ota_ab` keeps a power-cut window unless `params_addr2` is set, and `boot_counter` has one record copy. `sntp_client` refuses every reply when the caller passes no transmit time. `mqtt_client` does not report a refused or timed-out subscription.
- Seven wheels keep the caller's name pointer against api-conventions 3.3: `power_manager`, `scheduler`, `cli_shell`, `param_table`, `config_store`, `at_command` and `mqtt_client` (Q-8). `telemetry`, `sensor_hub` and `task_watchdog` now state the lifetime rule in their headers.
- Adding a timer to `timer_wheel` beside a tick in an interrupt needs the interrupt masked. `ring_buffer`, `fifo_queue`, `double_buffer`, `soft_timer` and `timer_wheel` stop the build on compilers other than GCC and Clang until their barrier macro is defined.
- `nmea_parser` takes position from GGA but not time (W3B-22). `signal_stats` stops counting at 2^32-1 feeds. `stepper` can make the first delay up to 16.7% short. `kvs` needs about twice its usual stack the first time garbage collection runs, which only the README says (Q18-2).
- HAL port: a read without STOP keeps clocking, a data NACK is reported as an address NACK, reads longer than 2 bytes need the interrupt served within a byte time, and the STOP clear can race a register update.
- The private flybox firmware cannot build the current wheels: its build passes no `tooling/harness` include path, does not list `nmea_parser`, and its `fr_shim.h` defines the `flash_ring` include guard before the header (D1-2). `projects/` is frozen, so this is recorded and not fixed. Its barometer adapter fault from the section on second sources is open too.

The 223 minor items of the tenth super audit are recorded and not fixed, except where a larger fix covered them.

## Fixes at the close (2026-11-10)

Two checks ran on the last day. Neither result has run on hardware.

**`flash_ring` under power cuts and I/O faults.** A randomized test drove small rings through pushes, pops, discards, header flushes and resets, with a power cut or a failing erase, program or read injected at any flash operation. It does not model the ring. It tracks what was programmed and which pushes were acknowledged, and checks that a live slot is never blank, that acknowledged frames stay in order and go only by a documented rule, and that after a reset the writer is where the last program ended. It found two faults in the November rewrite, each fixed with a failing unit test first:

- A program that failed, followed by a read-back that failed too, was counted as a frame. If nothing had been written, a blank slot sat among the live ones. On the last slot of a sector a reset stopped its walk there, and the next push erased the frames stored after it. The ring now stays on that slot and the next push reads it first. While the read fails, push returns `FLASH_RING_ERR_IO` and programs nothing. `discard_all` does not reach a slot in that state.
- After a failed header write, push still erased ahead. A lap later that erase took the sector the header pointed into, and a reset then started from a blank slot, served blank slots as frames and lost what was stored after it. Push now rewrites the header before such an erase, and returns `FLASH_RING_ERR_IO` without erasing if it cannot. This is the one case where a failed header write fails a push.

After both fixes 900,000 runs passed: about 850 million operations with about 32 million power cuts, on rings of 2 to 8 sectors with 1 to 8 slots each. A short run of the test, 1350 seeds in under a second, joined the wheel on 2026-11-11 as `test/test_faults_flash_ring.c`. It leaves out a half-written header, a frame of all 0xFF and flash images from older code, which the README lists as known. `flash_ring_t` grew by 4 bytes. Mutation testing of the unit test also found three changes it let through, now pinned: a ring emptied part way through a sector, and a header whose write or read index equals `total_slots`.

**GCC 15.** GitHub moved `ubuntu-latest` to Ubuntu 26.04 on about 2026-11-07, and the Nightly coverage job of the public mirror failed from then on. GCC 15 rejects, under `-Wextra` and `-Werror`, a string literal that fills a `char` array and leaves no room for its NUL: `base64` and the `msgpack` tests each had one. Two README API summaries listed calls with bare parameter names, which GCC 14 and later refuse as declarations. All three are fixed, `make check` passes with GCC 13 and with GCC 15, and the mirror's workflows now name their runner image. GCC 15's static analyzer, run over the wheel sources, reported two items, both in `mqtt5_packet.c` and both wrong: `mqtt5_encode_vbi` returns the number of bytes it wrote, and the analyzer does not connect the two.

`docs/arm-binary-sizes.md` was regenerated on 2026-11-11. The November fixes took the total from 157.5 KB to 173.9 KB of `.text`.

## Super audit, second run (2026-11-07)

The tenth super audit reported 36 critical, 250 moderate and 223 minor items in 20 slices. The first 57 fixes are in "Super audit, first fixes" below. This run handled the other critical and moderate items, each fix with a failing test or a forced failure first. The exceptions are in the section above. Two items were wrong as written: `signal_stats` could no longer divide by zero after an earlier fix (W1C-17), and `gpio_led_on` on a zeroed LED reported ON, it did not crash (W2-11). None of this has run on hardware.

What changed for callers, by layer. Each README has the detail.

Primitives:

- `flash_ring`: init refuses single-sector rings and erases the first data sector on fresh flash. Push erases one sector ahead and drops the oldest frames a sector at a time when full, so the ring holds at most `total_slots - 1` frames. Init walks forward from the stored `write_idx` to the real one, and frames popped since the last header write come back after a reset. Push and pop return OK when only the header write failed, with one exception since 2026-11-10 (see the section above). `peek` refuses a length above `slot_size` and a slot outside the live range, and returns EMPTY on an empty ring.
- `fal`: partition calls return `FAL_ERR_NOT_READY` until `fal_init` succeeds, and `fal_init` can return `FAL_ERR_HAL` or the new `FAL_ERR_PART_TABLE`. A range that wraps returns `FAL_ERR_OVERFLOW`.
- `gnss_filter`: `gnss_datetime_to_unix` returns -1 for a month outside 1 to 12. Init refuses non-finite floats, a `max_hdop` not above 0, a `max_vdop` below 0 and an alpha outside 0 to 1. A non-finite position or a NaN hdop closes the gate. The longitude filter wraps at 180 degrees.
- `cli_parser`, `cli_shell`: an over-long line prints `line too long` and does not run. `cli_parser` skips table entries with a NULL name or handler. Feed and run are not ISR safe.
- `base64_decoded_size` grows when the length is not a multiple of 4. `fp_to_int_rounded` saturates at 32767, and `fp_to_string(FP_MIN)` prints -32768. `framebuffer` clips sizes past 65535. `bitfield` writers need the caller's lock.
- `soft_i2c`: an address above 0x7F returns `ERR_PARAM`, SDA low before a START returns `ERR_BUS`, and `soft_i2c_bus_recover` returns `ERR_BUS` if SDA stays low.
- `soft_timer`: a callback runs after its timer is settled, so a one-shot can re-arm itself, and a restarted one-shot waits a full period.
- `str_to_float` returns `ERR_RANGE` past FLT_MAX. `varint` returns OVERFLOW for a tenth byte above bit 0. `timer_wheel_tick` does nothing with zero slots.
- `kvs`: init and clean repair a sector with a bad header, and the three iterators return at once after a failed init. A full TSDB keeps its logs and returns `FDB_SAVED_FULL` until cleaned. Two more marked fixes went into the FlashDB copy: the reverse time iterator lost older sectors when one filled exactly, and `fdb_tsdb_init` with every sector full lost all logs on the next boot. `VENDOR.md` lists every local change. The tests now call every public function.
- `log_dedup`: lines that differ only in level no longer collapse. `log_flash`: after a failed erase, lines that need the next sector are dropped until an erase succeeds. `log` can format with `printf_tiny`, which is off by default.
- `lookup_table`, `mag_cal`, `pid_controller`: NaN or infinite input is refused. `pid_controller` returns 0.0f for it, or the nearest limit if 0 is out of range. `mahony_ahrs` ignores non-finite input, and a reset clears the integrators.
- `moving_rms` keeps a second running sum, 4 bytes more. `printf_tiny` caps a width at 4095 and stops at the first unknown specifier, and its format attribute can raise `-Wformat` errors. `sha256_update` returns `SHA256_ERR_PARAM` for NULL data with a length.

Protocols:

- `ota_ab_init` refuses a `max_boot_count` of 255. `ppp_frame_decode` accepts any payload up to `payload_cap`, and source and payload buffer must not overlap. `pb_decode_field` returns `PB_ERR_MALFORMED` for field 0 and fields above 255.
- `s_record_encode` returns OVERFLOW for an address too wide for the record type, and decode refuses anything but CR or LF after the checksum. `tftp_parse` returns OVERFLOW for DATA above 512 bytes. `ymodem` has `YMODEM_ERR_OVERFLOW` for names of 64 bytes or more, and sizes past 32 bits saturate.
- `sntp_client` gives real times for 2036 to 2104, by the era rule of RFC 4330.
- `http_request` refuses a space in the path and CR or LF in any field. `intel_hex` refuses misshaped records. `mqtt_packet_parse_publish` refuses a cut packet. The `mqtt5_packet` parsers accept properties they do not store and refuse IDs not valid for the packet.
- `json_rpc` reads only top-level keys, treats batch arrays as invalid, and escapes method and message text. `ibus_decode` keeps 12 channel bits. After `ISOTP_ERR_SEQUENCE` the message is lost.
- `nmea_parser` ignores out-of-range values and returns OK. It reads decimals without `strtof`, so `1e2`, `0x10`, `12abc` and a bare `.` count as out of range.
- `onewire_search`: CRC and bus errors hold the search, both bits reading 1 gives `ERR_BUS`, and family code 0 is refused. The `ncp5623_parser` docs now name its protocol, ToshibaLED.

Modules:

- `data_logger`: `add_string` refuses comma, double quote, CR and LF. Commit refuses a short record and leaves it open.
- `event_bus`, `scheduler`: a subscriber or task removed during a pass is not called by it. `EVENT_BUS_MAX_DEFERRED` is gone.
- `flash_journal`: init returns `ERR_STORAGE` on any failed read, and appends then fail with `ERR_FULL` until an init succeeds. A write that fails after programming gives up the sector.
- `mqtt_client`: QoS 0 publishers now send PINGREQ. `next_id`, and the activity and release calls of `power_manager`, are no longer labelled ISR safe. `power_manager_register` refuses timeouts above 0x7FFFFFFF ms.
- `sensor_hub` counts infinity as a read error. Registering a known name with `task_watchdog` restarts it. `telemetry` formats floats without printf.
- A limit above 255 no longer compiles in the wheels that count in `uint8_t`.

Drivers:

- Every driver's init clears the device struct first. After a refused init, later calls return the parameter error, and a working struct passed to a refused init is wiped. Recovery needs another init.
- More drivers refuse a NULL callback they need: `adxl345`, `apds9960`, `bme280`, `bmp280`, `bno055`, `ccs811`, `ds3231`, `ft6x36`, `hd44780`, `lis3dh`, `lps22hb`, `mcp4725`, `mpu6050`, `ms5611`, `sgp40`, `tmp102` and `vl53l0x`.
- Refused now: `max31855` with D16 alone, `rc522` 7 and 10 byte UIDs, `sd_spi` byte-addressed blocks above 0x7FFFFF, impossible dates in `pcf8563`, `osrs_t` 0 in `bme688`, and `KEYPAD_MAX_COLS` above 16.
- `bme680`, `bme688`: pressure is right outside -47 to 97 C. `servo_get_angle` follows `set_pulse`. A failed `veml7700` setter keeps the old scale. `bno055` refuses `read_euler` after a failed mode change. `hcsr04` can block 25 ms longer. `pca9685` wakes and restarts its outputs after a failed prescale write. `hx711` reads after an init timeout return `ERR_PARAM`.

Tooling:

- `make frama-c`, `fuzz`, `test-mutate`, `batch_fix`, `wheel_promote`, `stack_analyzer` and `--merge-all` exit non-zero where they used to report success. `audit_wheels` fails on C99 or `-Werror` drift, `check_readme_examples` fails on stale wheel APIs, and `make test-property` includes `kvs`. Mutation rates from before this date do not compare with later ones.
- The generated `test_null_*` files assert rejection: 258 wheels, 1626 asserted calls, 36 allowed as optional. No wheel failed to reject NULL.
- Hooks are mode 100755, and `make bootstrap` warns when `core.hooksPath` is not `.githooks`.
- A sweep with `-fshort-enums`, the ARM default, passed on 258 wheels. `heatshrink` failed and is fixed.

## API changes approved for the audit (2026-11-07)

These items could not be fixed behind the old API. Each README has a migration note. None of this has run on hardware.

- Memory and concurrency: `hmac_sha256_t` holds a `sha256_t`, so it is aligned for Cortex-M. `ring_buffer` and `fifo_queue` dropped their count field: one writer and one reader may run in different contexts, and reset and clear belong to the reader's side. `double_buffer` and `soft_timer` are ISR safe behind a barrier.
- Transfer and storage: `xmodem` refuses EOT after a bad frame until a block passes, and can use line idle in timer mode. In `ymodem` one CAN no longer cancels, two EOTs back to back are handled, and a short file gives `YMODEM_ERR_SHORT`. `ota_ab` renamed `reserved` to `seq`, refuses overlapping regions and can keep a second parameter copy at `params_addr2`. The `boot_counter` record grew from 8 to 20 bytes, an invalid record loads as 0, a failed load blocks saves, and a KV backend must return `BOOT_COUNTER_READ_NO_RECORD`.
- Protocols: `http_parser` ends 1xx, 204 and 304 at the blank line, and has `parse_response_to_head` and `ERR_TRANSFER_ENCODING`. `at_command` has `expect_final`. `mavlink` returns `PARSE_UNKNOWN_MSG` for an unknown id and has `set_crc_table`. `crsf` limits a payload to 60 bytes and returns `ERR_FORMAT` for a length outside 2 to 62. `mqtt_client` poll returns `ERR_TIMEOUT` once for a publish with no PUBACK. `sntp_client` refuses LI 3, a stratum above 15, a zero transmit time and versions other than 3 and 4, and a reply whose originate time is not the request's. `dns_packet` type and class are `uint16_t`. The `heatshrink` error codes are -1. `crc_any_finalize` exists now. `slip` drops a broken frame up to the next END and returns `ERR_DROPPED`.
- Primitives and drivers: `ewma` output reaches its input, and the struct grew from 8 to 16 bytes. `gnss_filter` holds position in double, 60 to 80 bytes. `kalman_1d` reset keeps q and r. `onewire` returns `ERR_PRESENCE` on an empty bus and has `ERR_BUS`. A timer added to `timer_wheel` in a callback waits its full delay. `state_machine` returns `ERR_BUSY` for a nested process or reset. `gpio_button` reports RELEASE on every edge and DOUBLE_CLICK one tick later. `lora_sx1276`, `sx1262` and `cc1101` return a new `*_ERR_SIZE` for a packet longer than the buffer and copy nothing.
- `air780eg` follows its manual: a plain OK ends a command only when OK is awaited, FAIL lines give `CMD_ERROR`, commands end in CR, topics go to 256 bytes and messages to 4100. The struct grew from 1672 to 5872 bytes, and `AIR780EG_PAYLOAD_MAX` lowers it. New: `send_data`, `on_fail`, `on_mqtt_cached` and `get_msg_drop_count`.
- Other drivers: `lis3mdl`, `ltr390`, `apds9960` and `tmp102` return `ERR_NOT_READY` before a sample exists. `ds3231_get_time` returns `ERR_TIME_INVALID` while OSF is set, and `set_time` clears it. `enc28j60_receive` no longer counts the CRC. `icm42688_soft_reset` sets the reset-range divisors. `mcp2515_abort_tx` exists now. `pac1952` takes its address from the config.

## HAL port, docs and mirror audits (2026-11-07)

- HAL port (`vsf-addon/hal/driver/gd/GD32F4XX/`): 3 critical and 16 moderate items handled. Its status is compile-only, as the section at the top says, and its README states that. Host models of the peripherals are in its `test/` directory and run through `run.sh`, not through `make check`. Flash write and erase now return an error where they returned OK, and take whole sectors only. I2C refuses zero-length, NULL, missing-handler and busy calls. SPI master needs a GPIO chip select. USART with 8 data bits and parity sends 8 data bits, and 9 with parity is refused.
- Docs: 14 of 16 moderate items fixed. The quick start, the catalog example and the guide snippet compile and run again, and so does the `mps2_bare` scaffold. The two left are D1-2 and D1-5, in the section at the top.
- Public mirror: no real credential in the tree or in its history. Customer detail was removed from test data and comments.

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

## Super audit, first fixes (2026-11-06)

The tenth super audit covered wheels, tooling, CI, portability, memory safety, numerics, tests and licences. With the docs, security and HAL port slice, which was redone later, it reported 36 critical, 250 moderate and 223 minor items. These are the first 57 fixes, each proved by a failing test or a forced failure first, and none of those proved wrong. The rest were handled on 2026-11-07, see "Super audit, second run" above.

- Checks that could not fail: `test_all.sh` ran one executable per wheel, so 100 of 944 test targets never ran in `make check`. It now runs all of them, and all pass. `--parallel` lost pytest results, `audit_compile.py` could not see warnings, `audit_coverage.py` measured the test file, `audit_deep.sh` skipped failed builds, and `build_flash_check.sh` and `hil_runner.py` could report a pass after a failed run.
- Memory safety: `lorawan_build_data` wrapped its length check, `mqtt_client` overran its buffer when a callback reconnected, `soft_timer_start` accepted a destroyed slot, `memory_pool_init` took misaligned buffers, and `config_store` and `error_handler` trusted what they loaded from flash.
- Wrong results: `fp_to_string` at 5 places, `crc_any` below 8 bits, `stepper` ramps, `hdlc` on shared flags, `json_parser` state, and `heatshrink` finish.
- Storage: `flash_journal` lost appends after a torn write, and `flash_ring_init` accepted layouts that destroy live frames.

Fixed on 2026-11-07, after this section was written: `flash_ring` read a full ring as empty and overwrote cells after a reset, `framebuffer_line` could hang, and `gnss_datetime_to_unix` read out of bounds. `hmac_sha256_t` could HardFault on Cortex-M, `ring_buffer` and `fifo_queue` lost data in their ISR use, and `xmodem` could accept a truncated file.

## High-severity suspects settled from second sources (2026-11-06)

17 suspects went to vendor application notes, vendor drivers, Linux, Zephyr and mainstream libraries: 16 confirmed, 1 dismissed. None of this has run on hardware.

- Fixed: `tcs34725` colour temperature now uses DN40's formula, `ws2812` derives each slot inside the bit loop, `rc522_halt` sends its CRC, `mcp2515` sets zero filters for both frame formats, `drv2605` selects a library in init, `cst816s` accepts the known chip IDs, `apds9960` gets working gesture pulses and gain, `vl53l0x` encodes its timeout as ST does, `mpu6050` init waits for the reset, and `pcf8563_read_time` returns `PCF8563_ERR_VL` until the time is set.
- Dismissed: `sk6812` sends 4 bytes per LED, which is right for the RGBW parts. Its header now says 24-bit parts are not supported.
- Confirmed, needing a public API change: `tcs34725_get_lux` needs gain and integration time passed in, `rc522_halt` cannot halt a card because the driver never selects one, and `gc9a01` needs a per-panel init table, so a real panel may show no usable picture.
- Confirmed in a project: the flybox barometer adapter fails on the NACK its sensor gives to the 0xAC byte, by reading the code. Nobody has seen this on a board.

## Test models corrected from the manuals (2026-11-06)

The test models and mocks of 77 drivers were corrected to follow their manuals: ignored writes, read-only bits, reset values, busy periods and first-sample waits. Models that no test compiled are now wired in. New assertions failed on the old model first, and many were also checked by putting faults back into the driver. None of this has run on hardware, so a model is still only a reading of the manual.

Corrected models exposed about 30 more driver bugs, which were fixed:

- Reads that gave zeros or junk with OK before the first result: `tsl2591`, `tcs34725`, `max31865`, `adxl345`, `pct2075`, `ms5611`.
- Waits that ended early or never happened: `hmc5883l` init, `bme280` and `bmp280` during the NVM copy and at high oversampling, `dps310`, `sht30`, `sht40`, `shtc3`, `at24cxx`.
- Calls the chip ignores or rejects, reported as OK: `mcp4725` during an EEPROM write, `mcp2515_send` outside Normal mode, `nrf24_send` in RX mode, `scd40` commands during a measurement, `veml7700` setters with codes outside the table.
- `bmp388` reset lost its oversampling, `pca9685_sleep` twice lost RESTART, `ccs811` init sent APP_START to a running chip, and `mcp3008` never raised CS at init.

Left for a decision then, because a fix changes what callers get. All but the last were fixed on 2026-11-07, see "API changes approved for the audit" above:

- `enc28j60_receive` counts the 4 CRC bytes in `*frame_len`.
- `icm42688_soft_reset` leaves the scale divisors stale, so 1 g reads 0.5 g until init.
- `lis3mdl`, `apds9960`, `tmp102` and `ltr390` return OK with zeros or old data before the first result. `ds3231` reads back a reset date as valid with OSF set.
- `mcp2515` has no way to abort a frame that cannot go out, so mode changes time out.
- `adxl345_init` now waits the turn-on time, which is about 10 s at 0.10 Hz. This stays.

The held-back drivers kept their code, so their models stop short of what the manuals say: the `qmi8658a` model hands out data at once. `air780eg` was held back too, until its fix on 2026-11-07.

## Datasheet check of the last 22 drivers (2026-11-06)

This batch covers the 21 drivers reviewed only in May and `icm42688`, whose manual turned up: 130 confirmed, 53 suspect. 68 of the confirmed items are in driver code and 62 in test models. Each README lists what was checked, fixed and left open. None of this has run on hardware.

- Fixed in 17 drivers. The worst: 218 of the 256 entries in the `ds18b20` CRC table were wrong, so real reads almost always failed. `pca9685_init` left the oscillator off, so no channel could work. `hx711_tare` failed on its second sample. A skipped channel on `bme280` and `bmp280` came back as a real reading. `max30102` read a full FIFO as empty, and `bh1750` one-time modes returned the same value forever.
- `enc28j60` needed nine fixes. `send` reported success on aborted frames, and half duplex was not set up as the manual and errata DS80349C require.
- Not usable as claimed: `w5500` has no API to connect, listen or set a peer, so TCP and UDP cannot work. Its README now says so.
- Checked but not changed, because they ran on a board or are held back and no board is available to retest: `w25qxx`, `qmi8658a`, `qmc5883p`, `xgzp6816d` and `air780eg`. `w25qxx_chip_erase` gives up after about 0.4 s while the chip needs 20 s or more. `air780eg` took a plain OK as the end of any command until its fix on 2026-11-07. `xgzp6816d` was checked against the sibling GZP6816D only.
- `delay_ms` is now required by `aht20`, `enc28j60`, `max30102`, `pca9685` and `sht30`, and by `bh1750` in one-time modes. `at24cxx` waits 20 ms per write unless `AT24CXX_WRITE_CYCLE_MS` says less. `hx711` and `bh1750` init take longer. `icm42688_read` returns `ICM42688_ERR_NOT_READY` until the first sample. Each README says what changed.

## API fixes approved after the datasheet check (2026-11-06)

These drivers could not follow their manuals through the old HAL or config, so the HAL structs and two configs gained fields. New callbacks go at the end of each struct. A driver with a NULL callback it needs returns its parameter error and sends nothing, so an old HAL fails loudly. None of this has run on hardware.

| Driver | Added | What was wrong |
|---|---|---|
| `ccs811` | `write_raw` | APP_START carried a data byte, and ENV_DATA went out as two one-byte writes |
| `max17048` | `write_raw` | 16-bit registers were written a byte at a time, so no write took effect |
| `ms5611` | `write_raw` | A 0x00 followed each command, and an all-zero PROM passed the CRC |
| `pac1952` | `write_raw`, `delay_ms` | REFRESH was at the wrong address with a data byte, and CTRL needs a Block Write |
| `tca9548a` | `write_raw`, `read_raw` | Reading the channels cleared them |
| `mlx90614` | `write_read` | Reads had a STOP where the manual needs a repeated START |
| `mcp2515` | `delay_us` | Nothing waited out the oscillator start-up, and leaving Sleep never woke the chip |
| `is31fl3731` | `led_ctrl` in `is31fl3731_t`, 144 bytes | LED Control was read while the chip ran |
| `mcp4728` | `vref_internal`, `gain_x2` in the config | Every write moved the channel to VDD and gain x1 |
| `bmp388` | none | SKIP measured at x1. It now clears the enable bit, and `{ .addr = x }` alone returns `BMP388_ERR_PARAM` |

`mcp2515` waits 128 us after a wake, which follows the manual but says nothing about crystal start-up, so check it on a board. `pac1952` had a fixed address of 0x10 until 2026-11-07. With temperature set to SKIP, `bmp388` reuses the temperature measured in init.

## Fixed by the full sanitizer audit (2026-11-06)

`audit_sanitizer.py` ran only each wheel's main test target, skipped `kvs`, and counted a failed build as a skip. 27 wheels failed to build under it, because the instrumentation sets off their own `-Werror`, so they never ran. The earlier "259/259" was therefore too high. It now builds with `-w`, runs all 931 test targets in all 259 wheels, and fails on a build error. `wheel-gate` now runs ASan as well as UBSan, covers the property tests, and fails on a sanitizer build error.

- `bmp280`: compensation overflowed on corrupt calibration. It now uses 64-bit intermediates, and a real part gives the same results.
- `bme280`: `dig_H4` and `dig_H5` shifted a negative value left.
- `modbus_rtu`: a test passed one buffer as both source and destination of `memcpy`.

## How to read the datasheet check counts (2026-11-06)

The four batches confirmed 515 items in 104 drivers. 296 are in driver code, across 84 drivers. 219 are in test models or mocks, and 14 more drivers had confirmed items only there: `ads1015`, `ads1115`, `cst816s`, `dht11`, `ht16k33`, `ili9341`, `ina3221`, `lis2dh12`, `lis3mdl`, `max31855`, `max7219`, `sht40`, `st7735` and `tsl2591`. Model faults are fixed where a driver fix needed them, and the rest are under Still open in each README.

Six suspects from the last batch were confirmed later from a second source (`ms5611`, `ft6x36` and `bme688`, two each), so those counts differ by six from the batch figures below. 215 suspects remain open.

## Datasheet check of the last 49 drivers (2026-11-06)

This batch covers every driver with registers not yet reviewed in November: 186 confirmed, 103 suspect. Only `pcf8574` had no finding. `icm42688` could not be checked, because its manual could not be downloaded. Each README lists what was checked, fixed and left open. None of this has run on hardware.

- Confirmed driver faults in constants, formulas, timing and checks are fixed, each with a failing test first. The worst were: `lsm6dso` and `tcs34725` read every axis or channel from one register, `bme688` was a BME680 driver and its gas path could not work, `max31865` was 107 C out at 850 C, `mcp4725` power-down modes did nothing, `ms5611` failed its CRC on real parts, and `scd40` took not-ready for ready.
- Not usable on hardware until a larger change lands: `vl53l0x` has no device initialisation or calibration. `apds9960_read_gesture` needs its direction logic rewritten.
- The HAL changes for `mlx90614`, `ms5611` and `tca9548a` landed later, see above.
- `is31fl3731` and `mcp4728` needed public struct changes, which landed later, see above.
- The drivers reviewed only in May were redone later, see above. The three rechecks in the batch below all found faults, so that review was weaker than this one.
- Some calls now refuse what they used to accept, or take longer. `delay_ms` is required by `bmi160`, `ms5611`, `scd40`, `sgp30`, `shtc3`, `si7021` and `tcs34725`. `hmc5883l` init takes about 3 output periods when `delay_ms` is set, `dps310` init waits for its ready bits, and `pcf8563` stops the clock while setting the time. Each README says what changed.

Running `wheel-gate` on every wheel also found 8 that had been failing its UBSan stage, which `make check` does not run: negative shifts in `bmp280` and in the `pct2075`, `tmp102` and `ina3221` models, a NULL passed to memcpy in `byte_buffer`, overflow in `scale_map`, a bad width in `crc_any`, and a shift in vendored FlashDB. All are fixed. `audit_sanitizer.py` only ran each wheel's main test target then, which is why it reported them clean.

## Datasheet check of 24 more drivers (2026-11-06)

This batch covers 20 unreviewed drivers, rechecks of `ina219`, `ds3231` and `nrf24l01` from May, and `sx1262` against the datasheet the first batch could not download: 122 confirmed, 55 suspect. Only `tm1637` had no finding. Each README lists what was checked, fixed and left open. None of this has run on hardware.

- Confirmed driver faults in constants, formulas, timing and checks are fixed, each with a failing test first. `lora_sx1276` took 13 commits, `nrf24l01` 8, and `sd_spi`, `rc522`, `mcp2515` and `sx1262` 4 each.
- The May rechecks all found faults those reviews had passed. In `nrf24l01` the register map was right, but sequencing, timing and registers never written were not.
- The HAL changes for `ccs811`, `max17048` and `mcp2515` landed later, see above.
- Model faults are fixed where a driver fix needed them. The rest are in each README under Still open.
- Of the 23 suspect items from the first batch, 4 are now confirmed, 9 were dismissed and 10 stay open.
- Some calls now refuse what they used to accept, or take longer: 1 Mbps on `mcp2515`, SF6 on `lora_sx1276`, rates above 800 Hz on `adxl345`, a NULL `delay_ms` on `bmm150`, and about 100 ms in `nrf24_init`.

`make test-examples` used to build the examples without running them. It runs them now, and the first real run caught a mock that a `lora_sx1276` fix had left behind.

## Datasheet check of ten drivers (2026-11-05)

All ten had defects: 67 confirmed, 23 suspect. Each driver's README lists what was checked, fixed and left open. None had run on hardware, and these fixes have not either, so they match the documents and nothing more.

- Fixed in full or nearly so: `sx1262`, `cc1101`, `bme680`, `bma400`, `bno055`.
- Not usable on hardware until a larger change lands: `bmi270` (8 KB config upload) and `vl53l1x` (port ST's driver). `pac1952` got its HAL change later, see above.
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
