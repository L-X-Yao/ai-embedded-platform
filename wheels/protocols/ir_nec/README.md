# ir_nec -- NEC infrared remote control protocol encoder/decoder

**Status: candidate**
**Layer: L2 (protocols)**

NEC infrared remote control protocol encoder/decoder. Handles standard NEC (8-bit address +
complement) and extended NEC (16-bit address, no complement). Pure timing-based encode/decode
on pulse arrays -- no HAL needed, carrier modulation is handled externally. Includes repeat
frame detection.

## Origin

Proactive build for IR remote control integration in embedded projects. NEC is the most
common IR protocol worldwide, used by most consumer electronics remotes.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-12):

- IRremote (Arduino, LGPL) -- Arduino-specific, timer-dependent, heavy.
- IRLib2 (MIT, C++) -- C++/Arduino, hardware-coupled.
- Existing C implementations are all tied to specific timer peripherals.
- No suitable zero-dependency C99 pure encode/decode implementation found.

Reference: SB-Projects NEC protocol specification, Altium NEC protocol guide.

## Dependencies

None.

## API

```c
/* Encode a standard NEC message */
ir_nec_msg_t msg = { .address = 0x00, .command = 0x0C };
ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
uint16_t count;
ir_nec_encode(&msg, pulses, &count);  /* count = 34 */

/* Decode received pulses */
ir_nec_msg_t decoded;
ir_nec_decode(pulses, count, &decoded);

/* Repeat frame */
ir_nec_pulse_t repeat[3];
uint16_t rcount;
ir_nec_encode_repeat(repeat, &rcount);  /* rcount = 2 */
bool is_rep = ir_nec_is_repeat(pulses, count);

/* Extended NEC (16-bit address) */
ir_nec_pulse_t ext[IR_NEC_PULSE_COUNT];
uint16_t ecount;
ir_nec_encode_extended(0x1234, 0x56, ext, &ecount);

uint16_t addr; uint8_t cmd;
ir_nec_decode_extended(ext, ecount, &addr, &cmd);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `IR_NEC_TOLERANCE_PCT` | 20 | Decode timing tolerance percentage |
| `IR_NEC_PULSE_COUNT` | 34 | Pulses per standard frame (protocol-fixed) |

## Protocol details

- Leader: 9ms mark + 4.5ms space (normal) or 9ms mark + 2.25ms space (repeat)
- Data: 32 bits = address(8) + ~address(8) + command(8) + ~command(8)
- Bit encoding: 562.5us mark, then 562.5us space (0) or 1687.5us space (1)
- LSB first within each byte
- Extended NEC: 16-bit address (low byte + high byte, no complement), command + complement
- Carrier: 38kHz (not modeled -- HAL handles modulation)

## Supported platforms

- [x] Host (sim) -- passed 2026-05-12
- [ ] Real hardware -- not yet verified

## Known issues

- 38kHz carrier modulation not handled (hardware/timer responsibility).
- Only NEC and extended NEC supported. NEC2 (no repeat, resend full frame) uses the same
  encoding but different repeat logic -- consumer must handle at application level.

## How to test

```bash
cd wheels/protocols/ir_nec/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ir_nec
```

## Test status

- Unit tests (sim): PASS (16 C tests)
- NULL tests: generated
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-12
- **Key decisions**: Pure encode/decode on pulse timing arrays (no HAL coupling);
  separate standard and extended NEC APIs; timing tolerance configurable via macro;
  repeat frame as separate encode/detect functions; stop bit included in pulse count;
  complement validation on decode rejects corrupted frames.

## Responsible human

@user
