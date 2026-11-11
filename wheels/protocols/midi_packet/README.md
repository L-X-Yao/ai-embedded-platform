# midi_packet

**Status: candidate**
**Layer: L2 (protocols)**

MIDI message encoder/decoder. Streaming byte-at-a-time parser with running
status support. Builder functions for Note On, Note Off, Control Change,
Program Change, Pitch Bend, and SysEx. Pure C99, no heap.
## Origin

Proactive build. MIDI is the standard protocol for musical instrument
control, stage lighting, and audio equipment. Embedded controllers
driving synthesizers, LED rigs, or show-control systems need MIDI
encoding/decoding at the byte level.


## Why self-written?

Searched GitHub (2026-05-02). Found abique/midi-parser (varies license). MIDI protocol is simple and implementations converge. Self-written for unified error handling and zero-allocation guarantee.

## Dependencies

None. Pure C99 with `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<string.h>`.

## API

```c
/* --- Builder functions (return encoded length, or negative error) --- */
int midi_build_note_on(uint8_t *buf, size_t cap,
                       uint8_t channel, uint8_t note, uint8_t velocity);
int midi_build_note_off(uint8_t *buf, size_t cap,
                        uint8_t channel, uint8_t note, uint8_t velocity);
int midi_build_cc(uint8_t *buf, size_t cap,
                  uint8_t channel, uint8_t number, uint8_t value);
int midi_build_program(uint8_t *buf, size_t cap,
                       uint8_t channel, uint8_t program);
int midi_build_pitch_bend(uint8_t *buf, size_t cap,
                          uint8_t channel, int16_t value);

/* --- Streaming parser --- */
void midi_parser_init(midi_parser_t *p);
bool midi_parse(midi_parser_t *p, uint8_t byte, midi_msg_t *out);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `MIDI_SYSEX_MAX` | 128 | Maximum SysEx payload size in bytes |

## Supported platforms

- [x] Host (sim) — passed 2026-05-01
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

- SysEx data beyond `MIDI_SYSEX_MAX` is silently dropped.
- A SysEx ended by a status byte other than EOX is dropped, not delivered:
  the status byte is parsed as usual, and only an EOX yields `MIDI_MSG_SYSEX`.
- System Common messages (0xF1-0xF6) other than SysEx are ignored.
- No support for Poly Aftertouch (0xA0) or Channel Pressure (0xD0) parsing
  (data bytes are consumed but no message is emitted).

## How to test

```bash
cd wheels/protocols/midi_packet/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_midi_packet
```

60+ assertions across 20 test cases: builder encoding (Note On/Off, CC,
Program Change, Pitch Bend center/extremes/channel), error handling (null,
buffer overflow, range validation), streaming parser (channel voice, running
status, velocity-0 as Note Off, Program Change, Pitch Bend, SysEx,
real-time byte transparency), null parameter safety, build-then-parse
roundtrip verification.

## Test status

- Unit tests (sim): PASS (20 C tests, 60+ assertions)
- HIL: N/A

## API convention deviations

Build functions return `int` (packet length or negative error) instead of
`midi_packet_err_t`, because callers need the length to know how many bytes
to send over UART.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Streaming byte-at-a-time parser (MIDI arrives one byte at a time over UART, buffering full messages externally is wasteful); running status support (mandatory for real-world MIDI streams where repeated status bytes are omitted); Note On with velocity 0 mapped to Note Off (per MIDI 1.0 spec); pitch bend as signed -8192..+8191 (center=0, matching musical semantics rather than raw 14-bit encoding); SysEx buffer size configurable via compile-time macro (embedded systems need bounded memory).

## Responsible human
@L-X-Yao
