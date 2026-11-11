# Changelog

## v0.1.0 (2026-11-11)

**Wheels:** 259 (L0:77 L1:115 L2:47 L3:20) | **Tests:** 919 | **Commits since initial:** 2092

### Changes by type
| Type | Count |
|------|------:|
| wheel | 1209 |
| tool | 344 |
| docs | 388 |
| ci | 57 |
| project | 6 |
| port | 34 |

### Commit log
```
be19e581 docs: version the platform, not single wheels
b68e290b docs: define candidate by the host checks
16f73b69 docs: let registration calls keep the caller's strings
d5e12750 wheel: state the lifetime of kept strings in seven headers
012e2cbe tool(new_wheel): write draft status in the draft layer
4150ef4d docs: refresh the session state after the follow-up items
7497bd06 docs: correct the counts in both readmes
ded1f904 docs: correct the task type count in the directory tree
92af18e9 docs: name the responsible human in the wheel READMEs
3f3b8037 docs(primitives/flash_ring): list the random fault test
8c1103d8 wheel(primitives/flash_ring): add a random power-cut and fault test
0caa204c docs: refresh the session state
9c9c9ced docs: record the fixes made at the close
dd8d3ee6 docs: regenerate the arm binary sizes
c5956310 docs: recount the test figures in both readmes
d7cc32ee docs(primitives/flash_ring): say when a stuck header stops push
5ba45e80 wheel(primitives/flash_ring): keep the slot the header points at
2e914ab9 docs(primitives/flash_ring): cover a push that is not read back
3b58b14a wheel(primitives/flash_ring): keep a slot it could not read back
daaf5bce docs(primitives/flash_ring): say what a failed push can leave behind
11486684 wheel(primitives/flash_ring): pin two init boundaries in the tests
21d3f5bd docs(primitives/flash_ring): note the oldest frame and header wear
bfbf3fad wheel(primitives/flash_ring): let callers read read_idx
b5e33ff9 docs: write real prototypes in two README API summaries
196435d9 wheel(protocols/msgpack): build the tests under gcc 15 with -Wextra
a8f0c03a wheel(primitives/base64): build under gcc 15 with -Wextra
b922171b docs: correct the project state in the agent instructions
8548fdbf docs: add the november rounds to the audit history
6505d38b docs: record the close of the november round in known issues
9df808df wheel(primitives/datetime): stop calling add_seconds a pure function
ff8298a1 docs: date the board runs of five wheels
91a28df4 docs(protocols/nmea_parser): correct the coordinate resolution
656dc4c5 docs: say what the 63 audit checks cover
a8df149a docs(cortex_m): mark the flybox task config as internal only
09ed422d tool(cortex_m): stop pointing at one project task config
5e228f2c tool(update_wheel_inventory): drop the zero open findings claim
602b77c8 tool(audit_wheels): say how the 63 checks split
e4be16fc wheel(primitives/flash_ring): pin the header rewrite on an old ring
6f9a47d0 docs(primitives/flash_ring): state what push returns after a fault
e6c79201 wheel(primitives/flash_ring): read old rings without losing frames
7002f4b9 wheel(primitives/flash_ring): leave a slot a failed program touched
7f34d28d wheel(primitives/flash_ring): keep the header due across resets
276d780a docs: add migration notes for the driver API changes
bcd8b33d wheel(drivers/pac1952): take the I2C address from the config
77c7c32e wheel(drivers/mcp2515): add a call to abort a pending frame
53ff9f12 wheel(drivers/icm42688): scale by the reset ranges after a soft reset
627a4942 wheel(drivers/enc28j60): leave the CRC out of received frames
1fdeb1fd wheel(drivers/ds3231): report a time lost to a stopped oscillator
e722f064 wheel(drivers/tmp102): report a first read that finds no result
4114d2a0 wheel(drivers/apds9960): report a read before the first cycle
c68fc03c wheel(drivers/ltr390): report a read with no current result
92395caa wheel(drivers/lis3mdl): report a read before the first sample
8f3c9630 wheel(drivers/air780eg): make the ISR label on feed true
48f29469 wheel(drivers/air780eg): add the data phase and drop the echo
b2414833 wheel(drivers/air780eg): report messages waiting in the module cache
a4c3daee wheel(drivers/air780eg): read +MSUB messages by their length field
00ad723a wheel(drivers/air780eg): take only the CSQ values the manual defines
b3521e2b wheel(drivers/air780eg): end commands on the replies the manual gives
49d4589e wheel(drivers/air780eg): match the URC strings the manual gives
d36b2586 docs: add migration notes for the api changes
8bd6c2e2 wheel: refuse a received packet longer than the buffer
65ddce88 wheel(drivers/gpio_button): keep every release when a gesture fires
3858e1a8 wheel: state that three wheels keep the caller's name pointer
5bbc4f1c wheel(primitives/state_machine): refuse a nested process or reset
3bf702f0 wheel(primitives/timer_wheel): settle what is due before any callback
542ca16f wheel(primitives/onewire): report a dead bus from reset and read_rom
2258d8fe wheel(primitives/kalman_1d): keep q and r across a reset
5cc9e0ab wheel(primitives/gnss_filter): hold the filtered position in double
52e8f144 wheel(primitives/ewma): keep a scaled accumulator to reach the input
526643c1 docs: add migration notes for the audit api fixes
b4ab2e44 wheel(primitives/slip): drop all of a frame that lost a byte
4b81d352 wheel(protocols/crc_any): export the finalize the header promises
4e67f8d4 wheel(primitives/heatshrink): keep -1 in every result type
6cb3dec1 wheel(protocols/dns_packet): keep the whole 16-bit type and class
f83dfd6d wheel(protocols/sntp_client): refuse a reply to another request
f2031d7b wheel(protocols/sntp_client): refuse replies with no good time in them
63636b50 wheel(modules/mqtt_client): report a publish that got no PUBACK
2b5e34fa wheel(protocols/mqtt5_packet): report the topic length of a PUBLISH
37d2e642 wheel(protocols/crsf): hold frames to the spec's 64-byte limit
fdb57bcc wheel(protocols/mavlink): stop guessing the CRC_EXTRA of a message
55aa272a wheel(protocols/at_command): add a wait for the final result code
17e2df0a wheel(protocols/http_parser): report transfer-encoded messages
76554dbb wheel(protocols/http_parser): end bodiless replies at the blank line
00d823ad docs: add migration notes for the xmodem to boot_counter api changes
fb8121d2 wheel(modules/boot_counter): make the thread safety labels true
056a0e43 wheel(modules/boot_counter): let a read say no record exists yet
a1b58deb wheel(modules/boot_counter): check the record and keep a second copy
0aa97681 wheel(protocols/ota_ab): keep two params copies across a power cut
8d172b65 wheel(protocols/ymodem): refuse a file short of its announced size
1874ec12 wheel(protocols/ymodem): wait for a quiet line after a bad frame
374de573 wheel(protocols/ymodem): cancel only on two consecutive CANs
284008c5 wheel(protocols/xmodem): wait for a quiet line after a bad frame
0a798572 docs: add migration notes for the isr and alignment fixes
bd5de689 wheel(primitives/ring_buffer): correct the CBMC harness bound note
84465310 tool(check_readme_examples): follow header includes to other wheels
100438fb wheel(primitives/timer_wheel): publish a timer only once it is whole
d5e9ae23 wheel(primitives/soft_timer): make timers safe beside an ISR tick
ab606407 wheel(primitives/fifo_queue): check raw indexes in the CBMC harness
3eb0f4df wheel(primitives/ring_buffer): bound the CBMC harness to fit verify
a470bd37 wheel(primitives/double_buffer): make a swap visible to a poller
5e671d93 wheel(primitives/fifo_queue): make one producer and one consumer safe
a042fc8a wheel(primitives/ring_buffer): make one writer and one reader safe
c3acd3d4 wheel(primitives/hmac_sha256): hold the inner hash as a sha256_t
57933604 port(gd32f4xx): drop the last hardware claim from the readme
0bb12fc8 port(gd32f4xx): list the host models in the readme
3383afde port(gd32f4xx): refuse usart baud rates the divider cannot hold
d1033329 port(gd32f4xx): keep usart baud and oversampling in step at init
9572fea3 port(gd32f4xx): clock a GPIO port when a pin on it is configured
b7ff5343 port(gd32f4xx): make shared control register updates atomic
032a4b5d port(gd32f4xx): end the i2c request on a bus error
8009f690 port(gd32f4xx): refuse i2c requests the interrupt cannot finish
5b102a9c port(gd32f4xx): make spi mode zero software NSS as the template says
76b476ab port(gd32f4xx): stop claiming a spi chip select the port cannot drive
dd9c5ca4 port(gd32f4xx): keep one spi frame in flight so rx cannot overrun
96848d38 port(gd32f4xx): add to the flash irq mask instead of replacing it
74b88649 port(gd32f4xx): refuse flash requests while the FMC is busy
abc0af63 port(gd32f4xx): erase whole sectors only
235150c1 port(gd32f4xx): refuse flash requests no interrupt can finish
f3fe603c port(gd32f4xx): keep 8 data bits when usart parity is on
1c9633a2 port(gd32f4xx): clear CTSF in the usart interrupt
d2954900 port(gd32f4xx): state that the port has never driven a board
71da88c7 port(gd32f4xx): make 10-bit i2c addressing work
9eb8e964 port(gd32f4xx): follow the manual's N=2 steps for 2-byte reads
6836a9f6 port(gd32f4xx): refuse flash writes the ISR cannot finish
9e313e35 ci: pin the runner image to ubuntu-26.04
5e416cba docs: record what the mps2_bare scaffold needs from the log wheel
07d26fba docs: correct the release, merge and mirror steps
e1258303 docs: drop the config path form no wheel implements
0386fda6 docs: correct the bus sections and mark limited parts in the matrix
85b7e7fa docs: make the README quick start and commands run as written
e61911d8 docs: fix the catalog usage example and mark limited parts
7742f725 docs: name contract.h in the wheel reuse line
08e650a6 docs: tell integrators where contract.h comes from
4fc57a73 exp(mps2_bare): restore the build and give log a time source
3ae7b4ff docs(primitives/fal): update the test count
ea95a726 wheel(primitives/fal): refuse a range that wraps past the partition
d6310a76 docs: stop offering an isr as a caller of cli_shell_feed
c314fb8d wheel(modules/cli_shell): say feed must not run in an isr
d4b350cb docs(primitives/kvs): call fal_init in the readme example
df7c8c25 wheel(primitives/kvs): keep the logs of a full tsdb across a reboot
dad4b385 wheel(primitives/kvs): stop reverse time walks losing older sectors
fcef9174 wheel(primitives/kvs): test tsdb and kv objects on a working database
21973e13 docs(protocols/nmea_parser): state what a decimal field may hold
ec22bdf0 wheel(protocols/nmea_parser): read decimals without strtof
b4b50266 docs(adr): drop a local path from adr 004
ffd1b80a tool(bootstrap): look for claude projects under any windows user
31203977 port(gd32f4xx): drop one board's detail from the port
67e39983 docs: describe the crc16 regression test without its project
e1eec960 wheel(primitives/crc16): rename the regression test neutrally
57ab8aa8 wheel(drivers/air780eg): use a neutral topic in the mqtt rx test
3eeb85e9 wheel: clear the compile audit findings
b8dc92ad wheel(drivers/opt3001): reach the read in the property test
dc99cc26 docs: correct five statements that disagree with the code
536b1a46 wheel(drivers/vl53l0x): keep the timing budget when the write fails
cc2c5ab5 wheel(drivers/hd44780): refuse a hal with no delay_ms
9904cf28 tool(gen_sentinel_tests): emit the spdx line
e0ff8193 tool(gen_boundary_tests): emit the spdx line
5492f332 docs: repair the truncated origin lines in 24 readmes
576404d6 tool(check_readme_examples): empty the pending readme list
e130cfd6 docs: match six readme signatures to their headers
eddfb703 wheel: assert that null arguments are rejected
a3453a3a tool(gen_null_tests): list the null arguments that are optional
34cc93f6 wheel(primitives/fal): name the table test flags per target
df2e6ff2 wheel: label si7021 and tmp102 bus calls as not ISR safe
8abab5fb wheel: refuse a device init did not finish in vl53l1x and w5500
2a29da0f wheel: refuse a device init did not finish in tcs34725 to vl53l0x
8c5ca9fa wheel(drivers/tsl2591): store a setting only once it is written
b063e12d wheel: refuse a device init did not finish in si7021 to tca9548a
7efcbe81 wheel: refuse a device init did not finish in sgp30 to shtc3
5fb9517f wheel: refuse a device init did not finish in pcf8574 to sd_spi
0b23fa42 docs: correct gpio_button and gpio_led notes on refused inits
d732038f wheel: label blocking calls in ds18b20 and hmc5883l as not ISR safe
8e7bfc84 wheel: refuse a device init did not finish in lis2dh12 to lis3mdl
bb680b7c wheel: refuse a device init did not finish in is31fl3731 to st7789
838e5735 wheel: refuse a device init did not finish in icm42688 to ina3221
c50b300b wheel: refuse a device init did not finish in hmc5883l to hx711
d13549e5 wheel: refuse a device init did not finish in gpio_button to hd44780
5351455a wheel: refuse a device init did not finish in ds18b20 to gc9a01
a2f5a866 tool(test_audit_wheels): keep pytest from collecting the decorator
155af8e1 docs: point tooling readmes at the moved patch and capture paths
122beb81 tool(bootstrap): warn when the git hooks are not active
273ec2be tool(githooks): mark the hooks executable
3d899771 tool(wheel_promote): follow every consumer and fail a broken move
77e7786b tool(wheel_gate): fail when the audit itself fails
966ec357 tool(wheel_gate): fail a wheel with no test run or no readme
dff37fb9 tool(fix_readme_warnings): stop stamping reviews nobody did
f95018f0 tool(batch_fix): roll back and exit 1 when a fix fails
4f642b52 tool(gen_null_tests): keep length 0 where nothing can reject
b9750804 tool(test_mutation): fail on a bad baseline and score what compiled
dac4ee15 tool(test_differential): build the drivers and fail when one cannot
c7cc2496 tool(test_all): exit 2 when a filter matches nothing
73bc2f40 tool(run_libfuzzer): fail when nothing was fuzzed
6d3cf539 tool(wheel_gate): skip test/build folders in the spdx check
8418f518 tool(gen_null_tests): make _err_t functions reject null
647e88a6 tool(gen_null_tests): emit the spdx line
12e3efd2 tool(run_generated_tests): stop dropping tests and empty runs
a95cbede tool(dispatch_agent): let a failed squash merge reach its handling
fa4ad1de tool(dispatch_agent): keep worktrees that hold uncommitted work
55f645a4 tool(bootstrap): survive having no wheel source since the last audit
abfb7b60 tool(build_flash_check): point at the moved serial capture script
7b012f87 tool(stack_analyzer): follow tail calls and fail on missing data
e5358019 tool(patches): make vsf patching work from tooling/ci/patches
335e4918 tool(run_frama_c): exit non-zero when the proofs fail
396a24df tool(memory_audit): stop auditing another project's memory
aeb10766 tool(check_readme_examples): fail on stale wheel api examples
902a0c9c docs: align four readme api blocks with their headers
8ded6c9c tool(check_line_endings): read the whole blob when counting CRs
67aba418 tool(audit_wheels): make the c99 and -Werror checks able to fail
31c5d63b tool(audit_report): count a crashed tool as a failure
880e8d6b tool(makefile): make verify fail on cut loops, hangs and no cbmc
64181d53 wheel(primitives/ring_buffer): include stddef.h in the cbmc harness
94a832ad docs: correct driver readmes after the init fixes
69d39a40 wheel: label more blocking calls as not ISR safe
12f1344e wheel: refuse a device init did not finish in dps310 to drv8833
eb722f9b wheel: refuse a device init did not finish in bq27441 to dht11
6016e9d1 wheel: refuse a device init did not finish in bme280 to bmp388
20e01303 wheel: refuse a device init did not finish in apds9960 to bma400
2fc7b842 wheel: refuse a device init did not finish in a4988 to apa102
caefeecd docs: state the limits and behaviour the audit fixes changed
325f69c5 wheel(modules/telemetry): test the limit of the whole buffer
56cd81b6 wheel(modules/ota_manager): test failed activate and large progress
8569ddcc wheel(modules/mqtt_client): test timers from a real start time
c17d50b6 wheel(modules/param_table): test list against the buffer size
2c1dacbf wheel(modules/config_store): test float set and load validation
db072020 wheel(modules/flash_journal): pin a record and read after two boots
175cb0a0 wheel(modules/task_watchdog): test tasks past the first
1a5bd767 wheel(modules/telemetry): limit a field only by the buffer
ec8db7c4 wheel(modules/telemetry): check the float text before sending it
6cb341dd wheel(modules/task_watchdog): keep a fed disabled task disabled
d3b4143b wheel(modules/task_watchdog): restart a task registered twice
6d235039 wheel(modules/sensor_hub): reject infinite readings
f08794a5 wheel(modules/scheduler): survive a task removed during a pass
581c86f6 wheel(modules/power_manager): stop calling activity isr safe
b118292f wheel: refuse limits that a uint8_t count cannot hold
3f40296f wheel(modules/mqtt_client): stop calling next_id isr safe
c09bf026 wheel(modules/mqtt_client): ping when nothing was received
9023507c wheel(modules/flash_journal): give up a sector after a failed write
d8c6f9ed wheel(modules/flash_journal): fail init when a read fails
c2caf0f7 wheel(modules/event_bus): never drop an unsubscribe during publish
4b139b55 wheel(modules/data_logger): write complete records only
a98608c4 wheel(modules/data_logger): refuse strings that break the csv
7fcf582c wheel(modules/telemetry): format floats without printf
5b8a6dc9 wheel: test a zeroed HAL in nrf24l01 to pac1952
fc61110f wheel: test a zeroed HAL in mcp2515 to ms5611
ccb594c8 wheel: test a zeroed HAL in lora_sx1276 to mcp23017
2fadf249 wheel(drivers/mlx90614): label the temperature reads not ISR safe
29166593 wheel: refuse a device init did not finish in ms5611 to pcf8563
4812e5b4 wheel: refuse a device init did not finish in mcp2515 to mpr121
ccd92129 wheel: refuse a device init did not finish in max30102 to mcp23017
bfd3e68c wheel: refuse a device init did not finish in lm75 to max17048
cc2dacfb wheel(primitives/fal): spell out the table test targets
c306f2df docs: update wheel readmes for the audit fixes
6455482e wheel(primitives/log): offer printf_tiny and state the stack need
969544ce wheel(primitives/checksum): pin fletcher16 and odd-length answers
778d068a wheel(primitives/sha256): refuse NULL data with a non-zero length
becbb6e7 wheel(primitives/ring_buffer): return early from an empty skip
9fd467d1 wheel(primitives/printf_tiny): stop at the first unknown specifier
8572da2c wheel(primitives/printf_tiny): saturate the field width while parsing
dcb5f3c2 wheel(primitives/pid_controller): keep NaN and inf off the output
60f9f68c wheel(primitives/moving_rms): guard feed on a missing buffer
5db8d221 wheel(primitives/moving_rms): resync the sum each time head wraps
86890948 wheel(primitives/mahony_ahrs): integrate the gyro without accel
55b965c5 wheel(primitives/mahony_ahrs): keep non-finite input out of the state
25f77fab wheel(primitives/mag_cal): refuse non-finite samples and results
d7c17776 wheel(primitives/lookup_table): refuse NaN in the x array
775fc5c0 wheel(primitives/log_flash): retry a failed erase before moving on
c861035b wheel(primitives/log_dedup): strip only all-digit timestamp brackets
0664a4f5 docs: correct driver readmes after the audit fixes
335671b0 wheel(drivers/pcf8563): refuse dates that do not exist
22ace225 wheel(drivers/bme688): stop compensating skipped channels
0c0cf821 wheel: label blocking calls as not ISR safe
8cf86f8f wheel(drivers/stepper): assert exact delays over one whole move
817c1de0 wheel(drivers/veml7700): refuse a device init did not finish
11463a9f wheel(drivers/veml7700): store a setting only once it is written
0d33ec1a wheel(drivers/pca9685): refuse a device init did not finish
30c4d990 wheel(drivers/bno055): refuse a device init did not finish
891f29c1 wheel(drivers/stepper): refuse a device init did not accept
be770577 wheel(drivers/servo): follow set_pulse in the reported angle
654ce173 wheel(drivers/servo): refuse a device init did not accept
aac116b2 wheel(drivers/sd_spi): build with SD_SPI_RETRY_LIMIT overridden
c177b1af wheel(drivers/sd_spi): refuse blocks past the byte address range
9797a4dc wheel(drivers/rc522): refuse a UID that needs more cascade levels
20bfcb1f wheel(drivers/pca9685): wake after a failed prescale write
27a8e44e wheel(drivers/bme688): refuse a device init did not finish
8a270c89 wheel(drivers/bme680): refuse a device init did not finish
ad67b499 wheel(drivers/bno055): refuse a HAL without write and read
7fc1bc9b wheel(drivers/mpu6050): refuse a device init did not finish
91cf5ca3 wheel(drivers/mpu6050): refuse a HAL without write and read
0e2c6a4b wheel(drivers/max31855): report a fault bit with no cause
3f2182a4 wheel(drivers/keypad_matrix): hold up to 16 columns in a row mask
4c60205b wheel(drivers/hcsr04): wait out the last pulse before triggering
e3ef1f9d wheel(drivers/drv8833): clamp speed in 32 bits
05d72ae4 wheel(drivers/bno055): track the mode through a failed change
6f2c2bc7 wheel(drivers/bme688): keep compensation defined past 97 C
d114e83a wheel(drivers/bme680): keep compensation defined past 97 C
15573059 docs(primitives/flash_ring): describe capacity and power loss
4005dba5 wheel(primitives/flash_ring): run property and stress mocks as NOR
b6f3f7d7 wheel(primitives/flash_ring): keep peek inside the live frames
1d7dd9f7 wheel(primitives/flash_ring): erase ahead and find the write position
95228731 docs(protocols/nmea_parser): list GGA position and the checksum check
48a27244 wheel(protocols/nmea_parser): take the position from GGA
6a38e235 docs: correct the READMEs after the protocol fixes
be9ab8bf wheel(protocols/isotp): test long messages, padding and reset
4bad3a97 wheel(protocols/modbus_rtu): test FC16 replies and the 125-reg read
63ba18ee wheel(protocols/canbus): test that packing clears old bits
ae199baf wheel(protocols/onewire_search): retry after a glitch, not stop
823738cf wheel(protocols/onewire_search): refuse a ROM with family code 0
1d394e19 wheel(protocols/onewire_search): move on only after the CRC passes
999b9a19 wheel(protocols/nmea_parser): ignore values out of range
92ad3da5 docs(protocols/ncp5623_parser): say the protocol is ToshibaLED
9baf0637 wheel(protocols/ncp5623_parser): name the protocol it really parses
8c8f5c36 wheel(protocols/mqtt_packet): refuse a PUBLISH cut short
97dfa67b wheel(protocols/mqtt5_packet): skip properties it does not store
a7387cda wheel(protocols/json_rpc): escape method and message text
b2d9610b wheel(protocols/json_rpc): match only top-level keys
30d86812 wheel(protocols/isotp): end the reception on a wrong sequence number
27da1533 wheel(protocols/intel_hex): refuse records that do not fit their type
08e998d7 wheel(protocols/ibus): keep only the 12 channel bits when decoding
57967c81 wheel(protocols/http_request): refuse line breaks in request fields
52a89954 wheel(primitives/kvs): return from tsdb iterators after failed init
2e82ab52 wheel(primitives/kvs): survive a missing partition in messages
e0751254 docs(primitives/kvs): state the stack fdb_kv_set needs
1bf03494 wheel(primitives/kvs): stop a bad tsdb sector inheriting a status
6cbebe01 wheel(primitives/string_utils): test truncation at every size
051a8fc2 wheel(primitives/byte_buffer): test checks on a partly used buffer
fc870539 wheel(primitives/varint): refuse a tenth byte past 64 bits
ce7c0e31 wheel(primitives/timer_wheel): ignore tick on a wheel with no slots
1c2a4257 wheel(primitives/string_utils): refuse floats past the float range
e9cb7eab wheel(primitives/soft_timer): reload a fired one-shot on start
c499cea8 wheel(primitives/soft_timer): let a one-shot re-arm from its callback
5560a28a wheel(primitives/soft_i2c): report SDA held low as a bus error
e745a19c wheel(primitives/soft_i2c): refuse addresses above 7 bits
6d50e27c docs: correct READMEs for the cli and fal fixes
e414dc11 wheel(modules/cli_shell): refuse a line that overflowed
518ac98f wheel(primitives/cli_parser): refuse a fed line that overflowed
fdc86aa0 wheel(primitives/gnss_filter): count only time the quality has held
fb684e36 wheel(primitives/gnss_filter): keep NaN out of the gate and the EMA
02a3dc64 wheel(primitives/gnss_filter): wrap the longitude EMA at 180 degrees
ddab5ec7 docs(primitives/gnss_filter): complete the usage example input
592fefa2 wheel(primitives/framebuffer): index blit sources past 64 KB
bef03558 wheel(primitives/framebuffer): clip sizes that pass 65535
6e4dfc49 wheel(primitives/fixed_point): saturate fp_to_int_rounded at the top
f289af7a wheel(primitives/fixed_point): format FP_MIN from its true magnitude
036adb49 wheel(primitives/fal): report a failed device init
828bf0ea wheel(primitives/fal): check partitions against their device
b398507a wheel(primitives/fal): refuse partition calls before fal_init
73690fee wheel(primitives/event_queue): read head and tail once in count
bb4ce0d6 wheel(primitives/cli_parser): mark feed and run not ISR safe
a44cad5f wheel(primitives/cli_parser): skip table entries with holes
154bdced wheel(primitives/bitfield): mark the writers caller_must_lock
43072eaa wheel(primitives/base64): count the partial group in decoded size
a815016d wheel(primitives/gnss_filter): refuse months outside 1..12
d8139cf7 wheel(primitives/framebuffer): hold line error terms in int32_t
063a2d0f docs: correct protocol READMEs after the audit fixes
cc5091ba wheel(protocols/ymodem): test duplicate blocks and a two-file batch
13dddf05 wheel(protocols/ymodem): saturate sizes just past 32 bits
9be601ff wheel(protocols/ymodem): refuse a name that does not fit
bae6c481 wheel(protocols/xmodem): correct the total_bytes comment
cfe14112 wheel(protocols/tftp): refuse DATA larger than a block
4d00f258 wheel(protocols/sntp_client): correct the sign of offset_ms
0c9f83f6 wheel(protocols/sntp_client): follow the RFC 4330 era rule
802ca1a5 wheel(protocols/s_record): refuse text after the checksum
f0d16d76 wheel(protocols/s_record): refuse an address the type cannot hold
1b3640d6 wheel(protocols/protobuf_lite): refuse fields field_num cannot hold
2442685a wheel(protocols/ppp_frame): decode without a frame buffer
1a6d62de wheel(protocols/ota_ab): refuse a boot limit of 255
128bf1ec ci: make change detection and the ARM step able to fail
16053b55 docs: record the first super audit fixes
82beea63 wheel(primitives/flash_ring): refuse slots that straddle sectors
56b02bb7 wheel(primitives/fixed_point): widen the 5-place fraction product
ffa05f6d wheel(protocols/http_parser): strip trailing whitespace from values
bc7fc6b0 wheel(protocols/http_parser): refuse lengths that cannot frame a body
6db6c4a7 wheel(protocols/hdlc): take one flag as the end and start of frames
11df1449 wheel(protocols/dns_packet): require the root label after a pointer
c3fd9f74 wheel(protocols/cbor_lite): refuse floats and simples it cannot read
5e816839 wheel(protocols/bencode): refuse input that ends inside a container
09435ae5 wheel(protocols/at_command): accept the prompt only at line start
ba329390 wheel(protocols/at_command): keep URC lines out of prev_line
b2fe22de wheel(protocols/lorawan_packet): stop the frame size wrapping
8737deba wheel(protocols/crc_any): feed the message in bit by bit
378d052b tool(audit): count uncommitted work in --changed
e4eb7d4f tool(makefile): keep the commit message away from the shell
976733e3 tool(ci): let arm_size_audit.sh decide the ARM step
2f223d3b tool(audit_coverage): measure the wheel, not its test file
52b10c81 tool(pre-commit): stage only drift files the tool changed
75fc5044 tool(pre-commit): check real paths, any case, modified files
631aad28 tool(hil_runner): report this run's verdict and exit on it
1e95dfd8 tool(build_flash_check): stop when the build fails
35037213 tool(audit_deep): count sanitizer and stress build failures
485bb2c9 tool(audit_compile): fail on warnings and failed builds
668b48d8 tool(test_all): keep every result under --parallel
120eda5e tool(test_all): run every test target of a wheel
d9fba438 wheel(modules/data_logger): format floats without printf
edaa0cb0 wheel(modules/config_store): load past a key that fails to load
701c62a0 wheel(modules/command_queue): mark peek as not thread safe
780330a6 wheel(modules/button_handler): debounce the release edge
901c5a5f wheel(modules/ota_manager): bound the wait for 0-byte reads
cdf5b5f2 wheel(modules/mqtt_client): stop dispatch when a callback reconnects
db8d5e52 wheel(modules/flash_journal): retire a dirty write sector at init
6c9730af wheel(modules/error_handler): reject a loaded count above the maximum
e08559c5 wheel(modules/config_store): check a loaded string with memchr
af6df539 wheel(modules/config_store): reject a stored bool other than 0 or 1
13eec8ce docs: state the memory_pool alignment rule in its README
2f83503d wheel(primitives/moving_rms): reject samples that overflow the sum
80ec8c24 wheel(primitives/signal_stats): reject samples that poison the state
1bdd4f91 wheel(primitives/peak_detector): reject infinite samples
72942faf wheel(primitives/moving_window): reject values that overflow the sums
068a7606 wheel(primitives/moving_average): reject infinite samples
c7903b8a wheel(primitives/soft_timer): refuse start on a destroyed slot
ca940f89 example(power_monitor): align the memory_pool storage
b3979327 wheel(primitives/memory_pool): refuse misaligned buf and block_size
7f55a42f docs: state the move_to distance error in the stepper readme
ea4f7bbc wheel(drivers/a4988): keep STEP low for 1 us in a4988_step_n
9f28ecc0 wheel(drivers/stepper): refuse a move_to distance beyond int32_t
fad28dea wheel(drivers/stepper): hold the peak delay in an odd triangle move
c58fd107 wheel(drivers/stepper): keep the ramp delay in fixed point
3c077a4a docs: correct the log and kvs readmes after the audit fixes
82d6d7ea wheel(primitives/log): make LOG_LEVEL_MAX filter at compile time
048d3daf wheel(primitives/log): stop claiming log_emit is thread safe
f619cb8b wheel(primitives/kvs): refuse keys of FDB_KV_NAME_MAX characters
f16bd828 wheel(primitives/kalman_1d): skip samples that are not finite
0e8fd3f2 wheel(primitives/json_parser): split value state by context
35ccf7b7 wheel(primitives/hmac_sha256): zero the mac on a failed compute
00f41da5 wheel(primitives/heatshrink): report a partial decoder sink as full
01ca16ec wheel(primitives/kvs): reject records with an overlong name
1ffb1b37 wheel(primitives/heatshrink): end decoder finish on padding bits
14658024 docs(protocols/midi_packet): record that a cut sysex is dropped
fa624c6b wheel(protocols/midi_packet): end sysex on any non-real-time status
b2b19efb docs: record the suspects settled from second sources
78a02b70 tool(makefile): run the changed-wheel sanitizer audit in parallel
11e7e645 docs: record the suspects settled in 13 drivers
72033c8c wheel(drivers/pcf8563): report VL from pcf8563_read_time
25841d67 wheel(drivers/mpu6050): wait for the reset in init
b15f8758 example(motion_control_adv): clear DEVICE_RESET in the mock MPU-6050
bb44587e wheel(drivers/vl53l0x): encode the final range timeout as ST does
0f326afb wheel(drivers/apds9960): set gesture pulses and gain
149c342e wheel(drivers/cst816s): accept the known chip IDs in init
c2f6b132 wheel(drivers/drv2605): select a ROM library in init
7628c722 wheel(drivers/mcp2515): set zero filters for both frame formats
f38cd564 wheel(drivers/rc522): send HLTA with CRC_A
eb45869f wheel(drivers/sk6812): state that 24-bit parts are not supported
bf45e639 wheel(drivers/ws2812): derive each slot inside the bit loop
a49ffdf9 wheel(drivers/tcs34725): compute color temperature as DN40 does
e361d3fa docs: record the test model corrections
d0e5471a docs: record the model fixes of 13 drivers
1baca5ad wheel(drivers/air780eg): test +MSUB with the manual's own sample
dfd12ea7 wheel(drivers/qmc5883p): clear DRDY on a status read in the model
cdd22bbd wheel(drivers/qmi8658a): make the test model follow the manual
5d3eb332 wheel(drivers/w25qxx): make the test model follow the manual
742027a2 wheel(drivers/dht11): add a test model built from the bus timing
a2f9b227 wheel(drivers/dht11): hold the start longer and release the bus
6aba0438 wheel(drivers/at24cxx): make the test model follow the manual
376116a5 wheel(drivers/at24cxx): require delay_ms in init
39c3163d wheel(drivers/at24cxx): wait a write cycle after each fallback byte
ed7f2f66 wheel(drivers/bh1750): ignore Reset in Power Down in the model
6032f505 wheel(drivers/bmp280): make the test model follow the manual
31c1a517 wheel(drivers/bmp280): wait out start-up, soft reset and the NVM copy
cd0a062f wheel(drivers/bmp280): wait until a conversion has really ended
d1e19f72 example(env_monitor): end the mock BMP280 reading in sleep mode
a1d72855 example(mqtt_telemetry_demo): end mock BMP280 reads in sleep mode
8b27b981 wheel(drivers/bme280): make the test model follow the manual
f3df69b5 wheel(drivers/bme280): wait out start-up, soft reset and the NVM copy
cd50fc2a wheel(drivers/bme280): wait until a conversion has really ended
e45a2358 example(smart_home): end the mock BME280 measurement in sleep mode
b2e3516b wheel(drivers/aht20): make the test model follow the manual
e3f89ea5 wheel(drivers/sht30): make the test model follow the manual
dd0b74a1 wheel(drivers/sht30): wait out power-up and soft reset
ca7690b5 wheel(drivers/hx711): test the 24-bit limits from the manual
2a3f8358 wheel(drivers/pca9685): make the test model follow the manual
ee8dd7b9 wheel(drivers/pca9685): keep RESTART when sleep is called twice
6913a199 wheel(drivers/ds18b20): make the test model follow the manual
29c0b134 docs: record the model fixes of 10 drivers
af4a846c wheel(drivers/max31865): make the test model follow the manual
c9f8e561 wheel(drivers/max31865): report not ready before the first conversion
764c2030 wheel(drivers/sgp30): make the test model follow the manual
e0425a28 wheel(drivers/mlx90614): make the test model follow the manual
77951634 wheel(drivers/drv2605): make the test model follow the manual
ebf1d393 wheel(drivers/tcs34725): report not ready until a cycle ends
53b26a9b wheel(drivers/tcs34725): make the test model follow the manual
2799b0d6 wheel(drivers/ht16k33): make the test model follow the manual
2687677e wheel(drivers/ccs811): make the test model follow the manual
c8e5316b wheel(drivers/ccs811): send APP_START only from boot mode
c3a774a5 wheel(drivers/adxl345): make the test model follow the manual
59a6f306 wheel(drivers/adxl345): wait the turn-on time in init
38f1be37 wheel(drivers/tsl2591): make the test model follow the manual
178e24b3 wheel(drivers/tsl2591): report not ready until an integration ends
92b05a31 wheel(drivers/rc522): make the test model follow the manual
bfe2462d docs: record the model fixes of 6 drivers
55255bcc wheel(drivers/ili9341): make the test model follow the manual
ffec4e1e wheel(drivers/oled_ssd1306): make the test model follow the manual
fc809595 wheel(drivers/icm42688): make the test model follow the manual
6c70682d wheel(drivers/max30102): make the test model follow the manual
6d82f512 wheel(drivers/w5500): make the test model follow the manual
e90a85ff wheel(drivers/enc28j60): make the test model follow the manual
1bba9433 wheel(drivers/enc28j60): reset the transmit logic before each send
849f9876 docs: record the model fixes of 10 drivers
9caa3e60 wheel(drivers/mcp3008): make the test model follow the manual
83095661 wheel(drivers/mcp3008): bring CS high in init
bbaf7cd4 wheel(drivers/sgp40): compile the test model and follow the manual
4d8324e9 wheel(drivers/tmp102): make the test model follow the manual
a66d0e38 wheel(drivers/gc9a01): make the test model follow the manual
e158d295 wheel(drivers/apds9960): make the test model follow the manual
9f0580a8 wheel(drivers/ds3231): make the test model follow the manual
b77cb6df wheel(drivers/mcp23017): make the test model follow the manual
4482c8cd wheel(drivers/ina226): make the test model follow the manual
bdd073cc wheel(drivers/ina226): round the calibration and scaled readings
23787b7e wheel(drivers/lis3mdl): make the test model follow the manual
f440cd7d wheel(drivers/nrf24l01): make the test model follow the manual
9c3eadb0 wheel(drivers/nrf24l01): refuse a send in RX mode
4076e1b4 docs: record the model fixes of 10 drivers
7a4bade8 wheel(drivers/mcp4728): give the test model an EEPROM and read-back
9c8bfed4 wheel(drivers/htu21d): send the humidity status bit in the model
9a142df3 wheel(drivers/tmp117): make the test model follow the manual
4f608ac7 wheel(drivers/hmc5883l): make the test model follow the manual
288755b2 wheel(drivers/hmc5883l): wait a full period for the second sample
5e9f6b21 wheel(drivers/aw9523): test the driver against a corrected model
2c29ec4c wheel(drivers/ina219): model CNVR and OVF in the bus voltage register
463c3dc8 wheel(drivers/mpr121): make the test model follow the manual
2d97ac6d wheel(drivers/lora_sx1276): make the test model follow the manual
ba8a1382 wheel(drivers/lsm6dso): model STATUS_REG and the self-clearing bits
21823362 wheel(drivers/opt3001): make the test model follow the manual
c6c7bb70 docs: record the model fixes of 9 drivers
0ad8a294 wheel(drivers/ms5611): make the test model follow the manual
5de58386 wheel(drivers/ms5611): refuse an ADC result of 0
a806c651 wheel(drivers/ina3221): make the test model follow the manual
7a333adc wheel(drivers/cst816s): test the AutoSleepTime reset value
39a00b1b wheel(drivers/bme688): give field 0 its reset state in the model
370f0007 wheel(drivers/sht40): make the test model follow the manual
3ec4f8cd wheel(drivers/sht40): wait out tSR after a soft reset
64c4e6df wheel(drivers/pct2075): make the test model follow the manual
ea580563 wheel(drivers/pct2075): wait for the first conversion in init
19bcfbe4 wheel(drivers/max17048): test the model rules from the manual
67c7de24 wheel(drivers/mcp4725): make the test model follow the manual
9d2bf256 wheel(drivers/mcp4725): refuse writes during the EEPROM cycle
74359b36 wheel(drivers/ads1115): make the test model follow the manual
021efdb6 wheel(drivers/ads1115): keep polling through an 8 SPS conversion
6435d57e docs: record the model fixes of 9 drivers
9a6e2881 wheel(drivers/tca9548a): connect channels at the stop in the model
70f7296b wheel(drivers/ltr390): make the test model follow the manual
c898bb73 wheel(drivers/lis2dh12): make the test model follow the manual
9bda8047 wheel(drivers/bmi160): sample only in the model's running sensors
96fcada5 wheel(drivers/veml7700): keep the registers apart in the mock
eb91ac5b wheel(drivers/veml7700): check gain and time in the setters too
1aaf6b31 wheel(drivers/shtc3): make the test model follow the manual
a237be2e wheel(drivers/shtc3): wake before reset and wait until idle
35821033 wheel(drivers/bmp388): make the test model follow the manual
436072c3 wheel(drivers/bmp388): finish the reset and restore OSR after it
a1079e6c wheel(drivers/scd40): make the test model follow the manual
ada0b8e4 wheel(drivers/scd40): refuse idle commands during a measurement
edad9fa7 wheel(drivers/mcp2515): make the test model follow the manual
d4e5af36 wheel(drivers/mcp2515): refuse a send the chip cannot transmit
5d9b40d1 docs: record the model fixes of 9 drivers
7b5d8b16 wheel(drivers/max31855): make the test model follow the manual
f869c264 wheel(drivers/max7219): latch model frames on LOAD like the chip
cd282e79 wheel(drivers/pcf8563): make the test model follow the manual
dd09c1f4 wheel(drivers/lis3dh): make the test model follow the manual
9de26a2a wheel(drivers/ads1015): make the test model follow the manual
da410126 wheel(drivers/st7735): make the test model follow the manual
dbf4e5e0 wheel(drivers/dps310): make the test model follow the manual
e7799de9 wheel(drivers/dps310): wait out the no-access time in init
218a2edf wheel(drivers/si7021): make the test model follow the manual
7f3cc8ee wheel(drivers/sd_spi): make the test models follow the card spec
4f62669a ci: install the ARM toolchain from a cache
311a81a6 docs: record the datasheet check of the last 22 drivers
805f8b87 docs: record the datasheet check of 5 drivers
3654897d wheel(drivers/aht20): require delay_ms in init
e6acf236 example(air_quality): give the mock AHT20 a delay_ms
177fb50a wheel(drivers/aht20): keep waiting while the sensor is busy
26d84434 wheel(drivers/sht30): require delay_ms in init
949c4872 example(env_monitor): give the mock SHT30 a delay_ms
6c574e2a wheel(drivers/hx711): clock the gain in at init and power up
5217e2f3 wheel(drivers/hx711): wait for each conversion in tare
97b5cb17 wheel(drivers/pca9685): stop calling the MODE1 write a reset
be4304af wheel(drivers/pca9685): scale the duty cycle by 4096 counts
2770cd0c wheel(drivers/pca9685): require delay_ms in init
191a96b1 example(motion_control_adv): give the mock I2C a delay_ms
1f154776 wheel(drivers/pca9685): refuse equal ON and OFF counts
474fb261 wheel(drivers/pca9685): keep RESTART out of the write that wakes
7136f19c wheel(drivers/pca9685): leave the chip awake after the prescaler
35613ba3 wheel(drivers/ds18b20): drop undefined bits below 12-bit resolution
65f93d92 wheel(drivers/ds18b20): compute the CRC from the polynomial
1227e72d docs: record the datasheet check of 3 drivers
e3f9e790 wheel(drivers/oled_ssd1306): refuse sizes the chip cannot drive
49a10bb0 wheel(drivers/enc28j60): start ERXRDPT odd as the errata requires
cec22059 wheel(drivers/enc28j60): stop reception while the MAC address changes
e3dcabf6 wheel(drivers/enc28j60): keep sent frames within MAMXFL with the CRC
4e95c0a2 wheel(drivers/enc28j60): cancel the transmission when send times out
734e0473 wheel(drivers/enc28j60): report a transmit abort instead of success
89ee840b wheel(drivers/enc28j60): set MACON4.DEFER as IEEE 802.3 requires
6278233c wheel(drivers/enc28j60): stop half duplex loopback of sent frames
b6a81c1d wheel(drivers/enc28j60): clear PHCON1.PDPXMD to match half duplex
5bcd5fcd wheel(drivers/enc28j60): poll CLKRDY and require a delay after reset
e40d4393 docs: record the datasheet check of 6 drivers
fc8c0f90 wheel(drivers/mpu6050): leave the chip configured after a reset
9fb1f786 wheel(drivers/at24cxx): wait the longest write cycle by default
bd1a71ce wheel(drivers/bh1750): restart measuring after power on
88118cad wheel(drivers/bh1750): measure again on each one-time read
7d43c458 wheel(drivers/bh1750): wait for a measurement before reading it
f8e73a06 wheel(drivers/bmp280): cite section 8.2 for the fixed-point code
852ced41 wheel(drivers/bmp280): stop compensating skipped channels
38573c6e wheel(drivers/bme280): stop calling double pressure code integer
8e7d9299 wheel(drivers/bme280): stop compensating skipped channels
aa1ac199 docs: record the datasheet check of six drivers
4f4fd794 wheel(drivers/icm42688): correct the FS_SEL field in comments
55deac7c wheel(drivers/icm42688): report no data before the first sample
e0c5b75a wheel(drivers/icm42688): refuse accel LP above 500 Hz
f38bb292 wheel(drivers/icm42688): stop calling standby an accel mode
3d54a045 wheel(drivers/icm42688): refuse low-power mode for the gyro
3b61d34a wheel(drivers/icm42688): refuse accel ODRs that LN mode lacks
3069a84e wheel(drivers/icm42688): refuse reserved gyro ODR codes
1cac8909 docs: record the datasheet check of max30102 and w5500
5387b798 wheel(drivers/w5500): wait for send_ok or timeout after send
39bf8677 wheel(drivers/w5500): poll sn_sr until open and close have finished
875e8b43 wheel(drivers/w5500): read the size registers until two reads agree
0312bba3 wheel(drivers/w5500): refuse a zero length recv buffer
6ec6bda1 wheel(drivers/w5500): refuse macraw on any socket but 0
19eeeab9 wheel(drivers/max30102): cite data sheet 19-7740 rev 0
3e8dfd60 wheel(drivers/max30102): refuse init without a delay callback
6a6a620f wheel(drivers/max30102): set the 6.4 ma default from table 8
9727e3c3 wheel(drivers/max30102): clear the fifo after every mode write
d8bf5633 wheel(drivers/max30102): refuse a rate the pulse width cannot reach
d3e3d6f3 wheel(drivers/max30102): read a full fifo instead of seeing it empty
689e37e4 docs: update the review figures for all 104 drivers
cca61b50 ci: move the nightly schedule off the top of the hour
70003aea ci(arm-size-audit): retry a hung toolchain install
604e57b2 docs: record the approved API fixes
7ac52394 docs: record the API fixes of is31fl3731, mcp4728, mcp2515 and bmp388
29e645d0 wheel(drivers/bmp388): really skip a measurement set to SKIP
04b8a40c wheel(drivers/mcp2515): wake the chip when leaving Sleep
4c64bb72 example(can_gateway): give the mock SPI a delay_us
64bf597f wheel(drivers/mcp2515): wait out the OST after the SPI reset
da2f6663 wheel(drivers/mcp4728): carry reference and gain in Multi-Write
f2030717 wheel(drivers/is31fl3731): stop reading LED Control while running
1b5698ef docs: record the API fixes of tca9548a, mlx90614 and pac1952
4f8c6737 wheel(drivers/pac1952): correct the VSENSE and power-up notes
2657db61 wheel(drivers/pac1952): set the sample rate with a Block Write
8a9b3f3b wheel(drivers/pac1952): send REFRESH as a Send Byte at 0x00
0a3c28bf wheel(drivers/mlx90614): read through a repeated START
3fd0462d wheel(drivers/tca9548a): drop the register byte from every transfer
df8f9c7c docs: record the API fixes of ccs811, max17048 and ms5611
efd0ad26 wheel(drivers/ms5611): refuse an all-zero prom
62d207a5 wheel(drivers/ms5611): send reset and convert as one byte
09db2807 example(power_monitor): give the max17048 mock a word write
f26e6f2b example(lowpower_sensor_node): give the max17048 mock a word write
fbeeecfa wheel(drivers/max17048): write registers as one 16-bit transaction
9b308b7f example(air_quality): give the ccs811 mock a raw write
be0c9003 wheel(drivers/ccs811): write env data as one 4-byte transaction
fe9d6b33 wheel(drivers/ccs811): send app start as one byte with no data
6d555089 docs: drop the drivers that no longer wait on a HAL change
24169acc docs: record what the full sanitizer audit found
7f691b67 tool(wheel_gate): run ASan and the property tests in the gate
8be94f60 tool(audit_sanitizer): run every test target in every wheel
8e6c0ff2 wheel(protocols/modbus_rtu): stop a test overlapping its own buffers
3a5245db wheel(drivers/bme280): stop shifting negative humidity calibration
bd793e25 wheel(drivers/bmp280): keep compensation defined on bad calibration
79dc7e73 docs: separate driver and model counts in known issues
2f248450 ci(sanitizers): run the sanitizer audit every night
73b2441c ci(arm-compile-check): retry a hung toolchain install
b917b615 docs: separate driver and model fault counts
a6b56dc3 docs: update the review figures for the last batch
06bcfaa5 docs: mark vl53l0x as not usable on hardware yet
8eb6ef67 docs: record the last datasheet batch
f7692915 docs: update the bme688 notes on the BME680
3858a3e2 wheel(drivers/bme688): stop calling the register map the same
26f72b97 wheel(drivers/bme688): clamp a negative humidity to zero
df33d349 docs: record the datasheet check of four drivers
c6f5f0fd wheel(drivers/bmp388): cite section 9.2 for temperature
17876231 wheel(drivers/bmp388): write the OSR codes from the manual
0caca1d5 wheel(drivers/bme688): wait out the heater phase in bme688_read
51dc019a wheel(drivers/bme688): require heat_stab_r for a valid gas result
07b59839 wheel(drivers/bme688): compute the heater code as the manual does
51bb8781 wheel(drivers/bme688): correct humidity reading 4.096 times high
bdda3792 wheel(drivers/bme688): use the BME688 gas resistance formula
e3027cfe wheel(drivers/bme688): read gas data from 0x2C and 0x2D
ca265384 wheel(drivers/bme688): set run_gas at bit 5 of ctrl_gas_1
b206e3d1 docs: record the local change to vendored FlashDB
7d43d17c wheel(primitives/kvs): widen a vendored byte before shifting by 24
66726f84 wheel(primitives/scale_map): survive the full int32 range
0bbc2ee9 wheel(protocols/crc_any): refuse a bad width in update
e0186f16 wheel(primitives/byte_buffer): skip memcpy for zero-length copies
75bf82d9 wheel(drivers/bmp280): multiply where the compensation shifted signs
5f9178fc docs: say how the max31865 REFIN names were settled
5bc44e15 docs: update the datasheet check of ina3221, pcf8563 and opt3001
177361ac wheel(drivers/opt3001): name the addresses after the ADDR pin
1f4c0d3b wheel(drivers/pcf8563): hold STOP across the time writes
32647688 wheel(drivers/ina3221): shift inject counts as unsigned
36776f60 docs: record the datasheet check of 10 drivers
255341f8 wheel(drivers/ltr390): keep the default rate in MEAS_RATE
de6e3bd7 wheel(drivers/pcf8563): select 1/60 Hz when disabling the timer
8c5d299a wheel(drivers/pcf8563): check only TEST1 and STOP in init
f046ff60 wheel(drivers/mcp4728): send write_all as one transaction
b4390844 wheel(drivers/mcp4728): call the 0x40 command Multi-Write
57f7bec6 wheel(drivers/mcp4725): decode read_status from Figure 6-3
72af1d57 wheel(drivers/mcp4725): move PD to bits 2:1 in the write frame
ec7604ff wheel(drivers/mcp3008): divide the voltage by 1024
4a16578f wheel(drivers/mcp3008): put the diff pair in D2:D1
38f29bab wheel(drivers/max31865): say what the REFIN fault bits mean
4eaf15ed docs: record the datasheet check of 8 drivers
237f29b2 wheel(drivers/si7021): wait for the sensor after a software reset
c8418fef wheel(drivers/si7021): refuse a HAL without delay_ms
ff4544af wheel(drivers/htu21d): wait out power up before the first command
a0f386ea wheel(drivers/tmp117): state the accuracy as the manual gives it
4cee3246 wheel(drivers/tmp117): ignore the revision bits in the device ID
6855d195 wheel(drivers/tmp117): report no result before the first conversion
0e5107de wheel(drivers/tmp102): saturate alert limits at the 12-bit ends
1e528856 wheel(drivers/pct2075): accept all 27 addresses in Table 5
80879634 wheel(drivers/pct2075): stop the model shifting a negative value
f39914e2 wheel(drivers/max31865): solve the full equation below 0 C
b5702a36 wheel(drivers/max31865): seed the square root near its answer
4dc8a7d9 docs: record the datasheet check of bmi160, dps310 and tcs34725
6e4413bf wheel(drivers/tcs34725): say 21 cycles for the 50 ms setting
ff70082b wheel(drivers/tcs34725): wait 2.4 ms between PON and AEN
7dea5247 wheel(drivers/tcs34725): read RGBC with the auto-increment command
e38ac920 wheel(drivers/dps310): set TMP_EXT from the coefficient source
a2bf230d wheel(drivers/dps310): check PROD_ID, not REV_ID, in the ID register
f1086b4c wheel(drivers/dps310): wait for COEF_RDY before reading coefficients
726fbd60 wheel(drivers/bmi160): use the manual's sensitivity figures
8482a374 wheel(drivers/bmi160): require delay_ms so no CMD write is dropped
e0d17680 docs: record the datasheet check of 12 drivers
c8b6ed9e wheel(drivers/ft6x36): read the chip id from 0xa3
e952d8bd wheel(drivers/tca9548a): correct what the dummy register comments say
35466c63 wheel(drivers/gc9a01): correct the madctl mh bit
5611e945 wheel(drivers/gc9a01): wait 120 ms after sleep in
4058467e wheel(drivers/is31fl3731): call it software shutdown in the header
d666f18d wheel(drivers/is31fl3731): initialize all eight frames
4b35cc5a wheel(drivers/mpr121): report over current and mask the bitmap
6de4538f wheel(drivers/mcp23017): clear a pending interrupt in init
ca6c7a07 wheel(drivers/as5600): refuse a max angle of 18 degrees or less
47b6d448 wheel(drivers/as5600): keep the blank bits of zpos and mang
25df13e8 wheel(drivers/aw9523): correct the por state of config and int
be0b4f44 wheel(drivers/aw9523): set p0 to push-pull in init
9acef461 wheel(drivers/aw9523): reset through 7fh at the start of init
3e38f79a wheel(drivers/aw9523): accept addresses 0x5a and 0x5b
25f928fa docs: record the datasheet check of 7 drivers
bf2e320c wheel(drivers/apds9960): correct the gesture config comments
2c4dbb59 wheel(drivers/apds9960): write 8 us proximity pulses
81af32aa wheel(drivers/ms5611): require the delay callback
94e01a4f wheel(drivers/ms5611): read the PROM CRC from DB3-DB0
6b3dff12 wheel(drivers/mlx90614): refuse set_emissivity without a delay
5ebcc00f wheel(drivers/mlx90614): send a real PEC on EEPROM writes
692d3fa4 wheel(drivers/shtc3): require the delay callback
1d04fea8 wheel(drivers/shtc3): divide the readings by 2^16
6f144927 wheel(drivers/sgp30): require the delay callback
b27f838b wheel(drivers/scd40): refuse altitudes above 3000 m
a5e2a06f wheel(drivers/scd40): wait out stop_periodic and set_altitude
c79e9eee wheel(drivers/scd40): send reinit from perform_reset
2f23b1cd wheel(drivers/scd40): require the delay callback
abe6ce99 wheel(drivers/scd40): test the low 11 bits for data ready
755f6b90 docs: record the datasheet check of six more drivers
a399c20c wheel(drivers/lis3dh): use low power mode at the low power rates
e6da53cb wheel(drivers/lis3dh): select high resolution for 12-bit output
4b2c493f wheel(drivers/lm75): drop the undefined low bits of the temperature
76b7531f docs: update the review figures in the README
dfb3bbc8 docs: record the second datasheet batch
2d269d1e example(wireless_mesh): report FIFO_STATUS in the nrf24 mock
c93a3579 wheel(drivers/nrf24l01): correct the default rate comment
85cd81ba wheel(drivers/nrf24l01): set the width in set_rx_addr
2505e4a3 wheel(drivers/nrf24l01): wait the full power-on reset time
49b0aaa8 wheel(drivers/nrf24l01): raise the retransmit delay at 250 kbps
7c5d1fb2 wheel(drivers/nrf24l01): leave RX before writing registers
e7a08082 wheel(drivers/nrf24l01): set the payload width on every pipe
dbbfeef4 wheel(drivers/nrf24l01): use FIFO_STATUS to find queued payloads
aaac096f wheel(drivers/nrf24l01): model the FIFOs and addresses per the manual
31159a4d wheel(drivers/lsm6dso): keep IF_INC set when enabling BDU
696df00d wheel(drivers/lps22hb): cite the right datasheet number
12879cdb wheel(drivers/lps22hb): read the outputs the way BDU requires
a0017a8e wheel(drivers/hmc5883l): say Gauss in the file comment
2397555a wheel(drivers/hmc5883l): skip the sample taken at the old gain
c4c2108f wheel(drivers/hmc5883l): report the -4096 overflow marker
ad010864 docs: record the datasheet check of six more drivers
dfb05b49 wheel(drivers/sx1262): say which call wakes the chip from sleep
d9bb31a9 wheel(drivers/sx1262): use 12 preamble symbols at SF5 and SF6
dfadd8c6 wheel(drivers/sx1262): count the 12 fixed symbols in time on air
0f2a1373 wheel(drivers/ina226): keep the calibration inside its 15 bits
ffc04948 wheel(drivers/ina226): accept both documented die IDs
1c03964b wheel(drivers/sx1262): wake the chip from sleep in standby
9ee575e0 wheel(drivers/adxl345): refuse rates above 800 Hz
ccda7be2 docs: record the datasheet check of lora_sx1276, tm1637 and ws2812
fd53b097 wheel(drivers/ds3231): refuse a day the month does not have
2d5b5962 wheel(drivers/ina219): keep the calibration value even
75b81c5e tool(test_examples): run the examples, not just build them
a83e0c88 example(wireless_mesh): report CrcOnPayload in the mock
e4f043e1 wheel(drivers/lora_sx1276): check setup writes land in LoRa mode
5804f2a9 wheel(drivers/lora_sx1276): refuse frequencies outside the bands
a37c7fa7 wheel(drivers/lora_sx1276): add a negative SNR to the RSSI
1dc73102 wheel(drivers/lora_sx1276): use the HF RSSI offset from 862 MHz
cc91d5ac wheel(drivers/lora_sx1276): stop polling at RxTimeout
31c67ab3 wheel(drivers/lora_sx1276): wait for the time on air
00d5a895 wheel(drivers/lora_sx1276): raise the current limit for high power
b73a453a wheel(drivers/lora_sx1276): refuse SF6
6e834f88 wheel(drivers/lora_sx1276): follow the symbol time with LDRO
bba8f2df wheel(drivers/lora_sx1276): refuse a packet that has no CRC
cc832889 wheel(drivers/lora_sx1276): turn the payload CRC on
d2608dd3 wheel(drivers/lora_sx1276): calibrate at the frequency in use
1f11b296 wheel(drivers/lora_sx1276): enter LoRa mode from Sleep
bf0bcd69 docs: record the datasheet check of 14 drivers
7557d6d3 wheel(drivers/mcp2515): use the CANCTRL reset value in the models
04098c07 wheel(drivers/mcp2515): drop read-backs that fail on a healthy chip
45343f1a wheel(drivers/mcp2515): read RTR from the right bit per frame type
951f0610 wheel(drivers/mcp2515): refuse 1 Mbps on an 8 MHz oscillator
a54ab224 wheel(drivers/bmi270): wait for the next sample after a range change
6def8a1a wheel(drivers/cc1101): read MARCSTATE until two reads agree
00ff830a wheel(drivers/bmm150): require delay_ms in init
e8949f1c wheel(drivers/sgp40): use the failure pattern in the model
c8cf92b2 wheel(drivers/max7219): ignore D12-D15 in the model
8358c676 wheel(drivers/cst816s): use the register sheet for the motion mask
237b601f wheel(drivers/max17048): clear the reset indicator in init
19b9e38e wheel(drivers/max17048): clear the status flags with the alert
3ecb1e11 wheel(drivers/max17048): set EnSleep before asking for sleep
854642e3 wheel(drivers/drv2605): select the library with the actuator
ecc96ce7 wheel(drivers/drv2605): enter RTP mode for real-time values
116fbfd4 wheel(drivers/rc522): follow Set1 in the ComIrqReg models
190585ec wheel(drivers/rc522): compare the self-test with its signature
d2791950 wheel(drivers/rc522): restore the setup after the self-test
f8020e49 wheel(drivers/rc522): switch the RF field on in init
6d8c8112 wheel(drivers/sd_spi): report a data error token on read
3c8be980 wheel(drivers/sd_spi): treat only an illegal CMD8 as a legacy card
a71e25bb wheel(drivers/sd_spi): check the card status after a write
8d8267c4 wheel(drivers/sd_spi): limit card waits by time, not by bytes
ac86b216 wheel(drivers/lcd_st7789): keep MADCTL and COLMOD across SWRESET
e843c401 wheel(drivers/lcd_st7789): refuse a panel past the frame memory
df2b9b30 wheel(drivers/sh1106): clear display RAM before switching on
36b9e983 wheel(drivers/hd44780): refuse rows that overrun DDRAM
ed40ab68 wheel(drivers/hd44780): switch the display on after clearing it
bf8a436c wheel(drivers/ccs811): refuse a read in the raw data mode
1fc5565a wheel(drivers/veml7700): correct the base resolution
73485b24 wheel(drivers/sht40): correct the raw value in the 50 percent test
94808f62 wheel(drivers/sgp40): judge the self-test by its first byte
6754d6d6 docs: update the fuzz target and test counts
16f3ae46 docs: record the fixes from the stateful fuzz targets
7bb38fab wheel(modules/mqtt_client): correct the SUBACK comment
377d9ff4 tool(run_libfuzzer): register the four new targets
86a94d78 wheel: add fuzz targets for four stateful components
95a61814 wheel(modules/mqtt_client): close the session on a bad PUBLISH
2fc2ffa9 wheel(protocols/mqtt5_packet): refuse a PUBLISH with both QoS bits
48e773cd wheel(protocols/mqtt_packet): refuse a PUBLISH with both QoS bits
db45b2bb wheel(drivers/air780eg): drop MQTT messages with an oversized topic
3e5dbdb8 wheel(drivers/air780eg): ignore a CSQ value outside the AT range
c9f965e7 wheel(modules/ota_manager): fix progress on images over 42 MB
97f7bd95 wheel(modules/ota_manager): refuse an empty image
4f49defe wheel(modules/ota_manager): refuse a chunk larger than requested
6adf14af wheel(modules/ota_manager): keep the last chunk inside the image
3345d3d7 wheel(modules/mqtt_client): give each callback its own context
28282657 wheel(modules/mqtt_client): stop dispatching once disconnected
e1649a49 wheel(modules/mqtt_client): close the session on a broken length
49020cdc wheel(modules/mqtt_client): skip oversized packets whole
16e2723a wheel(modules/mqtt_client): keep bytes past the receive buffer
f0b1c94a ci(nightly): run the property tests
e39230d9 docs: correct the test counts and the fuzz target wording
cf403854 docs: record the fixes made after the encoder batch
cfcad581 tool(new_wheel): keep the read flag in the SPI model template
b3958406 wheel: label the cc1101 and sx1262 hw_model stubs as unused
1f5031c0 docs: record that new wheel building is paused
4d08b064 docs: update test status and the tool list
7762d19c wheel: remove 26 fuzz programs replaced by libFuzzer
f670d0a3 tool(audit_deep): fuzz with libFuzzer instead of the old runner
b1b6d1f1 wheel(protocols/dhcp_packet): pad messages to the BOOTP minimum
c41b40b5 wheel(protocols/bencode): refuse nesting past the depth limit
8374a6b1 wheel(protocols/bencode): reject an end with nothing open
d705f146 wheel(modules/mqtt_client): refuse subscribe QoS above 1
a46ad608 wheel(protocols/sntp_client): keep milliseconds across conversions
326e9268 wheel(protocols/intel_hex): reject undefined types and trailing text
160605c3 wheel(protocols/tftp): refuse filenames too long to carry whole
f897cd8e docs: record the encoder fuzz findings
d793ceb8 wheel: cover encoders in 11 fuzz targets
cae06132 tool(fuzz_input): add a string reader
f92296e2 tool: run property tests in make check and the wheel gate
88986614 wheel(drivers/pac1952): read VSENSE as unsigned in property test
9bdca6f1 wheel: refuse publish with a length but no payload
9ea3eac8 wheel(protocols/msgpack): skip the copy for empty strings
1791286b wheel(protocols/coap_packet): refuse a length without its bytes
463bf615 wheel(protocols/websocket_frame): refuse illegal control frames
7a5037c3 wheel(protocols/mqtt_packet): refuse subscribe qos above 2
ced1ae9e wheel(protocols/mqtt_packet): refuse lengths past the protocol limit
0be2707c wheel(protocols/mqtt5_packet): fix connect remaining length
9ba47ce8 docs: correct the driver verification claims
482160a4 docs: record the datasheet check of ten drivers
935e8769 docs: record the datasheet check in driver readmes
e5e1dae9 wheel(drivers/vl53l1x): stop claiming that ranging works
77fee88b wheel(drivers/bq27441): follow the gauge's access rules
48834f27 wheel(drivers/bmi270): enable the gyro and fix init steps
48f57794 wheel(drivers/bmm150): fix soft reset and report overflow
9e68232a wheel(drivers/pac1952): fix product ID and VSENSE format
1c37b774 wheel(drivers/bno055): refuse Euler reads outside fusion modes
576a2d37 wheel(drivers/bma400): keep ODR when setting the range
ee97d751 wheel(drivers/cc1101): calibrate the synthesizer and check CRC
5e6e06e2 wheel(drivers/bme680): fix humidity and heater formulas
c221fb9f wheel(drivers/sx1262): fix packet length, timeouts and PA setup
fdbc3589 example(air_quality): report a stable heater in the mock sensor
8e9e7667 docs: refresh session state
a1c8d5e2 docs: update libFuzzer target counts
496569bb ci(fuzz-protocols): keep the corpus between nightly runs
2a624a34 docs: record findings from the fuzz target expansion
0c5ee814 tool(run_libfuzzer): register the 28 new fuzz targets
645dcedf wheel: add fuzz harnesses for parsing primitives
c5480cd2 wheel: add fuzz harnesses for firmware and diagnostic protocols
f7caf538 wheel: add fuzz harnesses for link protocols
51d046f1 docs(primitives/heatshrink): fix polling loop in usage example
bdba5c39 wheel(primitives/heatshrink): restart the window once it is encoded
89729aad wheel(primitives/heatshrink): resume tokens across a full output
e59485d8 wheel(primitives/slip): check capacity while encoding long input
efa12879 wheel(primitives/cobs): fix uint16 wrap in size bound and encoder
500d2329 wheel(primitives/base64): fix uint16 wrap in encode size check
d1adedca wheel(protocols/isotp): stop reporting flow control as a message
f451c97a wheel(protocols/ota_ab): validate fw_size before reading the slot
8290cdc0 wheel(protocols/json_rpc): fix int32 overflow when parsing ids
1d477bcb wheel(protocols/s_record): reject counts too small for the address
e868455d wheel(protocols/crsf): reject payload_len past the payload array
97db96ad docs: reword flybox origin notes
26c13fbd wheel: reach parser paths the fuzz harnesses missed
2e7a8244 tool(fuzz_input): add byte reader for structured fuzz harnesses
16c05634 tool: fix grep count fallback in shell scripts
33d81849 tool(run_libfuzzer): add coverage report and crash directory
a7f0bc9a docs: reword origin notes in component readmes
1075a5ce docs: fix libFuzzer target breakdown wording
f12b9cff docs: correct libFuzzer target count
dcbfcda9 tool(test): union per-target gcov reports instead of gcov-tool merge
dc698005 docs: add zh-CN README, author section and nightly findings
78b7f388 tool(test): link crc_any into xmodem and ymodem fuzz targets
49c644aa wheel(protocols/syslog_packet): fix shift promotion in fuzz harness
01647101 wheel(protocols/nmea_parser): reject non-finite coordinate fields
b2d07d64 wheel(protocols/modbus_tcp): fix u16 wrap in frame length check
060d23f8 tool: drop project-only targets and ignores from the public mirror
36bc5890 docs: rewrite README around verification status for the public mirror
0d79e028 ci: move workflows to hosted runners and split nightly jobs
34c63d2c tool: centralize vendor excludes and clean stale coverage builds
b5a39cad tool: harden EOL check for Windows and vendor paths
483e0047 docs: update ARM binary sizes to 259/259 wheels
35db4b5d tool: normalize line endings and exclude vendor paths
a20d458b tool: fix ARM audit cross-wheel dependency resolution
b838bcdb docs(primitives/bcd): fix README example syntax for check_readme_examples
7b25375f docs: refresh arm-binary-sizes to 257 wheels, add examples index
c62ee3f1 wheel: fix -Werror warnings across 13 wheels
6d5c8d8d docs: restructure ai-workflow task ordering and fix inconsistencies
34c81b5f docs: add task types 13 (market deliverables) and 14 (datasheet verification)
6b07dab7 wheel(primitives/bitmap): fix off-by-one in find_first_set
f0ce9824 docs: fix 216 README audit warnings across 121 wheels
b73728f2 tool: fix parallel test race and add SPDX/README/target-count guards
df6915b0 docs: fill 48 stub wheel READMEs with real content
1deb5a0b wheel: add SPDX-License-Identifier to 882 test files
2ccab935 wheel: deduplicate CRC-16 in xmodem and ymodem via crc_any
87d50763 wheel(primitives/kvs): guard fal_flash_device_find NULL return
676798c9 example(motion_control_adv): fix -Wconversion in PCA9685 register read
bc97d23b tool: add catalog/link/header/eol drift guards and catalog sync
95b4c4cb tool: renormalize all files to LF per .gitattributes
c64e3b68 docs: fix 28 documentation drift issues from recursive audit
1641d536 docs: fix layer README wheel counts to match reality
fdb2c859 tool: add -Wconversion gate and layer README count check
baf196d5 docs(drivers): update README from 52 to 116 drivers
eac687c7 wheel: fix -Wconversion across 26 more wheels and test helpers
041ded89 docs: add flagship examples and MCU matrix link to README
8a64537c docs: add MCU compatibility matrix for 116 L1 drivers
04ba78fc tool(harness): fix -Wconversion in hw_model.h register write
12a9add6 wheel: fix -Wconversion warnings in 7 drivers and ubx_parser
21c50e9e wheel(modules/mqtt_client): add topic_len to message callback
a9722ead tool(test): add test_examples.sh and make test-examples target
23a89e83 docs: fix quick-start integration instructions
df4c6eb0 docs: update README to 264 components and add flybox-demo showcase
d88cfd9d project(flybox): add E2E sim with mqtt_client + json_parser + retry
8ecba7f9 wheel(drivers/bme280): fix signed left-shift and sign-conversion warnings
e087d7a5 wheel: fix 4 datasheet-verified driver defects
be7bc88e tool(wheel): add UBSan step to wheel-gate
e7bb235f wheel: fix OSR define name in qmc5883p and endian comment in qmi8658a
b21de8bf tool: add UBSan to make check gate
774ab3de wheel: remove hardcoded range ENSURE from 9 sensor drivers
e851475b wheel: fix signed left-shift UB in 5 test mocks
edc8dbd2 wheel(primitives/hmac_sha256): guard memcpy against NULL key with len 0
093444f0 wheel: fix signed integer overflow in bme680 and bme688 humidity compensation
55390f44 wheel(primitives/varint): fix signed overflow UB in zigzag_encode
524b998e wheel(drivers/dps310): fix product ID check and remove range ENSURE
fa5e4b35 wheel(drivers/lis3mdl): fix CTRL_REG2 and CTRL_REG3 register addresses
d6ebee8f tool(codegen): force LF line endings in generated output
65a72f7b wheel(primitives/log): split null-fn from no-slots in log_err_t
3fe5dc31 wheel(primitives/fal): wrap HAL returns with FAL_ERR_HAL
df6be630 wheel(protocols/modbus_rtu): split err_t into CRC, overflow, and param codes
621e2bb7 tool(bootstrap): warn when session jsonl exceeds 200 MB
83adb2d0 tool(test): add regen_and_verify one-shot script
60cd63c2 tool(audit): add per-check timing benchmark
9721a0a9 tool(test): emit FAIL roster at end of test runner output
ea3c51cd tool(audit): add scan_boundary_skips standalone scanner
ae9ed053 docs: update audit check count 62 to 63
287d391f tool(codegen): note blind-spot scan result in docstring
3d0740fe tool(audit): add boundary codegen blind-spot lint
5ff91dad wheel: regenerate boundary tests with extended struct detection
2be44709 docs(drivers/st7735): drop static inline from API block
6ced8eec tool(codegen): extend boundary callback-struct detection
f4907195 example(smart_lock): add multi-factor door lock integration demo
ce62117a example(can_gateway): add CAN bus message gateway integration demo
0370a29d example(lowpower_sensor_node): add battery duty-cycle integration demo
f6240d19 wheel(protocols/modbus_rtu): add modbus_rtu_err_t and guard parse oversize len
75e48680 wheel(primitives/log): add log_err_t enum for sink api codes
16c2d161 docs: update audit check count 55 to 62 across CLAUDE+playbook+workflow
165c9352 tool(audit): add ctx_t naming and err_t defined checks
e05c33eb tool(harness): add missing test_boundary cross-wheel include paths
d725c932 wheel: regenerate boundary tests with relaxed ret<=0 asserts
2b9e2c57 example(mqtt_telemetry_demo): add mqtt_client state machine demo
2574380a example(device_info_demo): add identity banner integration demo
e8a982d8 example(command_queue_demo): add ASCII command dispatcher
cea4b7a4 example(secure_ota): adapt to aes128_t and sha256_t renames
a8c8b5c1 example(crypto_pipeline): adapt to aes128_t and sha256_t renames
494680a8 wheel(primitives/hmac_sha256): adapt to sha256_t rename
7969980f wheel(primitives/sha256): rename sha256_ctx_t to sha256_t
8f2ffc41 wheel(primitives/aes128): rename aes128_ctx_t to aes128_t
c4f870f1 example(gps_tracker): adapt to ubx_parser_ctx_t to _t rename
d3519138 wheel(protocols/ubx_parser): rename ubx_parser_ctx_t to ubx_parser_t
6f0a53e8 example(timer_retry): adapt to retry_ctx_t to retry_t rename
4c46aebf wheel(primitives/retry): rename retry_ctx_t to retry_t
48915d75 wheel(protocols/dmx512): rename dmx512_ctx_t to dmx512_t
b9a39c00 wheel(primitives/fal): introduce fal_err_t and switch to named codes
dcaccc67 wheel(primitives/log_flash): migrate to log_flash_err_t return
334428bb wheel(primitives/kalman_1d): migrate to kalman_1d_err_t return
12ff4f29 wheel(primitives/iir_lowpass): migrate to iir_lowpass_err_t return
5f3719f4 wheel(primitives/median_filter): migrate to median_filter_err_t return
defd7b18 wheel(primitives/heatshrink): add round-trip property tests
3d14fb6a wheel(primitives/bitstream): add round-trip property tests
a8d411c7 wheel(primitives/varint): add round-trip property tests
47bc5fbe wheel(primitives/utf8): add round-trip property tests
c10c8766 wheel(primitives/rle_codec): add round-trip property tests
5a644978 wheel(primitives/bcd): add round-trip property tests
ea163a2d tool(codegen): relax boundary test asserts and skip max len values
1e707cab wheel: regenerate boundary tests across 250 wheels
da036671 tool: tooling audit cleanup — set -e, .gitignore, and 6 argparse migrations
189e6fee wheel: migrate B-class inline delay counters to mock_time.h in 5 drivers
e092a51e wheel: migrate inline mock_delay stubs to mock_time.h in 10 drivers
07d91589 tool(harness): add fixtures, skip/fail, SPI matchers, and mock_alloc
2698a81c tool(harness): add 6 assert macros, fail injection, mock_time, and fix mock_gpio
06c5876d tool: add driver prompt generator, null-test placeholder, wheel-docs batch count
8e5b6b38 docs: update wheel count 251 to 259
47f2ae10 wheel(drivers/mpr121): add NXP MPR121 12-channel capacitive touch
8c3dcb75 wheel(drivers/lis3mdl): add ST LIS3MDL 3-axis magnetometer
afd93b45 wheel(drivers/bma400): add Bosch BMA400 ultra-low-power accelerometer
ada1adda wheel(drivers/ht16k33): add HT16K33 LED matrix and 7-segment driver
902741b2 wheel(protocols/j1939): add SAE J1939 CAN identifier and PGN codec
bdf12426 wheel(protocols/intel_hex): add Intel HEX firmware format parser
ffa3066c wheel(primitives/utf8): add UTF-8 validation, decode, encode, and count
d6d4bac4 wheel(primitives/scale_map): add linear value mapping with clamping
53ab07d7 docs: update wheel count 243 to 251
9a44e088 wheel(drivers/bmi160): add Bosch BMI160 6-axis IMU driver
a9501e00 wheel(drivers/htu21d): add TE HTU21D humidity and temperature sensor
07311b6e wheel(drivers/pct2075): add NXP PCT2075 digital temperature sensor
16d30d43 wheel(drivers/mcp4728): add Microchip MCP4728 4-channel 12-bit DAC
bd3d596f wheel(protocols/uds): add UDS (ISO 14229) diagnostic message codec
92c10fb9 wheel(protocols/s_record): add Motorola S-Record firmware format parser
fa7957b0 wheel(primitives/varint): add LEB128 variable-length integer codec
b1edb748 wheel(primitives/moving_rms): add running RMS over sliding window
c5124294 tool: add agent prompt template, wheel-docs target, fix inventory script
00c59fa4 Revert "docs: update wheel count 243 to 250"
9bca7a62 docs: update wheel count 243 to 250
646aeaf3 docs: update wheel count 235 to 250
0bd3dbb2 docs: update wheel count 235 to 243 with batch 6 additions
cdd9a34c wheel(drivers/ina3221): add TI INA3221 triple-channel power monitor
3f9bf1f3 wheel(drivers/dps310): add Infineon DPS310 barometric pressure sensor
8b999abb wheel(drivers/vl53l1x): add ST VL53L1X time-of-flight distance sensor
334af5f4 wheel(drivers/ltr390): add Lite-On LTR-390UV UV and ambient light sensor
acc96ddf wheel(protocols/modbus_ascii): add Modbus ASCII frame encode/decode
7eea6923 wheel(protocols/isotp): add ISO-TP (ISO 15765-2) CAN multi-frame transport
f4334a8b wheel(primitives/signal_stats): add running mean/variance/min/max tracker
6ff67bd9 wheel(primitives/bitstream): add bit-level read/write over byte buffer
455228bf tool(wheel): fix gate audit regex and add null test auto-generation
1854cc81 wheel: fix 20 audit warnings across batch 5 wheels
bcfa9776 docs: update wheel count 227 to 235 with batch 5 additions
91640531 wheel(protocols/crsf): add TBS Crossfire/ELRS CRSF protocol codec
b43be99d wheel(protocols/ibus): add FlySky iBUS RC protocol codec
d41a0101 wheel(drivers/bmm150): add Bosch BMM150 3-axis magnetometer
366828d8 wheel(drivers/lis2dh12): add ST LIS2DH12 3-axis accelerometer
56797bab wheel(drivers/max6675): add K-type thermocouple SPI driver
8b77c699 wheel(drivers/tmp117): add TI TMP117 high-accuracy temperature sensor
5d6b0151 wheel(primitives/peak_detector): add signal peak/valley tracker with decay
70ee1c3f wheel: scaffold 8 wheels for batch build
aef40a99 wheel(primitives/bcd): add BCD encode/decode for RTC and legacy protocols
cba5b24a tool(wheel): add wheel-gate commit workflow with test+audit enforcement
f9b28801 docs: update tooling descriptions with 3 new audit checks and gen_boundary_tests
52c3c54d wheel: add polling delay in ads1015 and ads1115 DRDY loops
8f52c908 tool(wheel): generate hw_model skeleton for all 5 HAL types at scaffold time
1f37f2bf tool(codegen): rewrite gen_boundary_tests.py with float, enum, and single-wheel support
40452e7e tool(audit): add 3 checks for polling delay, spec vectors, test tautology
ddd51f05 wheel: fix 4 moderate audit findings and add 12 edge-case tests
c876d0fb wheel: fix protocol compliance in lin_bus and hdlc
8b7f4e35 wheel(drivers/lsm6dso): fix 2 critical bugs in gyro register encoding and unit scaling
fff4e370 docs: update wheel count 219 to 227 with batch 4 additions
34e2225a wheel: add hdlc and lin_bus L2 protocols
cb5aeff4 wheel: add bmp388, shtc3, lsm6dso, is31fl3731 L1 drivers
db56edce wheel: add ewma and hysteresis L0 primitives
dd7beb6b docs: update wheel count 211 to 219 and audit check count 52 to 54
67140725 wheel: fix 2 HAL guard gaps and remove 7 stale hw_model orphans
a412dfb7 tool(audit): add hw_model orphan and HAL guard checks, widen float window
f7b0392c wheel(drivers/lm75): activate CONTRACT_ENABLED and add 2 hw_model tests
2b6417d5 wheel(primitives): fix 3 bugs and add 7 tests across datetime and interpolate
006b709c wheel: fix 3 audit findings in ir_nec and onewire_search
f7eaed6d wheel: fix audit findings in hcsr04 and mcp3008
57e64ced wheel: add onewire_search and ir_nec L2 protocols
9121d561 wheel: add hcsr04, mcp3008, lm75, st7735 L1 drivers
c7e87fd1 wheel: add datetime and interpolate L0 primitives
7b6d9741 docs: add 5 new wheels to CLAUDE.md inventory lists
a2139f06 example: add 10 integration examples consuming 72 wheels
73dd5165 tool(commit-msg): add example to allowed commit types
f77fe4ae wheel: add scd40, bme688, cst816s L1 drivers
21baab76 wheel: add heatshrink and framebuffer L0 primitives
f7fca57a tool(audit): scan examples/ directory for wheel consumers
737bffb2 docs: add example commit type for examples/ directory
62706502 ci: trigger fresh CI run after runner session cleanup
1170bb74 ci: optimize for self-hosted runner
2895859f ci: switch to self-hosted runner in WSL
f531a12c docs: add 3 runnable examples exercising 10 unconsumed wheels
84095e12 tool: add ci-local gate and upgrade pre-push hook
5c7420e7 tool: fix ARM audit dependency resolver and modbus_rtu -Wconversion
c25af1f5 docs: add CMakeLists.txt for 5 hardware reference examples
cd3c0054 ci: fix runner failures and example build-only handling
52447e86 docs: add 5 integration examples for open source
34ebccb0 docs: add wheels catalog index organized by domain
d3dcbfa9 docs: rewrite README for open source community
b7558883 wheel: fix 20 audit warnings across new wheels
43fe2c0f docs: update wheel inventory 186 → 206
c52a788c wheel(drivers): add sx1262, cc1101, tb6612, a4988, sk6812 implementations
d6fa4dbf wheel(drivers): add adxl345, ms5611, ccs811, bno055, sgp40 implementations
242388cd wheel(drivers): add sh1106, hd44780, ina226, apa102 implementations
75722f10 wheel(protocols): add ubx_parser, json_rpc, ppp_frame, dmx512 implementations
57cc871b wheel(primitives): add double_buffer and rle_codec implementations
a910816e wheel(drivers/sgp40): add SGP40 VOC raw signal sensor driver
861ca506 wheel(drivers/bno055): add BNO055 9-axis IMU with sensor fusion driver
8491d21a wheel(drivers/ccs811): add CCS811 eCO2/TVOC gas sensor driver
2e9dde40 wheel(drivers/ms5611): add MS5611 precision barometric pressure sensor driver
ae333007 wheel(drivers/adxl345): add ADXL345 3-axis accelerometer driver
c16b2fe0 wheel(drivers/sk6812): add SK6812 RGBW addressable LED driver
d141ca0c wheel(drivers/a4988): add A4988 stepper motor driver
a5339258 wheel(drivers/tb6612): add TB6612FNG dual H-bridge motor driver
e3199b67 wheel(drivers/cc1101): add CC1101 Sub-GHz transceiver driver
7760e8f2 wheel(drivers/sx1262): add SX1262 LoRa transceiver driver
7455e27e wheel: scaffold 20 new wheels for ecosystem batch build
aaa82ad8 wheel: scaffold 11 wheels for batch build
6cfcce3f tool: add check-readme-examples to make check gate
40dfafa6 docs: fix 21 wheel README compile errors found by Q6 audit
f673bcd5 wheel(primitives/hmac_sha256): fix 2 moderate security audit findings
62ffc9d1 wheel(primitives/sha256): replace memset wipe with volatile loop in final
64dace89 wheel(primitives/aes128): fix 2 security audit findings
6c662cfd docs: remove corrupted-filename placeholder from index
f475298a tool(audit): add check_clones.py for Q26 cross-wheel duplication
f1ea9de9 tool(audit): add check_complexity.py and check_readme_examples.sh
277dab69 tool(audit): add check_readme_examples.sh for Q6 README compilability
547d59e4 ci: add SPDX header check and nightly libfuzzer jobs
57f79714 docs: replace proprietary LICENSE with MIT
0f0ea0f5 docs: add CONTRIBUTING.md with quick-start and wheel guide
80cd6456 ci: add examples and ARM cross-compile smoke jobs on push
cedb9dc2 docs: fix audit check count 52 → 55 (CHECK_TABLE 52 + 3 special)
a347cb36 docs: fix remaining stale counts and directory descriptions
9379fd8e docs: sync stale counts across 4 documentation files
420f6568 docs: update README.md platform stats to current reality
0c837cbf docs: fix 8 stale path/count references across documentation
d3049413 tool: fix 4 stale tooling/test path references from restructure
bfc8dc3b wheel: fix examples include path tooling/test → tooling/harness
704316d5 docs: update CLAUDE.md wheel count 178 → 186
cb234444 tool: update fuzz runners and regenerate sentinel path comments
e1bd4993 wheel: add prop/boundary/fuzz tests for 8 new wheels
59b06ad9 wheel: fix 1 CRITICAL + 3 MODERATE audit findings across new wheels
4b35eebc wheel: fix 4 audit/coverage issues across new wheels
45a384ec wheel(drivers/bmi270): implement Bosch BMI270 SPI 6-axis IMU
ec01cecf wheel(drivers/ili9341): implement ILI9341 SPI TFT display driver
4cae5617 wheel(drivers/rc522): implement MFRC522 SPI RFID reader
0d83ea51 wheel(drivers/pcf8563): implement NXP PCF8563 I2C RTC driver
fd750d69 wheel(protocols/tftp): implement TFTP protocol builder/parser
1793e4d4 wheel(protocols/dhcp_packet): implement DHCP message builder/parser
d17ca136 wheel(primitives/font_5x7): implement 5x7 bitmap font
761c8ca5 wheel(primitives/memory_pool): implement fixed-size block allocator
00f4a627 wheel(primitives/font_5x7): scaffold font_5x7
99be4d31 wheel(protocols/tftp): scaffold tftp
f29d74ed wheel(protocols/dhcp_packet): scaffold dhcp_packet
1b37efff wheel(drivers/pcf8563): scaffold pcf8563
e155dc94 wheel(drivers/bmi270): scaffold bmi270
8b619fcb wheel(drivers/ili9341): scaffold ili9341
3d22b87d wheel(drivers/rc522): scaffold rc522
7985bba9 wheel(primitives/memory_pool): scaffold memory_pool
5428efcb wheel: add contract.h assertions to 12 remaining wheels
402779f3 wheel: add 12 stress tests for critical L0 and L2 wheels
de6975c3 wheel: add contract.h assertions to 8 L0 primitives
fd4b6098 wheel: add 10 LibFuzzer harnesses for L2 protocols
cd3e0a49 wheel: add fuzz harnesses for mavlink and sbus protocols
12ae7bf2 wheel: add 27 property tests and fix fal mock conflict
9ebefab3 wheel: add 3 missing hw_model files for L1 drivers
166ee22b wheel: fix 6 audit warnings across log ecosystem
241368e4 tool: remove corrupted mojibake file from tracking
d4d01e62 tool: restructure tooling/ directory for stability-based isolation
3264afa2 docs: update session state and add audit snapshots
b92af6ac wheel: fix null test build failures across 7 wheels, register 160 null targets
89e70c1b tool(test): fix double-pointer codegen and scaffold stale-dir crash
4b0cfcc1 tool: fix 7 audit findings across debug/hil/metrics/hooks
5d7ea56b tool: fix 3 MODERATE/MINOR findings across dispatch/batch_fix/commit_suggest
83842d0c tool(audit): fix 6 correctness and robustness issues in audit tooling
5127bbe8 tool(test): fix 5 findings in test infrastructure deep audit
c8e2075b tool(test): strip __attribute__ in read_header so FUNC_RE matches decorated decls
8ea14e0e tool: fix invalid --hal accepted silently and usage() leaking set -euo pipefail
344bf67c tool(test): fix sentinel generator for null-terminated string output
d00186d4 tool: fix 7 MODERATE/MINOR findings across fix/patches/debug/metrics/hil
903cb871 tool(session): fix dispatch cleanup ordering and sync_vsf pipe masking
dcf19551 tool(test): fix 5 CRITICAL/MODERATE and 5 MINOR findings in test infrastructure
c07870cd tool(new_wheel): fix I2C/SPI NULL function pointer checks and hw_model write logic
e2142137 tool(audit): fix 6 bugs found in re-audit of tooling/audit/
147e9760 tool: add test-scaffold to make check pipeline
6289ba46 tool(new_wheel): fix scaffold compile errors in none template
adda74af tool: fix 3 bugs found in round 5 sweep
828b132a tool: fix 2 stale comments after refactoring
301309b4 tool(test): fix --parallel default serialization and gc cleanup crash
1dd1244d tool(dispatch_agent): fix 2 bugs in --merge-all logic
4161b52c docs: fix documentation drift across 3 tooling READMEs
38e02679 tool: fix 6 remaining findings in wheel/hil/debug/metrics
bfea2227 tool: fix 6 remaining findings in ci/session/fix scripts
65261475 tool(test): fix 3 remaining findings in test generators
4342074a tool(audit): extract shared arm audit code into _arm_audit_common.sh
6c3b6b9c tool(audit): fix 8 remaining findings in audit infrastructure
127b4bb1 tool: fix 5 MODERATE in wheel/metrics/debug/Makefile
eb64249b tool: fix 5 MODERATE in ci/session/fix scripts
420a2a8f tool(test): fix 5 MODERATE findings in test infrastructure
9f748b9c tool(audit): fix 5 MODERATE findings in audit infrastructure
fb1f5a69 docs: fix Makefile help drift and experiment README phantom entry
82421c53 tool: fix 4 CRITICAL and 5 MODERATE in wheel/patches/hil/metrics/debug
648f9166 tool: fix 3 CRITICAL and 7 MODERATE in ci/session/fix scripts
2d7ee675 tool(test): fix 3 CRITICAL and 5 MODERATE test infrastructure findings
6e289f6d tool(audit): fix 5 CRITICAL and 8 MODERATE audit findings
b84a33c1 docs: update CLAUDE.md tooling description after deep audit
8d3ccde7 tool: fix 8 remaining audit findings across tooling
afab5414 tool(audit): unify find_wheels() into _audit_build_base shared module
6f971ea9 tool(audit_wheels): fix brace counting in chk_switch_default for char/string literals
972a701e tool: replace grep -P with POSIX-compatible equivalents
cd952539 tool(gen_sentinel_tests): migrate to shared _wheel_parser module
7ddbd023 tool(audit_wheels): remove 10 dead functions and fix 2 check heuristics
3c4869a2 tool: fix 4 workflow tool bugs in commit_suggest, find_all_refs, update_wheel_inventory, release
6466a2b9 tool(doc_drift_check): fix substring replacement, multi-PHONY counting, dead function
9e290543 tool(test): fix test runner bugs in test_all, coverage_wheel, run_generated_tests, test_scaffold
96275ea2 tool(test): add multi-pin support to mock_gpio.h
5f9dc7aa tool(test): add 4 assertion macros to test_harness.h
ac9ef3b7 docs: fix CLAUDE.md tooling references
24c32675 docs: fix stale counts in tooling and audit documentation
63fc3f1e tool(fix): remove 9 orphan one-time fix scripts
eb7c852f tool(test): add unit tests for _wheel_parser shared module
a5dd5fae tool(test): extract _wheel_parser.py shared module from test generators
24490799 tool: normalize color variable names in fuzz scripts
4d40c743 tool: fix Makefile help gaps and add missing standalone targets
7933fd15 tool: add cleanup traps to batch_fix.sh and test_mutation.sh
d4a05f66 docs: update wheel count to 178 and tooling descriptions
4c2bad93 tool: add batch-scaffold Makefile target for batch wheel builds
c7a0d163 Revert "wheel: scaffold 2 wheels for batch build"
20111df6 wheel: scaffold 2 wheels for batch build
21891c8e tool: add auto-registration and merge-retry for batch wheel builds
a59a25ea wheel: fix 4 minor audit findings in buzzer, mavlink, sbus
78b859ce wheel: register null-param test targets in max30102, mavlink, sbus CMakeLists
3981b986 wheel: fix NULL dereference in aht20, buzzer, and max30102 drivers
c9b850e8 wheel(protocols/mavlink): implement MAVLink v2 frame parser and serializer
08aeddab wheel(primitives/soft_spi): implement software SPI master with all 4 modes
6daac4cc wheel(drivers/buzzer): implement passive buzzer driver with tone and melody support
c57cbbb0 wheel(drivers/tm1637): implement TM1637 7-segment LED display driver
4043a1c9 wheel(drivers/max30102): implement MAX30102 pulse oximeter and heart rate sensor driver
e66758d7 wheel(drivers/w5500): implement W5500 SPI Ethernet controller driver
aa320249 wheel(protocols/sbus): implement S-Bus RC receiver protocol parser and encoder
6580044c wheel(primitives/onewire): implement 1-Wire bus protocol with ROM commands and CRC8
51a0311a wheel(drivers/gc9a01): implement GC9A01 round LCD controller driver
9da1211b wheel(drivers/aht20): implement AHT20 temperature and humidity sensor driver
98234ef5 docs: add outsource project delivery workflow
f53e5870 docs: add project test directory convention and API discovery guide
7cb55a21 tool: make check-full a proper superset of check
5add46b2 tool: add coverage gate to make check
d1dc994b ci: fix coverage-gate nightly skip bug and self-detect harness changes
8f5f91d1 wheel: harden mutation test coverage for fixed_point, bitfield, json_parser, cobs
91dd3396 tool: remove zero-consumer test frameworks
a00be48e ci: add coverage gate job for nightly and harness changes
58d788df tool(coverage): add native fs build and parallel scan
e56e5c4a docs: fix wheel counts to 168 and remove stray drivers/test directory
372ff19a tool(coverage): add check-coverage gate for coverage regression
e72fa3f0 tool(coverage): fix gcda merge bug and add batch scan
e23408b2 docs: fix wheel counts to match actual directory count (169)
33bf5779 wheel(drivers/mcp23017): add tests to raise hw_model coverage from 73% to 100%
42775cb8 wheel(drivers/ws2812): add tests to raise coverage from 70% to 100%
5f4bf8c7 wheel(drivers): add tests to raise coverage to >=90% for three wheels
83e83abe wheel(drivers/drv2605): add tests to raise coverage from 71% to 100%
0147c74d wheel(primitives/iir_lowpass): add NULL guard tests to reach 100% coverage
9d105bf7 wheel(drivers/lps22hb): add negative pressure test to reach 100% coverage
83fdfc44 docs: enhance project template for Day 1 client onboarding
2f162f6b docs: update wheel counts to 173 and mark ota_updater wishlist complete
2b8dc097 wheel(protocols/http_request): add HTTP/1.1 request builder with Range support
375b29e1 wheel(primitives/hmac_sha256): add HMAC-SHA256 message authentication (RFC 2104)
247dd521 wheel(primitives/soft_i2c): cover SOFT_I2C_ERR_BUS enum value in tests
b79c753d tool(gen_null_tests): simplify assertions to crash-only checks
a14f26db wheel(drivers/bme680): add tests to raise coverage from 33% to 99%
cf22831a wheel(drivers/enc28j60): add tests to raise coverage from 53% to 99%
9d7549b4 wheel(drivers/air780eg): add tests to raise coverage from 49% to 99%
6fd60f29 wheel(drivers/mcp2515): add tests to raise coverage from 26% to 99%
a036398c wheel(drivers/lora_sx1276): add tests to raise coverage from 19% to 100%
ee58f606 wheel(drivers/bme680): add tests to raise coverage from 33% to ≥80%
4ab803bd wheel(modules/event_bus): add tests to raise coverage from 77% to 100%
72715fab wheel(protocols/cbor_lite): add tests to raise coverage from 77% to 99%
365d774e wheel(primitives/sha256): add tests to raise coverage from 26% to 100%
fe955838 tool(arm_size_audit): fix 4 ARM compilation failures
a67a3582 docs: sync wheel counts and listings for iir_lowpass + kalman_1d
18557bf8 wheel(primitives/kalman_1d): fix audit findings — NaN guard, deviations, tests
81197909 wheel(primitives/iir_lowpass): fix audit findings — annotations, tests, cmake
1df9b74e wheel(primitives/kalman_1d): add 1D scalar Kalman filter
1d3a82e0 wheel(primitives/iir_lowpass): add first-order IIR low-pass filter
d68e3ad8 wheel(primitives/fal): fix audit findings — headers, null guards, tests
758d8293 wheel(primitives/fal): self-write replacing vendored RT-Thread FAL
e7582eed tool: fix gen_null_tests single-wheel mode + doc_drift_check regex
8974dd06 tool(scaffold): add --deps flag for cross-wheel CMakeLists injection
ad367f6c tool(scaffold): add hw_model template for I2C/SPI + test_error_codes for all templates
2bb9e620 wheel: fix 12 audit warnings across 9 new wheels
12a19c96 wheel(drivers/aw9523): add AW9523B I2C GPIO expander and LED driver
9b218f08 wheel(drivers/sgp30): add SGP30 air quality sensor driver
c763af16 wheel(drivers/lps22hb): add LPS22HB barometric pressure sensor driver
e8ba7f3f wheel(drivers/sht40): add SHT40 temperature and humidity sensor driver
359ea891 wheel(drivers/tsl2591): add TSL2591 ambient light sensor driver
fa7dda95 wheel(drivers/icm42688): add ICM-42688-P 6-axis IMU SPI driver
a4e18dc8 wheel(drivers/bq27441): add BQ27441 smart fuel gauge driver
6ab706ad wheel(drivers/ft6x36): add FT6x36 capacitive touch controller driver
ccc6575f wheel(drivers/lcd_st7789): add ST7789 TFT LCD SPI driver
e59c1118 wheel(drivers/max17048): add MAX17048 LiPo fuel gauge driver
e503cf62 wheel(protocols/sntp_client): add SNTP v4 packet codec + clock offset
8f5b65e1 wheel(modules/power_manager): add cooperative sleep/wake orchestrator
5730ff6a wheel(modules/mqtt_client): add stateful MQTT 3.1.1 client
585a84a1 tool(ci): add control-character filename guard to pre-commit hook
1c0c6a04 tool(test): add coverage_scan.sh for full-platform coverage report
31ce48b8 wheel(drivers): raise line coverage for tcs34725, ina219, pca9685, ws2812
7dd2b95b wheel(drivers): raise line coverage to 100% for mcp23017, nrf24l01, ds18b20
9c6ffa4c wheel(protocols): raise coverage — msgpack 63%→93%, coap_packet 74%→99%
38a3e522 wheel: add property-based tests for 4 new wheels
39e4ea82 docs: update wheel counts 146→150 and flybox extraction status
b38f211b wheel(drivers/air780eg): fix uint32 tick overflow causing instant timeout
8d1c2fbf tool(test): add per-wheel coverage report — make coverage-wheel W=xxx
44e57e22 wheel: add 42 missing test cases across 4 new wheels
7dd1e898 tool(scaffold): fix 6 template-audit misalignments
b1bc9520 tool(audit+scaffold): fix template-audit misalignment
cd9846ef wheel: fix audit warnings across 4 new wheels
24beea5b tool(gen_null_tests): regenerate null tests for all 150 wheels
4456c4c2 wheel: extract 4 wheels from flybox — soft_i2c, flash_ring, gnss_filter, air780eg
c3d18202 docs: remove remaining unverified percentages and stale numbers
96466e1b docs: replace hardcoded 10/90 ratio with qualitative description
124c67be docs: restructure root README with core philosophy section
0eeb0131 docs: update root README.md to reflect 146 wheels
2fed9419 docs: update wheel layer index READMEs to reflect 146 wheels
c48c5389 docs+tool: fix documentation drift across 6 files
647abdd1 tool(audit): add --wheel single-wheel audit mode
8fe7d43b tool(gen_null_tests): add --wheel single-wheel mode
91175488 tool+wheel: add audit checks for missing null tests and hw_model
2a3775b6 wheel(drivers/mcp4725): add full test suite — null, property, hw_model
595ac40c wheel(drivers/mcp4725): add MCP4725 12-bit I2C DAC driver
8a71fa32 tool+wheel(drivers): fix ACSL false positives in naming check, fix 2 more hw_model warnings
54719f2b wheel(drivers): fix -Wconversion and -Wsign-conversion in 8 hw_model tests
493708d8 tool: extract shared test runner, remove 108 lines of Makefile duplication
4db3c6e4 tool: add make frama-c target for WP formal verification
f341a40a wheel(drivers): register 23 unregistered hw_model tests in CMakeLists
fc076817 tool: fix make verify, add test-contract and test-differential targets
24c3b3c3 tool+wheel: add gen_boundary_tests.py and make test-boundary target
da9bcb89 tool: activate 1405 generated tests that were never executed
6f7bf23f tool+wheel: add Frama-C ACSL formal specs for 5 core wheels
9a8385da wheel(primitives): add aes128 + sha256 test/prop CMakeLists updates
3c7d4eac tool+wheel: libfuzzer harnesses, differential testing, runtime monitor, crypto wheels
b7fb4946 tool: fix fuzz script missing -I tooling/test for contract.h
9e7907f6 docs: update README final numbers + fix Makefile target count drift
aa072463 tool+wheel: fix 13 MINOR findings from super audit
1862a1f3 tool: add arm_stack_audit.sh for per-function stack usage analysis
4e6b59cc docs: add examples/ to CLAUDE.md directory structure
90b18326 wheel: fix -Wconversion in bmp280 ADC assembly + mqtt_packet QoS shift
fde788a3 exp: add 3 integration recipes demonstrating wheel composition
b5002c4d docs: update audit history + fix bme680 timeout test coverage
dc0bb999 wheel: fix 9 CRITICAL findings from super audit
710455a5 tool: fix 2 CBMC verify specs + soft_timer invariant
fcace2dd tool: fix audit_coverage.py gcov invocation
0616a5c4 wheel: fix 5 contract ENSURE failures + i2c_scan stale test
f349d383 wheel(drivers): add enc28j60 + sd_spi hardware models
6d24768e wheel(drivers): add lora_sx1276 + mcp2515 SPI models + CMakeLists updates
4d27702a wheel(drivers): complete hw models — 42 drivers covered
cd793cab wheel(drivers): servo test file from signal model agent
735b317f wheel(drivers): mcp2515 + remaining hw model tests
bd85cb12 wheel(drivers): add ds18b20 hw model
df18b988 wheel(drivers): add servo/stepper/ds18b20/lora signal models + tests
5d82a2ea wheel(drivers): add test files for max31865/opt3001/sht30/veml7700/xgzp6816d + ws2812 model
86cbea4d wheel(drivers): add 14 I2C sensor hw models from batch A+B agents
d3d8210c wheel(drivers): finalize nrf24l01 hw model update
29c0a811 wheel(drivers): last hw model straggler
8e02cb3a wheel(drivers): finalize hw model CMakeLists + test integration
14ed5f1e wheel(drivers): add at24cxx EEPROM hw model
caa7cad0 wheel(drivers): add hw models for ads1115, tmp102, nrf24l01 + test files
c4eaf564 wheel(drivers): add bh1750 + max7219 hw models
9ca04d21 wheel(drivers): add remaining hw model test files
fa51c042 wheel(drivers): add tca9548a + nrf24l01 hw models from agent
adb91d7a wheel(drivers): add hw models from final agent batch
bf908def wheel(drivers): add hw model for max31855 thermocouple
e98679f3 wheel(drivers): add hw models for lis3dh, mcp23017, w25qxx
9c59a0f0 wheel(primitives): add contracts to final 17 L0 wheels (141/143 total)
d480d275 wheel: finalize CBMC verify_base64 + verify_timeout from agent
0a88ebe4 tool: add apply_contracts.py helper from L1 agent
e0a0799c wheel: complete contracts (124/143) + CBMC (15/15) from parallel agents
589a6ed1 wheel: add contracts for 4 L3 modules + verify_checksum CBMC
18f4613b wheel: add 6 missing property tests + contracts for cli_shell/telemetry
01cddb95 wheel: add remaining contracts + CBMC verify for cobs
9df0546a wheel: add contracts to 42+ wheels + 5 new CBMC verifications
934a7ea3 docs: expand audit README with architecture, design rationale, trustworthiness
6caabaa6 tool: delete 23 duplicate legacy loops from audit_wheels.py main()
a35a8f92 docs: add audit system README (Chinese) + refactor audit_coverage.py
dcfc369c tool: extract _audit_build_base.py shared module for 3 audit tools
facb8231 tool: remove 6 duplicate L1 patterns + arm_cross_compile + rename mutation_test
63d24609 wheel: fix UB found by sanitizer in fixed_point and bme680
04526ed8 tool: fix audit_sanitizer.py build command (remove -- -j, use resolve())
d9d5cb6f wheel: fix 17 NULL test assertions + update gen pattern knowledge
56f6686a wheel: fix i2c_scan count overflow + 2 sentinel NUL-terminator fixes
aa2abaf5 tool: validate and fix 8 new audit tools (Phase A)
2e54edc5 wheel(drivers/bmp280): add datasheet-accurate hardware register model
99eb7c0c tool: generate 87 sentinel boundary tests across 27 L0/L2 wheels
86ced0ad tool: generate 1318 NULL parameter tests across 141 wheels
af2b3420 wheel(primitives/ring_buffer): add 10 mutation-driven tests, kill rate 48% → 87%
51b3297c wheel(drivers): add CMakeLists property test targets for 43 L1 drivers
e4c19d86 wheel: add final 19 L0 property tests + CMakeLists updates
54ad0e0f wheel: add last 8 property tests + clean compiled binaries
44e4de8c wheel: add 24 more property tests from final agent batch
570df60a wheel: add final 8 property tests from parallel agents
8896b482 wheel: add 6 more property tests + late agent fixes
f1b59f6d wheel: add property tests for 47 more wheels (total 83/143)
a4faa242 wheel: fix 15 audit warnings across L0/L1/L2/L3
85bb759c tool: fix float-to-int FP and unchecked-return (void) exclusion
40cdc760 wheel: add 2 more property tests + fix bme680 float-to-int cast
63b2442e wheel: add property-based tests for 36 wheels across all layers
322471bc wheel(primitives): add contracts to ring_buffer + CBMC proof for state_machine
ce53d42b tool: fix known_exceptions display level in audit output
f8b6f8fa docs: add 7 Makefile targets for new audit phases
c748da87 wheel(primitives): add property tests and CBMC verification exemplars
ec43d06b tool: add property testing, contract, fault injection, hw model frameworks
86c627d4 tool: add compiler analysis, sanitizer, coverage tools + known exceptions
ddd255b3 docs: update counts for 57 audit checks and 38 Makefile targets
60c68294 tool: add 4 audit checks (Q8/Q10/Q13/ADR-009) + audit-quick target
2ef23231 tool: reduce float_to_int_cast false positives with clamp detection
a5154509 wheel: fix 12 audit findings across L0/L1/L2/L3
627ebcb5 wheel(drivers): add range clamp before float-to-int cast
94a253ab wheel: fix signed left shift UB in 4 wheels (8 locations)
13d9623e tool: fix 95-100% false positive rate in 3 audit checks
eb45645e docs: sync audit check count to 53
8ac1cb4b tool: add 7 code-level checks and 2 semantic checks to audit_wheels.py
4ff5bd8b docs: sync Makefile target count to 37
b49b1261 tool: clean up audit system — delete dead code, fix cache bypass, add L6/report
c2528ba9 docs: sync audit check count to 46 after registry refactor
b077c33f tool: refactor audit_wheels.py with file cache, check registry, and test suite
fa407664 tool: add fix_isr_safe.py batch annotation script
dadd6a7f docs: sync audit check count and Makefile target count after tool update
01b9a0d6 wheel: add @isr_safe annotations to 98 L0/L1 wheels + fix kvs README order
1293c556 tool: fix 4 audit bugs and add 10 new checks to audit_wheels.py
372104b9 tool: add 6 stress tests for Layer 4 deep audit
6cce583a tool: add 5-layer deep audit system (make audit-deep)
febb8f7f wheel(protocols/ymodem): fix unsigned underflow on oversized transfer
8bfe6b5c wheel(protocols/protobuf_lite): fix UB in ZigZag left-shift
34d32806 wheel(modules/telemetry): reject NaN/Inf in float updates
7197ac59 wheel(primitives/fixed_point): fix 4 integer overflow UB paths
2e158daf wheel: fix strlen truncation in cli_shell and data_logger
5685d5d3 wheel(protocols/msgpack): fix size_t overflow in enc_str/enc_bin
d64e0119 wheel(protocols/mqtt5_packet): fix 4 critical parse vulnerabilities
4b464cb3 wheel(protocols/canbus): fix Motorola signal bounds check
9bb25494 wheel(drivers/stepper): fix INT32_MIN undefined behavior
3c8e8e6e tool: add adversarial test patterns to scaffold templates
974c33d3 wheel: fix 10 NaN bypass vulnerabilities across 8 wheels
078f78bb wheel(protocols/websocket_frame): enforce RFC 6455 64-bit length MSB=0
7170c5ef wheel(drivers/pca9685): fix NaN bypass in set_duty float guard
5fff4bd7 wheel: fix NaN propagation in 3 wheels
93f0f40a wheel: fix HAL struct stored by value in 15 I2C drivers
5030e143 wheel(modules/config_store): reject NaN in float range validation
cd42de9c wheel(protocols/lorawan_packet): fix payload_len uint8_t truncation
c7e96840 wheel(modules/flash_journal): fix buffer overflow in read_nth
d7819721 tool: add fuzz harnesses for 22 L2 protocol parsers
eadfbd36 tool: add ARM Cortex-M4 cross-compile verification script
26d63053 wheel: add physical unit suffixes to 8 wheels
5678486f docs: normalize README section names across 17 wheels
15b65faf tool: add README section name normalization script
40710077 docs: replace generic "Why self-written?" with truthful content
41799972 tool: add wheel open-source search results data
648caa12 wheel: reorder README sections in 69 wheels to standard order
a461098b tool: add README section reorder script
3b345c76 wheel: normalize READMEs across 140 wheels
4f36462e tool: add README normalization script
e98b8dd2 docs: add "Why self-written?" to standard README sections
31f25580 wheel: fix all remaining audit warnings to zero
ac89b24c wheel: apply convention v2 phase 2 across 143 wheels
7e272423 tool: add convention v2 phase 2 fix script
0cf18dfc wheel: fix 6 remaining convention v2 violations
fea5913b wheel: apply convention v2 fixes across 143 wheels
c64be386 wheel: remove "Why not VSF native?" from 124 READMEs
435089a7 tool: add convention v2 batch fix script
68af8d9f docs: auto-fix documentation number drift
d4972125 tool: update scaffold and audit for convention v2
090ddcd6 docs: codify wheel convention v2 across 19 dimensions
26019ce6 tool: add merge_agents.sh, parallel make check, fix CLAUDE.md wheel lists
7eda84d2 tool: optimize audit_wheels.py with consumer cache + --fast mode
b1c07685 wheel: fix 22 MINOR audit findings across all layers
72b412a0 docs: auto-fix documentation number drift
a0e8440d wheel: fix S1 naming + document accepted abbreviations
f96b2769 docs: add missing @thread_safe annotations and complete file headers
9fdb7afb docs: update audit history, known-issues, and wheel counts
e8dbf4c9 wheel(drivers): fix 8 MODERATE audit findings across L1
a2815fa5 wheel: fix 9 MODERATE audit findings across L2 and L3
3785c6cd wheel(primitives): fix 11 MODERATE audit findings across L0
1368cd8e wheel(protocols): fix 4 CRITICAL audit findings across L2
ecf30431 wheel: fix 4 CRITICAL audit findings across L1 and L2
c06bae4f docs: auto-fix documentation number drift
208c6deb wheel: add ads1015 (12-bit ADC) and command_queue (priority dispatch)
e3454fea wheel(modules/flash_journal): add append-only flash journal
d5846742 wheel(drivers/bme280): add I2C temp/humidity/pressure sensor
b36162a1 wheel(modules/version_info): add semantic version comparison
7ab0c6de wheel(drivers/vl53l0x): add I2C time-of-flight distance sensor
a6885a9f wheel(protocols/cbor_lite): add CBOR encoder/decoder
8a0a3ff0 wheel(primitives/string_utils): add embedded string utilities
50da1fa2 wheel(drivers/max31865): add SPI RTD temperature sensor
eb08c5be wheel(protocols/msgpack): add MessagePack binary serialization codec
c635161b wheel(primitives/timer_wheel): add hashed timing wheel
9eaba7ec Merge branch 'main' of https://github.com/L-X-Yao/ai-embedded-platform
91d1b6b1 wheel(drivers/tcs34725): add I2C RGB color sensor
33c08090 wheel(primitives/hash_fnv): add FNV-1a hash function
e663633d wheel(drivers/drv2605): add I2C haptic motor driver
41f783fc wheel(protocols/crc_any): add configurable CRC calculator (1-32 bit)
a0aa3585 docs: auto-fix documentation number drift
593f7de5 wheel: add bme680, boot_counter, param_table (restored from agents)
bb501924 wheel(primitives/lookup_table): add interpolated lookup table
875f919f wheel(protocols/i2c_scan): add I2C bus scanner and device identifier
5a7181a3 wheel(protocols/mqtt5_packet): add MQTT v5.0 packet encoder/decoder
d462c910 wheel(modules/ring_log): add ring buffer structured log storage
010cd9fc wheel(primitives/fifo_queue): add typed fixed-size FIFO queue
55e7614e wheel(drivers/apds9960): add I2C proximity/color/gesture sensor
a9a07e9d wheel(protocols/bencode): add Bencode encoder/decoder
5f3af80c wheel(drivers/lis3dh): add I2C 3-axis accelerometer driver
d5ff7dbf wheel(modules/event_bus): add publish/subscribe event dispatcher
dbf1c3c7 wheel(drivers/mlx90614): add I2C non-contact IR temperature sensor
5730b2bf wheel(protocols/midi_packet): add MIDI message encoder/decoder
00b64be6 wheel(drivers/max31855): add SPI thermocouple temperature sensor
50668f8f wheel(primitives/crc8): add CRC-8 calculator with multiple polynomials
fb83dd2d wheel(drivers/tmp102): add I2C digital temperature sensor
466e8e5a wheel(primitives/moving_window): add sliding window statistics
189119fb wheel(drivers/as5600): add I2C magnetic rotary position sensor
6ac09355 wheel(drivers/opt3001): add I2C digital ambient light sensor
57841401 docs: add L1 driver cross-validation results against GitHub refs
3ba14811 docs: auto-fix documentation number drift
cb1e1c71 wheel(primitives/bitmap): add fixed-size bitmap for resource tracking
6e7b8ed1 wheel(modules/scheduler): add cooperative task scheduler
f01c10bf wheel(drivers/pcf8574): add I2C 8-bit GPIO expander driver
cc95afa2 wheel(drivers/veml7700): add I2C ambient light sensor driver
c898a76c Merge branch 'main' of https://github.com/L-X-Yao/ai-embedded-platform
c53582bc wheel(protocols/ymodem): add YMODEM batch file transfer receiver
ac58f6a6 docs: auto-fix documentation number drift
205da63b wheel(drivers/tca9548a): add I2C 8-channel multiplexer driver
5f5ee881 wheel(protocols/xmodem): add XMODEM/XMODEM-CRC file transfer protocol
5d953662 wheel(drivers/lora_sx1276): add SPI LoRa radio transceiver driver
9d600e90 Merge branch 'main' of https://github.com/L-X-Yao/ai-embedded-platform
348cad5a wheel(modules/button_handler): add high-level button event processor
77f574b1 wheel(primitives/rate_limiter): add generic token bucket rate limiter
0ddcbdc6 docs: auto-fix documentation number drift
b6023770 docs: resolve merge conflict in CLAUDE.md wheel counts
91e75bd6 docs: fix documentation number drift (93→99 wheels)
896120d5 docs: auto-fix documentation number drift
07873b5a wheel(protocols): remove duplicate tlv_packet (keeping tlv_codec)
f7a6d119 wheel(drivers/drv8833): add dual H-bridge DC motor driver
044afd1f Merge branch 'main' of https://github.com/L-X-Yao/ai-embedded-platform
48c2ca20 wheel(modules/telemetry): add periodic telemetry formatter
82cd0c8d wheel(primitives/endian): add portable byte-order conversion utilities
ada5dc02 wheel(protocols/tlv_codec): add Tag-Length-Value encoder/decoder
3c7bc3fd docs: auto-fix documentation number drift
81dd4006 wheel(primitives/hex_codec): add hex string encoder/decoder
6640e0c5 wheel(modules/ota_manager): add firmware update orchestration
b1bcbbb3 wheel(drivers/hx711): add GPIO 24-bit ADC driver for load cells
b501dd6b Merge branch 'main' of https://github.com/L-X-Yao/ai-embedded-platform
63beae74 wheel(drivers/mcp23017): add I2C 16-bit GPIO expander driver
d04d28f7 wheel(protocols/syslog_packet): add RFC 5424 syslog message formatter
6d345659 docs: auto-fix documentation number drift
c7796e79 wheel(modules/device_info): add device identity and runtime info
4eb0babb wheel(modules/task_watchdog): add software task health monitor
852a5cca wheel(modules/error_handler): add fault logging and recovery module
4a8cbb17 wheel(drivers/mcp2515): add SPI CAN bus controller driver
6d7c6cd9 wheel(protocols/dns_packet): add DNS query/response encoder/decoder
73209a3a tool: extend mock_i2c.h with write_regs/delay_ms, add mock_gpio.h
d30ca04f docs: auto-fix documentation number drift
08c8a9b6 wheel(primitives/slip): remove temp file
3ba5ea74 wheel(primitives/slip): add SLIP framing encoder/decoder with streaming
14d4f9ac wheel(drivers/ina219): add I2C current/voltage/power monitor
a7f7a3a1 wheel(drivers/pca9685): add I2C 16-channel PWM driver
da7c625c wheel(modules/config_store): add configuration management module
1f30f984 Merge branch 'main' of https://github.com/L-X-Yao/ai-embedded-platform
c996780b wheel(protocols/canbus): add CAN 2.0A/B frame codec with signal extraction
5528d9aa docs: auto-fix documentation number drift
e0e86e58 wheel(drivers/sd_spi): add SPI-mode SD card block driver
9e504b41 wheel(primitives/fixed_point): add Q16.16 fixed-point math library
bb9f4af2 wheel(drivers/enc28j60): add SPI Ethernet controller driver
d7b95c40 wheel(protocols/lorawan_packet): add LoRaWAN MAC frame encoder/decoder
952c25bb wheel(modules/sensor_hub): add multi-sensor polling and aggregation
874b3818 docs: auto-fix documentation number drift
b8420a30 wheel(drivers/nrf24l01): restore full implementation from agent
c3b83f31 wheel(drivers/nrf24l01): add SPI driver for nRF24L01+ 2.4GHz radio
4be67aa2 wheel: merge uuid: UUID v4 generation and parsing, 20 tests
d3e34627 wheel: merge keypad_matrix: GPIO matrix keypad scanner, 14 tests
a8719428 wheel: merge protobuf_lite: lightweight protobuf wire format, 24 tests
f0e8ee38 wheel: merge data_logger: record-based data logger, 30 tests
38b76145 docs: auto-fix documentation number drift
72c354ea wheel(primitives/cobs): add COBS encoder/decoder for serial framing
96a45518 wheel(drivers/stepper): add step/dir motor driver with acceleration
118466af wheel(modules/cli_shell): add command-line shell module
15c63841 wheel(protocols/modbus_tcp): add Modbus TCP frame encoder/decoder
b446d6a6 wheel(primitives/printf_tiny): add minimal printf for constrained MCUs
2461a70a docs: auto-fix documentation number drift
0c1f9458 wheel(drivers/ds3231): add I2C RTC driver for DS3231
3396b2c4 wheel(drivers/at24cxx): add I2C EEPROM driver for AT24Cxx series
f1c1882c wheel(primitives/base64): add Base64 encoder/decoder
a4376bf7 wheel(primitives/json_parser): add SAX-style JSON parser
edf83219 wheel(primitives/ring_buffer): add byte-level circular buffer
c8a8fd16 docs: sync RTOS-as-detail, update workflow task type 1, add efficiency roadmap
811e6cf7 docs(architecture): application layer standardizes on POSIX API
87bc39e2 docs: auto-fix documentation number drift
c2dc5451 docs: remove VSF grep gate and clarify application layer OS choice
efd0be01 docs: add architecture vision and shift to ecosystem-driven strategy
2e36eb2a tool: add --no-audit to new_wheel.sh, use in test_scaffold.sh
4f41d3d0 tool: add -Wconversion to new_wheel.sh scaffold template
5831bd63 docs(drivers/w25qxx): document W25QXX_BUSY_TIMEOUT config macro
33e0f345 wheel(primitives): add -Wconversion to 26 L0 wheels
ba4cf3f1 wheel(protocols): add -Wconversion to remaining L2 wheels and fix warnings
f2c8e4eb wheel(drivers): add -Wconversion to L1 wheels and fix warnings
774793f5 ci: mark flybox firmware build as non-blocking
23ff27db wheel(protocols): add doc comments and -Wconversion to http_parser and websocket_frame
72af282b ci: fix wget --fail flag in flybox firmware job
78abc098 ci: treat workflow_dispatch and schedule as full regression triggers
647969c0 ci: add workflow_dispatch trigger for manual re-runs
238b4ba3 docs: auto-fix documentation number drift
346d13f6 ci: grant contents write permission for doc drift auto-push
fa2a62ce tool(dispatch_agent): add --cleanup with Windows long-path handling
9b2f0924 docs: fix doc drift after R14 audit round (23 -> 24 checks)
184621b4 wheel(drivers): fix gpio_button duplicate typedef and bh1750 uninitialized var
0080e9d7 tool: add --prompt-file to dispatch_agent.sh and reduce memory_audit false positives
45c1b47b wheel(primitives/soft_timer): add test for SOFT_TIMER_ERR_FULL error code
46348b39 wheel(drivers): add HAL structs to gpio_button and rotary_encoder
f9a0b8b8 wheel(protocols): fix naming convention across 5 L2 wheels
9ff2bf57 wheel(drivers/gpio_button): fix naming convention to use gpio_button_ prefix
8c213ca4 wheel(primitives/mahony_ahrs): fix naming convention to use mahony_ahrs_ prefix
b7f91103 tool: add tooling/logs/ to gitignore
df34a284 tool: add memory_audit.py to detect stale and orphaned memory files
8c314b7f tool: rewrite busy-wait timeout guard to scan all .c files
0f68fda5 tool: add busy-wait timeout guard check to audit_wheels.py
cfaecf67 docs: add dispatch_agent.sh mandatory rule to CLAUDE.md
3c5ed6eb docs: add tooling directory README with structure and quick start
f8224105 tool: restructure tooling/ from flat scripts/ to domain-based subdirectories
647e89b3 docs: sync workflow docs with dispatch_agent.sh and fix audit check count
84ccbd23 docs: sync workflow documentation with claude -p -w dispatch model
d997610a tool: fix merge-all branch parsing, add make check and scope flag
2fa41707 tool: fix dispatch_agent.sh commit and budget flag issues
d4d059e3 docs: fix audit check count drift in ai-workflow.md
ccf2409f wheel: add const qualifiers and improve API symmetry across 8 wheels
93c2a63c tool: add dispatch target, nightly arm-size CI, and pre-commit drift fix
09b3e95f wheel(drivers): implement 3 formerly-dead error code paths with tests
ce5818c7 tool: add dispatch_agent.sh for claude -p -w task dispatch
337e5a40 ci: auto-fix doc drift in wheel-audit job
41c7162f ci: auto-fix doc drift in wheel-audit job
a5e719e1 tool: add dispatch_agent.sh for claude -p -w task dispatch
effe7505 wheel(drivers): implement 3 dead error code paths with tests
ea57bc9b wheel(primitives/log_flash): propagate erase errors and fix flybox consumer
09f1f996 docs: update known-issues with R12 cross-wheel error propagation findings
b9166320 @ wheel(primitives/log_flash): propagate erase errors from log_flash_erase_all
8462b073 docs: fix audit check count drift 22 to 23
1db2c271 wheel(primitives): fix 5 R11 boundary value findings across L0
9e1edd98 docs: update known-issues with R11 audit findings
5ce82b05 wheel(primitives): fix 5 R11 boundary value findings across L0
8f94f12e tool: add rollback to batch_fix, api-convention checks to audit, fix drift count
afd750ce docs: update known-issues with R11 negative test coverage
61203bc7 wheel(drivers): add negative tests for 3 untested error codes
a44536d1 tool: add rollback to batch_fix, api-convention checks to audit
686597f2 Merge branch 'worktree-wheels'
d5458013 wheel(drivers/qmi8658a): add outer (uint8_t) cast on ctrl2/ctrl3 shift-or expressions
26111a8c tool: add fix suggestions to audit warnings and bootstrap tool recommendations
22b0f9e1 Merge branch 'worktree-wheels'
e1867924 docs: fix numeric drift across CLAUDE.md, ai-workflow, and playbook
bc900f96 tool: add parallel test, json audit, make help, and ci doc-drift check
b81a47a5 docs: update known-issues with R10 audit findings
2284138c wheel: fix 8 R10 audit findings and add 6 negative tests across L1-L2
060d06c2 docs: update multi-window section with claude code native capabilities
0cf61ded docs: update known-issues with R9 audit findings
a7f0407f wheel(protocols): fix 7 R9 audit findings across L2
9a3a0fbe wheel(drivers): fix 8 R9 audit findings across L1
b17ea963 wheel(primitives): fix 4 R9 audit findings across L0
393ea77b tool: remove --fix-error-tests stub generator from audit_wheels.py
9db894f0 Revert "wheel: add auto-generated error-code test stubs for 7 wheels"
f1fe0dfa wheel: add auto-generated error-code test stubs for 7 wheels
58f4e068 tool: add session handoff and audit trigger detection
26078782 tool: add --quality mode to platform_metrics
26d79443 tool: add --fix-error-tests auto-generation to audit_wheels.py
5cba8ca1 tool: add zero-init safety test to all scaffold templates
b4e98c7a tool: add error-code test coverage check to audit_wheels.py
7f1556ef docs: add audit-checklist.md and known-issues.md for agent efficiency
f1fcb538 docs: update audit history with round 8 results
c6457b1e wheel: add 3 negative tests closing error code coverage gaps
e73ce7a7 tool: move git tag from release.sh body to next-steps instructions
da5fabee wheel: fix http OWS tab stripping and event_queue modulo-by-zero
2b2ab238 wheel(primitives/cli_parser): fix CRLF double prompt
3ec89451 docs: update audit history with round 7 results
0b273fa9 wheel: add 4 tests closing guard coverage gaps from R4-R6
512b0884 wheel: fix 2 remaining MINOR issues from prior rounds
e53b2947 wheel: fix 3 L2 protocol spec violations from super audit round 7
4a6b5d60 wheel(primitives/log_dedup): fix lines_equal for truncated storage
de63540a docs: update audit history with round 6 results
5992ff72 tool: fix _draft consumer grep using wrong path in bootstrap
d1a92ab7 wheel: fix 6 L2 protocol spec violations from super audit round 6
9552ac5d wheel: fix 3 L1 driver findings from super audit round 6
37f8b43c wheel: fix 3 L0 findings from super audit round 6
6316e24a docs: update audit history with round 5 results
705f1efa ci: replace grep -oP (PCRE) with -oE (ERE) for portability
42dbd4f2 wheel: fix 2 L0 primitive findings from super audit round 5
d02cb78f wheel: fix 3 L1 driver findings from super audit round 5
06e74307 wheel: fix 5 L2 protocol findings from super audit round 5
ad48224b docs: update audit history with round 4 results
242754b5 wheel(drivers/w25qxx): promote BUSY_TIMEOUT to configurable macro
fc0390ab port(gd32f4xx): add timeout guard to flash write busy-wait
60c1beef wheel(protocols/mqtt_packet): add QoS=1 roundtrip and 4-byte RL tests
518633e5 docs(protocols/ota_ab): fix README code example compilation
169a07b3 wheel: fix 4 portability and safety audit findings
39e94f5a wheel(protocols/coap_packet): fix C++ keyword and build guards
a0c74b39 ci: fix first-push empty BASE and unquoted MATRIX in notice
2ac4579a tool: fix BRE/ERE grep portability in downstream and promote scripts
e883224a wheel: fix 8 L2 protocol findings from super audit round 4
bcd155ce wheel: fix 3 L1 driver findings from super audit round 4
c7fbda66 wheel: fix 7 L0 primitive findings from super audit round 4
d288bf9b wheel: fix const correctness and enum bounds from Q25/Q30
aa8c1d88 tool: add secret file patterns to .gitignore
86b2452d docs: update audit history with round 3 results
159baf71 ci: fix flybox build toolchain path
d295d231 tool: fix script bugs from super audit round 3
44df87da wheel: fix 16 MODERATE findings from super audit round 3
911154af wheel: fix 7 CRITICAL findings from super audit round 3
a5b5b136 tool: integrate doc_drift_check into make check
c1fad3e5 docs: update audit history with session 2 results
e9959915 wheel: fix bh1750 enum validation and add unit comments
41f58bba docs: fix phase 0 drift found by doc_drift_check
92a4608b tool: add scaffold compile test as permanent regression guard
7c6a2f20 tool: fix 3 scaffold template compile errors
526751de tool: fix 3 MODERATE findings in release and find_all_refs
488e7ffc tool: fix 6 CRITICAL audit findings in tooling scripts
a5e86dda docs: fix 3 remaining drifts found in methodology audit
7dd31fab tool: add platform metrics tracking and tool decision tree
a8c269d6 docs: fix numeric drift across 3 files via doc_drift_check
28077296 tool: add doc drift checker, commit suggestor, and ref finder
6917bbda tool: add audit auto-fix, batch-fix, and ship workflow
0309d719 ci: auto-detect cross-wheel deps in change detection
0044c51f docs: add session handoff, error recovery, and release workflow
d1d98a2f tool: add platform release script with auto-changelog
a69ae2f2 docs: restructure audit framework from 42 to 40 dimensions
122561cb tool: add ARM binary size audit and fix 2 cross-compile issues
f68460d3 wheel: add doc comments to 19 wheels and fix xgzp6816d enum
303d396a docs: fix drift and add workflow improvements
c3cae4c0 tool: add protocol scaffold, hooks, and dev tools
e0e319c9 wheel: migrate 4 I2C driver tests to shared mock and add SPI mock
f3db9dc1 docs: add multi-window parallel development guide
d0b6c494 tool: add make check and audit --quiet mode
8ea7e26e tool: add clean target and 2 new audit checks
bc906041 tool: add shared test_mock_i2c.h and migrate bmp280 test
e6755230 tool: fix scaffold template root causes from Q23/Q29/Q4 audit
113250e0 docs: add extension protocol and discipline to audit framework
4c5fc528 wheel: kill last 2 surviving mutation test gaps
7c44db71 wheel: add oled_ssd1306 COM pin config test and log stack warning
60411eda wheel: fix enum array bounds and log_emit thread_safe annotation
ed5d554f docs: expand audit framework from 36 to 42 dimensions
0a9d2c38 docs: document undiscovered config macros in 3 wheel READMEs
6756c746 wheel: fix qmc5883p unit doc (uT->Gauss) and gpio_button boundary test
8cbd38d9 docs: expand audit framework from 30 to 36 dimensions
deff5448 docs: expand audit framework from 24 to 30 dimensions
b8e14c17 wheel: replace atoi/atof with strtol/strtof and check erase returns
a910f528 wheel: fix header self-containment and macro hygiene
debd150c docs: expand audit framework from 18 to 24 dimensions
7b5e3414 wheel: fix error code sign convention across 6 wheels
ae1188f6 docs(drivers/qmi8658a): update README for dev_t API refactor
0f7e0424 wheel: fix implicit integer promotion in byte shifts
6c722055 docs: codify batch commit splitting strategy in CLAUDE.md
b0a313b5 docs: expand audit framework from 12 to 18 dimensions
ac2ac44d wheel: add tests for 4 uncovered L1 driver functions
294371bb docs: add audit-dimensions.md with 12-dimension audit framework
bdfee1ef port(gd32f4xx): fix 9 HAL audit findings
3ddd0f99 docs: fix documentation drift across 13 files
fd3a0698 docs: fix README test counts and repo hygiene
798c1c20 ci: skip crc16 from matrix and add cross-wheel dependency triggers
bff26cdc tool: fix 6 tooling script audit findings
6454d564 wheel: fix L2 protocol audit findings
0a8d1936 wheel: fix L1 driver audit findings
97549f49 wheel: fix L0 primitive audit findings
31695d8d wheel(primitives/log): fix 19 audit findings across log ecosystem
8a0220f7 docs: add sink directory table to log README and fill sink READMEs
2352d510 wheel(primitives/log_flash): add persistent flash log sink
b219498a wheel: add log_uart, log_rtt, log_rate, log_dedup sink wheels
07a03dd0 wheel: add log_color, log_file, log_filter sink wheels
3818f5f6 wheel(primitives/log_ringbuf): add RAM circular log buffer
15a22214 wheel(primitives/log): add built-in stdout sink
ccc866cc wheel(primitives/log): decouple from VSF, make pure C99
3982b9f2 tool: add projects/ hygiene scan to audit_wheels.py
52ab7869 tool: clean up directory structure and file placement
f52abcdc docs: update CI description and remove migration scripts
eb6d128f ci: add change detection for O(changed) wheel testing
cde78c8c tool(audit): add TEST/RUN balance check for orphan test detection
89377b60 tool: unify test framework with tooling/test/test_harness.h
7b18da17 docs: close contract-audit gap for all 49 wheels
4d6f13cd wheel(drivers/ads1115): align I2C HAL to standard Pattern A
1beb73ab docs: update wheel registry and counts to 49
c4f76a15 tool: fix 5 workflow gaps found by self-audit
7ff037f9 tool(new_wheel.sh): fix README template for audit compliance
27c7ab2c docs: fix audit compliance for 19 new wheels
ebe68f93 wheel(primitives): add debounce and led_pattern
48cbba66 wheel: add servo rotary_encoder median_filter checksum
a7d8f7e4 ci: add http_parser websocket_frame coap_packet to matrix
03b1b60d wheel(protocols): add http_parser websocket_frame coap_packet
f483b716 ci: add 10 new wheels to test matrix
79ecfa31 wheel(drivers): add dht11 ads1115 bh1750 ds18b20 max7219 hmc5883l
4c7fc43e wheel(primitives/crc32): add CRC32 IEEE 802.3 table-based computation
028c5b8f docs: add post-work review rule to AI work principles
9a420576 ci: re-trigger CI run
0099c5d4 ci: re-trigger after concurrency cancellations
2f6618ad wheel(primitives/event_queue): add circular FIFO event queue
372fb4d5 wheel(primitives/byte_buffer): add linear read/write byte buffer
1ed5a514 wheel(protocols/mqtt_packet): add MQTT 3.1.1 packet serializer/deserializer
bf792d16 wheel(drivers/oled_ssd1306): add SSD1306 OLED framebuffer display driver
cb98ba98 wheel(drivers/mpu6050): add MPU-6050 6-axis IMU driver (accel + gyro)
15145019 wheel(drivers/sht30): add Sensirion SHT30 temperature/humidity driver
7a128b7d ci: add bmp280 to wheel-test matrix
f3f1dd88 wheel(drivers/bmp280): add Bosch BMP280 temperature/pressure sensor driver
729b570d ci: add pid_controller, moving_average, state_machine, soft_timer to matrix
20ffd3ae wheel(primitives/soft_timer): add tick-driven software timer manager
2a5ec0d6 wheel(primitives/state_machine): add table-driven FSM with guards and actions
2cd49b99 wheel(primitives/moving_average): add SMA and EMA signal filters
f1b6c4f6 wheel(primitives/pid_controller): add discrete PID controller with anti-windup
67bd9f66 wheel(primitives/cli_parser): add CR, CRLF, and control-char tests
f86bc7bc wheel(protocols/ota_ab): add boot boundary, target-slot, and CRC vector tests
762afd6f wheel(protocols/nmea_parser): add talker-ID, truncation, and conversion tests
a7cd8857 tool: add root .clang-format and .editorconfig
5d4f9054 wheel(protocols/at_command): add CRLF, error-state, and URC edge tests
71405ff3 wheel(drivers/qmi8658a): add range scaling, reset failure, and 6-axis tests
a2d9f41d docs: add pre-commit hook to CLAUDE.md tooling inventory
32896525 tool: add pre-commit hook to block binary artifact commits
f189cf16 docs: update audit check count 15 to 17 across all docs
20dde602 tool: add responsible-human and heading-consistency audit checks
b193d7d5 wheel(primitives/mahony_ahrs): add gimbal lock, extreme rate, and bias tests
bf774f10 ci: fix pip cache path and working-directory resolution
3c801f6f wheel(protocols/modbus_rtu): fix GCC 13 uninitialized warning in overflow test
1f6e375f docs: fix integrating-wheels VSF example and add cross-references
495e21ca docs: standardize wheel README sections across 19 wheels
fe17727e ci: add concurrency group and job timeouts
019a55dd wheel(protocols/modbus_rtu): fix uint16 overflow in frame_len computation
67166ffc docs: add git hooks setup instruction to debugging playbook
922ae5f2 ci: add pip caching and standardize on python3
6da8a3ea tool: replace grep -P with grep -E and python with python3
9c59de44 tool: fix commit-msg hook grep -P portability for Windows Git Bash
04adb2fc docs: fix vsf upstream sync instructions to match vendored setup
4a7c16ca tool: add sync_vsf.sh for upstream synchronization
8226d543 wheel(protocols/modbus_rtu): fix uninitialized array warning with GCC 13
ae8b5023 tool: fix local keyword outside function and add downstream to playbook
5e3843fe docs: update CLAUDE.md tooling section with current capabilities
6b6a5bef tool: add downstream convenience target to Makefile
2f73dbf9 wheel(drivers/gpio_led): add breathe/blink boundary and large-dt tests
0679f723 tool: add -Werror enforcement check to audit_wheels.py
6c774e54 docs: add test-projects to Makefile header and build verification to template
2a9c861e tool: add pre-flight validation to wheel_promote.sh
48bbbe55 docs: add missing make targets to debugging playbook
cd85c999 wheel: add -Werror to all non-vendor wheel CMakeLists
4f3c4bb7 wheel(protocols/ncp5623_parser): add toggle, non-standard enable, and register boundary tests
f1c6585c wheel(primitives/mag_cal): add degenerate input and null output tests
e4f8c851 wheel(primitives/crc16): standardize test output format for test_all.sh
8c533c33 tool: add body line length warning to commit-msg hook
538d9335 tool: add null safety and SPI error injection to new_wheel.sh templates
9aa13fa6 wheel(drivers/gpio_button): add boundary timing and triple-click tests
8b6d4daa wheel(drivers/ws2812): add color corner, mid-gray, and critical section isolation tests
7f41a3c6 wheel(primitives/crc16): add edge case tests for buffer patterns and CRC self-check
28e9bf90 docs: fix ota_updater table formatting in WISHLIST.md
b6d857f4 docs: add make targets quick reference to debugging-playbook
a254aa85 docs: update w25qxx and xgzp6816d README test counts
9db04127 tool: add null safety starter test to SPI template in new_wheel.sh
cbe1f87b tool: rename TESTS column to ASSERTS in session_bootstrap.sh
71957c1c wheel(drivers/w25qxx): add page boundary and JEDEC validation edge tests
5e65246d wheel(drivers/xgzp6816d): add intermediate pressure and temperature tests
9cd4f7b2 docs: update test counts for qmi8658a, qmc5883p, mahony_ahrs, cli_parser
93e4bba8 tool: show build output on failure in test_downstream.sh
10a223db wheel(drivers/qmc5883p): add mid-init I2C failure tests for all steps
70a7e48a tool: add non-ASCII rejection to commit-msg hook
a8133726 wheel(primitives/cli_parser): add tab tokenization tests
aa67ad1a wheel(drivers/qmi8658a): add mid-init I2C failure tests for CTRL2/3/7
e9cb181c wheel(primitives/mahony_ahrs): add safety guards for zero freq and quaternion norm
06c61799 docs: fix wheel count and ota_ab location in README.md
8c2e9d30 docs: update L2 wheel README test counts and fix ota_ab path
2c95901c wheel: add edge case tests to L2 protocol wheels
fed93f79 tool: add test-fast and graph targets to Makefile
1ab877fe tool: add consumer mapping summary to audit report
3074833f tool: add platform detection and --fail-fast to test_all.sh
891e2cb4 tool: add --graph flag to audit_wheels.py for dependency visualization
4877f82b ci: add pull request template with checklist
ad9ab985 docs: add Makefile and commit-msg hook to CLAUDE.md tooling list
e554eeb1 ci: add parse_fault.py pytest to flybox-tests job
d7267d84 docs: document git tag versioning convention for wheels
dddbc2a3 tool: add Origin section to new_wheel.sh README template
95b6210c tool: add test suite for parse_fault.py Cortex-M4 fault decoder
69c0c873 port(gd32f4xx): update HAL port status from draft to HIL verified
ac706a98 tool: add top-level Makefile for common operations
e12506c5 docs: codify test output format requirement in wheel-contract
e7fb54ef wheel(protocols/nmea_parser): add equator and prime meridian boundary test
9a972333 wheel: standardize test output to "N tests, M passed, M failed"
6387839e docs: update ota_ab references after promotion to candidate
60ae28bc docs: fix duplicate section numbering in api-conventions.md
8f1601c3 tool: add --coverage flag to test_all.sh for gcov instrumentation
115b67ba wheel(protocols/ota_ab): promote from _draft to candidate
65055558 docs: add integrating-wheels.md link and update audit check count
fa1bad1c tool: add commit-msg hook enforcing CLAUDE.md conventions
8e9f1165 docs: update root README.md to reflect current platform state
4897852e docs: add new project kickoff checklist to CLAUDE.md
39a17c87 docs: add registry table and ota_ab draft to wheels/README.md
d41a2861 docs: add wheel integration guide with CMakeLists.txt examples
6626175b docs: add Origin sections to cli_parser, crc16, and log READMEs
66b80ce9 ci: switch wheel-test and flybox-tests to ctest
aa641a6b ci: add flybox project tests job and ELF header validation
4145b984 ci: add ELF header validation to flybox build job
d9f556ac tool: add test exit code validation to audit_wheels.py
d2dd6c8e docs: add README to tooling/experiments/ documenting scaffold status
9e361c20 docs: add mag_cal to CI matrix, catalog, and CLAUDE.md
bea0fc0a wheel(primitives/mag_cal): add min/max magnetometer calibration
9b3af51a tool: add @thread_safe annotation check to audit_wheels.py
08859f50 docs: add ota_ab to CI matrix, update WISHLIST and CLAUDE.md counts
b1dfd70e wheel(_draft/ota_ab): add A/B firmware slot manager draft
a2cd9262 docs: add mahony_ahrs to CI matrix, catalog, and CLAUDE.md
f04d1c72 wheel(primitives/mahony_ahrs): add Mahony AHRS complementary filter
dd2f4c2f tool: fix test_all.sh summary regex to match both output formats
d14c3559 docs: add nmea_parser to CI matrix, catalog, and CLAUDE.md
51321b22 wheel(protocols/nmea_parser): add NMEA 0183 sentence parser
243279e5 docs: update test counts for gpio_led, cli_parser, w25qxx, ws2812
bb4d32d2 tool: add extern "C" guard check to audit_wheels.py
ec664d80 docs: codify extern "C" guard and NULL pointer guard requirements
9d8075b6 wheel: add edge-case tests to w25qxx and ws2812
ab755dc8 wheel(primitives/cli_parser): add NULL commands guard and normalize test variable
c96a1799 wheel(drivers/gpio_led): add zero-duration guard to tick_blink
e434b8e2 tool: update new_wheel.sh scaffold to match current conventions
af3973f1 docs: update test counts in protocol and parser READMEs
5f3c572f wheel: add boundary and edge-case tests to protocol and parser wheels
86caa7aa wheel: add extern "C" guards to all wheel headers
e1d8229c wheel: add NULL guards and defensive tests to w25qxx and ncp5623_parser
370d2cf2 docs: add tooling script references to ai-workflow and wheel-contract
53788845 docs: add convention deviation sections to driver READMEs
7c640282 wheel: add re-init and boundary tests to gpio drivers
adf25150 wheel: add NULL guards and defensive tests to sensor drivers
58cf20e0 tool: enable -Wall -Wextra -Wpedantic in all wheel test builds
687c068e wheel: add edge-case tests for at_command, modbus_rtu, cli_parser, qmc5883p
ca6067f9 docs: fix stale counts in crc16, cli_parser, qmi8658a READMEs and CLAUDE.md
9143b6f7 wheel(drivers/w25qxx): add stuck-busy timeout test
46f33ae0 docs: document convention deviations in log and at_command
dc9ee9ae tool: add Supported platforms and Test status to wheel scaffold
1477301e project(flybox): move flybox scripts to projects/flybox/scripts/
fcde3e86 docs: add WISHLIST sync step to task type 2
9bc73fc6 tool: add candidate README section check to wheel audit
4e8a3ae1 docs: add candidate-level sections to 8 wheel READMEs
6e3c2e37 tool: add file header audit check and clarify log convention
62bfc5e4 docs: update wheels/README.md catalog and remove stale TODO
afca50a5 wheel: add file headers and thread-safety annotations to all wheels
fdc63450 wheel(primitives/cli_parser): add minimal CLI parser for debug shells
67271e7f docs: fix ssd1306 WISHLIST VSF status after zero-rule grep
67fa437c tool: add ctest registration check to wheel audit
4b9cfcfd docs(primitives/log): translate Chinese headings and content to English
9f0f2b0f docs: add trigger count columns to WISHLIST tables
0cf535d2 docs(primitives/crc16): translate Chinese headings to English
a02540d1 tool: enforce C99 standard in scaffold and audit
db7e986c tool: add test_downstream.sh for cross-wheel regression
9d7f2c60 docs: standardize API review heading and fix doc drift
52354bdf wheel: enforce C99 standard in all test builds
088e9450 tool: audit API review, matrix CI snippet, vendor promote guard
07a3cea4 docs: add ADR-009/010 guidance to project template
5e3c0ac0 ci: collapse 12 identical wheel jobs into matrix
0c37d4f7 wheel(primitives): add ctest registration to crc16 and log
933ad539 tool: improve new_wheel.sh README template
d4c93c81 docs: fix VSF path errors in debugging playbook
daf44e61 docs: reconcile error handling convention with practice
a7f939f7 docs: align wheel-contract with current reality
1b15928d tool: fix session_bootstrap accuracy
d16a590b tool: add --vendor mode to new_wheel.sh
12aa09d6 tool: extend wheel audit with ADR-006/009/010 checks
d02104f3 docs(primitives): align "Why not VSF native?" heading to English
840521ba wheel(primitives/kvs): add KV storage via FlashDB (vendor)
8efefad8 wheel(primitives/fal): add FAL flash abstraction layer (vendor)
c16b59e3 docs(adr): add ADR-010 adopt open-source before reinventing
3c15492a wheel(protocols/modbus_rtu): add Modbus RTU codec with master/slave
aa9092f7 tool: relax audit consumer check to WARN for proactive wheels
da584a38 docs(adr): accept ADR-009 wheel dependency rules
369f62e9 docs(adr): add ADR-009 wheel dependency rules
61009d9d wheel(protocols/at_command): add generic AT command engine
af649d2c wheel(drivers/gpio_button): add debounce + gesture driver
a04dc028 wheel(drivers/gpio_led): add LED state machine driver
30d8f4a5 tool: extend wheel audit with include-scan and origin-check
ddf94ae8 wheel(drivers/ws2812): extract WS2812 LED driver from drone_light
b8c18ed8 docs: update status — HIL automation and project template now complete
5f7fb4bc tool(hil): add soak-test runner with result archival and comparison
ec424bb4 docs: add project-integration template
f8a60a02 docs: update platform status after flybox compat + wheel sink completion
cfad2bf4 docs: upgrade 4 driver wheel READMEs to candidate level
3c51a548 tool: add wheel_promote.sh and test_all.sh --changed mode
f8e1db6b tool(gd32f4xx_debug): add parse_fault.py cortex-m fault decoder
a872d199 tool(build_flash): add structured JSON verdict to HIL check
97081625 tool: add session_bootstrap.sh session context dump
ff7b01da tool: add new_wheel.sh wheel scaffold generator
6b21c0ab tool: add test_all.sh unified test runner
53af31d5 wheel(drivers/qmc5883p): add wake register, tests, README, and CI job
e64ce185 wheel(drivers/qmc5883p): add platform-agnostic magnetometer driver
678575c6 docs(wheels): add ncp5623_parser to WISHLIST (drone_light trigger ×1)
23ac8d95 docs: codify task 9 sink three-step + audit guardrail cross-link
0801b324 ci(xgzp6816d): add wheel build + test job
e696541c wheel(drivers/xgzp6816d): add platform-agnostic baro driver
7637ab0e tool(gd32f4xx-debug): drop superseded analyze_stack.py
4d1f8495 ci(crc16): update working-directory after primitives/ promotion
da4f5f62 wheel(primitives/crc16): promote from _draft/ to primitives/, status candidate
ba6b22bb ci(wheel-audit): add audit_wheels.py as lint job
acfb7826 docs(wheels): drop stale w25qxx from WISHLIST
e74c805d tool(audit-wheels): add orphan + WISHLIST drift audit
60e9869f ci(qmi8658a): add wheel build + test job
6ba6d073 ci(w25qxx): add wheel build + test job
070c6423 tool(gd32f4xx-debug): remove stale diag config duplicates at root
31e1a1bb ci(crc16): add crc16 wheel build + test job
a0d1d478 wheel(_draft/crc16): add crc16-modbus wheel + tests
2410226f ci(flybox): assert gcc-arm-none-eabi major version 13
3d99198c ci: upgrade actions to node.js 24 compatible versions
f5116280 docs: add ci badge to readme
f34fa457 ci(flybox): exclude cmsis core headers from sdk stage
8a74ff8e ci(flybox): stage gd32 vendor sdk before build
4e84024b ci(flybox): add gd32f405 cross-compile + stack analysis job
68d7a349 docs: add ci commit type for github workflows
38a9cef1 tool(gd32f4xx_debug): split stack_tasks into template + flybox config
42d6d532 tool(gd32f4xx_debug): add porting guide for other MCUs
a01903fa tool(_phase_b_archive): archive obsolete debug tcl scripts
687582b7 tool(build_flash): add generic build/flash/check pipeline
d7afa15c docs: add gd32f4xx_debug pack reference in playbook
0b2a9760 tool(gd32f4xx_debug): add reusable cortex-m4 + gd32f4 debug pack
01e09ab9 docs: add tool commit type for tooling content
a3322d8b project(flybox): add openocd halt-and-dump diagnostic scripts
15818eb2 project(flybox): add static stack analyzer and gdb fault decoder
f960386f wheel(drivers/w25qxx): add test coverage for edge cases
c26aabfc wheel(drivers/qmi8658a): fix shift UB and add boundary tests
cd45e8a6 wheel(drivers/w25qxx): add platform-agnostic SPI NOR flash driver
bbd11413 wheel(drivers/qmi8658a): add platform-agnostic 6-axis IMU driver
23107aa3 project(flybox): preserve OpenOCD TCL debug scripts from HIL session
0cd98efe exp: add HIL tooling and bare-metal uart test
92a04dc8 project(flybox): fix three hardware boot blockers found by HIL
3a9e5318 port(gd32f4xx): fix i2c rx/tx timing bugs and mark stubs
d49c62d9 docs: add project commit type for projects/ changes
c4776a28 port(gd32f4xx): add clkrst region function declarations
6058c810 port(gd32f4xx): fix build compat for flybox full source inclusion
ae27d2df port(gd32f4xx): fix link errors for flybox vsf build
24947073 port(gd32f4xx): fix spi/i2c/device symbols for vsf template compat
75c0aa47 port(gd32f4xx): add driver.c and cmake build infrastructure
1281607f docs: update gd32f4xx hal status after flash/spi/i2c addition
efbee9b6 port(gd32f4xx): add flash, spi, and i2c hal driver skeletons
60e168c0 docs: update gd32f4xx hal status after usart addition
07ad33db port(gd32f4xx): add usart hal driver skeleton
f1fb6249 docs: add CI status to current state section
9aa8c714 docs: integrate drone_light as second real project
56d927fa exp(vsf-tryouts/linux_shell_size): verify usleep, posix_queue, event_group on qemu
603d2db4 docs: sync posix verification results to platform docs
e778990e exp(vsf-tryouts/linux_shell_size): verify posix pthread/sem/recursive-mutex on qemu
75a32d61 exp(vsf-tryouts/linux_shell_size): measure vsf linux shell size on cortex-m
9493e4b6 port(gd32f4xx): add vsf hal driver skeleton with gpio
bb987c78 docs: integrate flybox as first real project
b2bc1b8e docs: add log wheel to primitives and wheels directory listings
ca63af30 docs: remove stale hardcoded counts from CLAUDE.md status section
c23859d5 docs: fix quickstart commands and stale counts in README and CLAUDE.md
0947e418 docs: fix broken memory/ directory references across all docs
76558d65 docs(primitives/log): fix test count and document missing APIs
05f6e978 wheel(primitives/log): set status to candidate and add log.c file header
50d1501a wheel(primitives/log): trim unnecessary VSF features from config
7155cb0f docs: fix task type ordering and count mismatch
1f1d85b5 wheel(primitives/log): add volatile to shared state and clean up tests
69099202 wheel(primitives/log): auto-build release binary in pytest fixture
49a8391f docs: fix CI missing libbsd-dev dependency
fd83fde4 docs: add github actions CI for log wheel
bd7fe92e exp(vsf-tryouts/mps2_bare): sync log wheel path after promotion
d37dc9bd docs: update all _draft/log references to primitives/log
d2677072 wheel(primitives/log): promote log from _draft to primitives
ede47ef4 docs: add proprietary license
097ffbbf docs: add repository README
964f3af0 docs: add mps2 -device help proactive checklist
3f49581a docs(adr): absorb usb probe findings into 006 and 008
0f99ce27 docs(_draft/log): note rtdbg.h-inspired follow-ups
5979c9c5 docs: add qemu semihosting stderr pitfall to mps2 checklist
0628f028 exp(vsf-tryouts/mps2_bare): add pytest regression harness
35019f5b docs(_draft/log): mark mps2 bare-metal proven in platform support
4a248140 exp(vsf-tryouts/mps2_bare): consume wheels/_draft/log via semihosting sink
3eecb7b7 docs: add vsf usbip linux-host gap pitfall to playbook
eabe67c6 docs(adr): note usbip linux-host gap in ADR-008 option B
6e16bfc9 exp(vsf-tryouts/mps2_bare): upgrade probe to teda+sem test
1869ed7a docs: add vsf_start_trace BSP-dependency pitfall to mps2 checklist
d07ee76a exp(vsf-tryouts/mps2_bare): add mps2 bare-metal sim scaffold
da07dbe3 docs: codify mps2 bare-metal sim as verified first-class path
0841d63a docs(adr): refine ADR-008 with mps2 feasibility sanity check
13c2afa6 exp(vsf-tryouts/kernel_test): add linux64 sim scaffold
5f6bf7af docs: codify vsf linux64 sim findings as ADR-008
7dfee203 docs: drop Co-Authored-By footer per user preference
47725ab9 docs: tighten commit convention to strict closed-set
f50f4308 docs: codify commit convention from existing git log
70099ee2 docs: freeze Sprint 0, trim WISHLIST, relax draft README scope
96622518 docs: codify self-review as workflow task type 11, update status
6b3402f3 docs: tighten conventions, single-source VSF-native rule, soften ROI
b4d0e399 wheel(_draft/log): true compile-time level filter
124d931d wheel(_draft/log): first wheel — leveled + module-tagged logging
f721cec9 docs: add wheels wishlist (not a TODO, just anticipated candidates)
a15306d4 chore: gitignore Claude local state
1e0427f4 chore: vendor VSF upstream snapshot
975b4da3 chore: initial platform scaffolding
```

