# dmx512 -- DMX512 lighting protocol data layer

**Status: candidate**
**Layer: L2 (protocols)**

Manages DMX512 universe data: channel set/get, packet building, and packet parsing.
Handles the data layer only (start code + 512 channel values). Timing (break, MAB)
is hardware-dependent and left to the driver. Caller provides a 513-byte universe buffer.

## Origin

Proactive build for lighting and LED control projects. DMX512 is the standard protocol
for stage lighting, architectural lighting, and LED installations.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- Existing DMX512 libraries are typically Arduino-specific or coupled to UART hardware.
- The data layer is trivial but benefits from bounds checking and consistent API.
- No suitable zero-dependency C99 data-layer-only implementation found.

Reference: ANSI E1.11 (DMX512-A).

## Dependencies

None. Pure C99, no external dependency.

## API

```c
uint8_t universe[DMX512_PACKET_SIZE]; /* 513 bytes */
dmx512_t ctx;
dmx512_init(&ctx, universe, 512);

/* Set individual channels (1-based) */
dmx512_set_channel(&ctx, 1, 255);   /* Channel 1 = full */
dmx512_set_channel(&ctx, 2, 128);   /* Channel 2 = half */

/* Set range of channels */
uint8_t rgb[] = {255, 0, 128};
dmx512_set_range(&ctx, 10, rgb, 3); /* Channels 10-12 */

/* Get packet for UART transmission */
const uint8_t *pkt;
size_t pkt_len;
dmx512_get_packet(&ctx, &pkt, &pkt_len);
uart_send_dmx(pkt, pkt_len); /* Send with break+MAB timing */

/* Parse received packet */
dmx512_parse_packet(&ctx, rx_data, rx_len);
uint8_t val;
dmx512_get_channel(&ctx, 1, &val);
```

## Configuration (compile-time)

No compile-time configuration needed.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] Real hardware target -- not yet verified

## Known issues

- Channel numbering is 1-based (DMX convention). Channel 0 is invalid.
- Only standard start code (0x00) is supported. RDM (start code 0xCC) is not handled.
- parse_packet with fewer channels than configured zeros out remaining channels.
- Universe buffer must be caller-provided and sized at least (num_channels + 1) bytes.

## How to test

```bash
cd wheels/protocols/dmx512/test
cmake -B build -S . && cmake --build build
./build/test_dmx512
./build/test_null_dmx512
```

30 tests + 12 null tests: init (basic, small universe, null, zero/max channels), channel set/get (basic, zero, out of range, null), set_range (basic, at end, overflow, null, zero count, channel zero), get_packet (basic, small universe, null), parse_packet (full, short data, bad start code, start code only, null, too short, excess data), round-trip (tx to rx), all-channels full range, error codes.

## Test status

- Unit tests (sim): PASS (30 C tests + 12 null tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-11
- **Key decisions**: 1-based channel numbering (matches DMX convention); caller-provided buffer (no malloc); configurable channel count (1-512); parse_packet zeros channels beyond received data; get_packet returns pointer to internal buffer (zero-copy).

## Responsible human

@user

## Protocol

DMX512 packet format:

| Field | Size | Value |
|-------|------|-------|
| Start Code | 1 | 0x00 (standard DMX) |
| Channel 1 | 1 | 0-255 |
| Channel 2 | 1 | 0-255 |
| ... | | |
| Channel N | 1 | 0-255 (N <= 512) |

Physical layer: break (>= 88us low) + MAB (>= 8us high) precede the start code.
Electrical: RS-485, 250 kbaud, 8N2.
