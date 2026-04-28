# ncp5623_parser — NCP5623 I2C color protocol parser

**Status: candidate**

Pure-software parser for the NCP5623-compatible LED color protocol used by ArduPilot flight controllers.
Zero hardware dependency — feed raw I2C bytes in, get structured color/enable events out.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`ncp5623`, `i2c_slave_parse`, `led_protocol`, `i2c.*color` — zero results.

VSF has no application-layer I2C protocol parsers.

## Origin

Extracted from `projects/drone_light/Src/i2c_slave.c` (NCP5623 protocol state machine).
Decoupled: I2C hardware handling stays in the project, parser is pure byte-stream logic.

## Protocol

NCP5623-compatible subset used by ArduPilot (`AP_Notify/NCP5623.cpp`):

| Command | I2C data (after address) | Description |
|---------|--------------------------|-------------|
| ENABLE  | `[0x04] [0x03]`          | Enable LED output |
| DISABLE | `[0x04] [0x00]`          | Disable LED output |
| COLOR   | `[0x01] [B] [G] [R]`    | Set color (4-bit per channel, 0x00-0x0F) |

Color commands are ignored when parser is in disabled state.

## API

```c
ncp5623_parser_t parser;
ncp5623_parser_init(&parser);

/* In I2C address-match ISR: */
ncp5623_parser_start(&parser);

/* In I2C data-received ISR: */
ncp5623_evt_t evt;
ncp5623_evt_type_t r = ncp5623_parser_feed(&parser, byte, &evt);
if (r == NCP5623_EVT_COLOR) {
    /* evt.data.color.r/g/b are 4-bit (0..15) */
    set_led(evt.data.color.r, evt.data.color.g, evt.data.color.b);
}

/* In I2C stop-condition ISR: */
ncp5623_parser_stop(&parser);
```

## Dependencies

- None (pure software, no external dependency)

## Supported platforms

- [x] Host (sim) — passed 2026-04-27
- [ ] N32G003 (drone_light) — original project code verified, wheel not separately HIL tested

## Known issues

- Only handles the ArduPilot NCP5623 subset (ENABLE + COLOR). Real NCP5623 has PWM, gradual dimming, and off-timer registers — not implemented.
- Color output is raw 4-bit. Brightness mapping (e.g., non-zero → full brightness) is left to the consumer.
- The `enabled` flag persists across I2C transactions (by design — ArduPilot sends ENABLE once at boot, then only COLOR commands).

## Tests

```bash
cd wheels/protocols/ncp5623_parser/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ncp5623_parser
```

40 assertions across 13 test cases: init state, enable/disable, color parsing, enable gating, 4-bit masking, unknown register rejection, persistence, short packet, null safety, off color.

## Test status

- Unit tests (sim): PASS (13 C tests, 40 assertions)
- HIL: N/A

## API review

- **Date**: 2026-04-27
- **Key decisions**: Parser is stateful (tracks `enabled` across transactions); output is raw 4-bit values (consumer decides brightness mapping); feed-byte API for ISR-safe operation (no buffering, immediate event emission); transaction boundaries via `start`/`stop` calls.
