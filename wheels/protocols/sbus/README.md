# sbus -- Futaba S.Bus RC receiver protocol parser and encoder

**Status: candidate**
**Layer: L2 (protocols)**

Byte-feed S.Bus protocol parser with frame detection, 16-channel 11-bit decode/encode,
digital channel flags (CH17/CH18), failsafe/frame-lost detection, and channel-to-microsecond
conversion. Zero OS dependency -- consumer feeds bytes from any UART source.

## Origin

Proactive build for RC receiver integration in drone/robotics projects. S.Bus is the
de-facto standard protocol for Futaba-compatible RC receivers (FrSky, RadioMaster, etc.).

## Why self-written?

Searched GitHub (2026-05-07). Found bolderflight/sbus (MIT, C++/Arduino-specific, pulls in
Serial dependency), zendes/SBUS (C++), and various Arduino-only implementations. No suitable
zero-dependency C99 implementation found that fits our wheel contract (no OS, no heap, pure
parsing library). Self-written for platform convention consistency.

## Dependencies

None.

## API

```c
/* Decode a complete 25-byte frame */
sbus_frame_t frame;
uint8_t raw[SBUS_FRAME_LEN]; /* received from UART */
sbus_err_t err = sbus_decode(raw, &frame);
/* frame.channels[0..15]: 0-2047 */
/* frame.ch17, frame.ch18: digital channels */
/* frame.frame_lost, frame.failsafe: status flags */

/* Encode a frame for transmission */
uint8_t buf[SBUS_FRAME_LEN];
sbus_encode(&frame, buf);

/* Byte-at-a-time parser (for streaming UART) */
sbus_parser_t parser;
sbus_parser_init(&parser);

/* In UART RX interrupt / callback: */
sbus_parse_state_t st = sbus_parse_byte(&parser, byte);
if (st == SBUS_PARSE_COMPLETE) {
    sbus_frame_t f;
    sbus_decode(sbus_parser_get_buf(&parser), &f);
    /* process f */
}

/* Channel value to servo microseconds */
uint16_t us = sbus_channel_to_us(frame.channels[0]);  /* 172-1811 -> 1000-2000 */
uint16_t ch = sbus_us_to_channel(1500);                /* 1000-2000 -> 172-1811 */
```

## Configuration (compile-time)

No compile-time configuration needed. All constants are fixed by the S.Bus protocol specification.

| Define | Default | Purpose |
|--------|---------|---------|
| `SBUS_NUM_CHANNELS` | 16 | Number of analog channels (protocol-fixed) |
| `SBUS_FRAME_LEN` | 25 | Frame length in bytes (protocol-fixed) |

## Protocol details

- Serial: 100000 baud, 8 data bits, even parity, 2 stop bits, inverted signal
- Frame: 25 bytes, sent every 14ms (analog) or 7ms (high-speed)
- Format: `[0x0F] [22 bytes channel data] [flags] [0x00]`
- Channels: 16 x 11-bit (0-2047), packed LSB-first little-endian
- Typical RC range: 172-1811 (maps to 1000-2000 us servo pulses)
- Flags byte 23: bit0=CH17, bit1=CH18, bit2=frame_lost, bit3=failsafe

Signal inversion is a hardware concern (use an inverter IC or configure UART polarity).
This wheel handles protocol parsing only.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-07
- [ ] Real hardware -- not yet verified

## Known issues

- Signal inversion not handled (hardware responsibility).
- Footer validation is strict (must be 0x00). Some receivers may use non-zero footer bytes
  for protocol extensions (S.Bus2) -- those frames will be rejected.
- No inter-frame gap timing detection. Consumer must configure UART idle detection if needed.

## How to test

```bash
cd wheels/protocols/sbus/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sbus
```

## Test status

- Unit tests (sim): PASS (44 C tests)
- NULL tests: PASS (4 generated tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-07
- **Key decisions**: Byte-feed parser (no UART dependency); separate decode/encode for
  static frame handling; parser stores raw buffer, caller decodes explicitly; channel-to-us
  conversion as utility (not required for basic operation); 11-bit masking on encode to
  prevent overflow; strict header/footer validation.

## Responsible human

@user
