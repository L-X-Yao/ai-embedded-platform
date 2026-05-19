# mavlink -- MAVLink v2 frame parser and serializer

**Status: candidate**
**Layer: L2 (protocols)**

Minimal MAVLink v2 framing layer: byte-at-a-time state machine parser,
frame serializer, CRC-16/MCRF4XX (X.25), and CRC_EXTRA lookup for
common messages. Handles the wire protocol only -- does NOT implement
the full auto-generated message set.

## Origin

Proactive build for drone/UAV communication protocols (2026-05-07).

## Why self-written?

Searched GitHub (2026-05-07). The official mavlink/c_library_v2 is
auto-generated, header-only, and pulls in the entire 300+ message
set -- far too large for constrained MCUs and violates our zero-dependency
wheel contract. This wheel implements only the framing layer (< 300 lines)
with hardcoded CRC_EXTRA for 5 common messages.

## Dependencies

None.

## API

```c
#include "mavlink.h"

/* Parser (byte-at-a-time state machine) */
mavlink_err_t         mavlink_parser_init(mavlink_parser_t *parser);
mavlink_parse_state_t mavlink_parse_byte(mavlink_parser_t *parser, uint8_t byte);
const mavlink_msg_t  *mavlink_get_msg(const mavlink_parser_t *parser);

/* Serializer */
mavlink_err_t mavlink_serialize(const mavlink_msg_t *msg,
                                uint8_t *buf, uint16_t buf_len,
                                uint16_t *out_len, uint8_t crc_extra);

/* Utilities */
uint16_t mavlink_crc16(const uint8_t *data, uint16_t len, uint16_t init);
uint8_t  mavlink_msg_crc_extra(uint32_t msgid);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `MAVLINK_MAX_PAYLOAD` | 255 | Max payload bytes per frame |

## Supported platforms

- [x] Host (sim) -- passed 2026-05-07
- [ ] Real hardware target -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/protocols/mavlink/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mavlink
```

## Test status

- Unit tests (sim): **31 C cases** -- parser init, CRC-16 known vectors,
  CRC_EXTRA lookup, serialize/parse roundtrip for all 5 common messages,
  24-bit msgid, max payload (255 bytes), garbage prefix, bad CRC, truncated
  frame, sequence wrap, auto-reset after complete/error, back-to-back
  streaming, adversarial input, mini-fuzz (2000 random bytes)
- Null tests: **6 auto-generated** (gen_null_tests.py)

## API convention deviations

None.

## Known message CRC_EXTRA values

| Message | ID | CRC_EXTRA |
|---------|----|-----------|
| HEARTBEAT | 0 | 50 |
| SYS_STATUS | 1 | 124 |
| GPS_RAW_INT | 24 | 24 |
| ATTITUDE | 30 | 39 |
| GLOBAL_POSITION_INT | 33 | 104 |

## API review

- **Date**: 2026-05-07
- **Key decisions**: Byte-at-a-time parser (not buffer-based) for
  ISR-friendly incremental feeding; CRC_EXTRA hardcoded for 5 common
  messages (returns 0 for unknown, allowing custom extension)

## Responsible human

@user
