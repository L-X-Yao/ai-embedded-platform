# ncp5623_parser — ToshibaLED (TCA62724) I2C color protocol parser

**Status: candidate**

Pure-software parser for the LED color protocol that ArduPilot's ToshibaLED driver sends to a
TCA62724 at I2C address 0x55. Zero hardware dependency — feed raw I2C bytes in, get structured
color/enable events out.

**This is not the NCP5623 protocol, despite the name.** A real NCP5623 master
(`AP_Notify/NCP5623.cpp`, address 0x38 or 0x39) sends single bytes of
`(reg & 0xE0) | (data & 0x1F)`, which give no events here. The name stays because renaming the
wheel would change its API.

## Origin

Reimplemented for the needs of an internal project, which called this protocol NCP5623.
Decoupled: I2C hardware handling stays in the project, parser is pure byte-stream logic.

## Why self-written?

Searched GitHub (2026-05-02). No suitable lightweight zero-dependency C99 implementation found. Trivial register-write format specific to ArduPilot's ToshibaLED driver. Self-written per platform conventions.

## Dependencies

- None (pure software, no external dependency)

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

## Supported platforms

- [x] Host (sim) — passed 2026-04-27
- [ ] N32G003 (drone_light) — original project code verified, wheel not separately HIL tested

## Known issues

- Only handles what ArduPilot's ToshibaLED driver sends (ENABLE + COLOR). The real NCP5623 format, with its PWM and gradual dimming registers, is not implemented.
- Color output is raw 4-bit. Brightness mapping (e.g., non-zero → full brightness) is left to the consumer.
- The `enabled` flag persists across I2C transactions (by design — ArduPilot sends ENABLE once at boot, then only COLOR commands).

## How to test

```bash
cd wheels/protocols/ncp5623_parser/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ncp5623_parser
```

77 assertions across 22 test cases: init state, enable/disable, color parsing, enable gating, 4-bit masking, unknown register rejection, persistence, short packet, null safety, off color, re-init state reset, empty transaction, extra-long packet, disable-then-color gating, rapid enable/disable/enable toggle, non-standard enable values (0x01/0xFF), register address boundaries (0x00/0x02/0x03/0x05/0xFF), ToshibaLED traffic, real NCP5623 traffic.

## Test status

- Unit tests (sim): PASS (22 C tests, 77 assertions)
- HIL: N/A

## API convention deviations

None.


## Protocol

The TCA62724 writes ArduPilot makes at I2C address 0x55 (`AP_Notify/ToshibaLED_I2C.cpp`):

| Command | I2C data (after address) | Description |
|---------|--------------------------|-------------|
| ENABLE  | `[0x04] [0x03]`          | Enable LED output |
| DISABLE | `[0x04] [0x00]`          | Disable LED output |
| COLOR   | `[0x01] [B] [G] [R]`    | Set color (4-bit per channel, 0x00-0x0F) |

Color commands are ignored when parser is in disabled state.

## API review

- **Date**: 2026-04-27
- **Key decisions**: Parser is stateful (tracks `enabled` across transactions); output is raw 4-bit values (consumer decides brightness mapping); feed-byte API for ISR-safe operation (no buffering, immediate event emission); transaction boundaries via `start`/`stop` calls.

## Responsible human

@user
