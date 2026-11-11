# ubx_parser -- u-blox UBX binary GNSS protocol parser/builder

**Status: candidate**
**Layer: L2 (protocols)**

Incremental byte-at-a-time parser and frame builder for the u-blox UBX binary protocol.
Supports all UBX message classes. Handles sync detection, Fletcher-8 checksum verification,
and payload extraction. Zero-copy: parsed payload points into the caller-provided buffer.

## Origin

Proactive build for GNSS integration. UBX binary protocol provides higher precision and
lower overhead than NMEA text sentences, commonly used with u-blox GPS/GNSS receivers.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- No suitable zero-dependency C99 incremental parser found.
- Existing libraries (e.g., SparkFun u-blox GNSS) are C++/Arduino-coupled.
- Protocol is well-documented and straightforward to implement correctly.

Reference: u-blox receiver protocol specification (UBX protocol).

## Dependencies

None. Pure C99, no external dependency.

## API

```c
/* Incremental parser (byte-at-a-time) */
ubx_parser_t ctx;
uint8_t buf[300];
ubx_parser_init(&ctx, buf, sizeof(buf));

/* Feed bytes from UART ISR or DMA */
for (size_t i = 0; i < rx_len; i++) {
    ubx_feed_status_t st = ubx_parser_feed(&ctx, rx_buf[i]);
    if (st == UBX_FEED_COMPLETE) {
        ubx_msg_t msg;
        ubx_parser_get_msg(&ctx, &msg);
        handle_ubx(msg.msg_class, msg.msg_id, msg.payload, msg.payload_len);
        ubx_parser_reset(&ctx);
    } else if (st == UBX_FEED_ERROR) {
        ubx_parser_reset(&ctx);
    }
}

/* Build a UBX frame for sending */
uint8_t frame[64];
uint8_t cfg_payload[] = { /* ... */ };
int len = ubx_parser_build(frame, sizeof(frame),
                           UBX_CLASS_CFG, 0x01,
                           cfg_payload, sizeof(cfg_payload));
uart_send(frame, len);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `UBX_PARSER_MAX_PAYLOAD` | 256 | Maximum payload size hint (actual limit is buffer size) |

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] Real hardware target -- not yet verified

## Known issues

- Buffer must be at least `UBX_OVERHEAD` (8) bytes; practical use needs ~300 bytes for NAV-PVT (92 byte payload).
- Parser is not thread-safe; caller must ensure single-threaded access to the context.
- No built-in payload parsing for specific message types (NAV-PVT fields, etc.) -- that is application responsibility.

## How to test

```bash
cd wheels/protocols/ubx_parser/test
cmake -B build -S . && cmake --build build
./build/test_ubx_parser
./build/test_null_ubx_parser
```

26 tests + 11 null tests: checksum computation (known vectors, empty, null), frame building (no payload, with payload, buffer overflow, null params, ACK), incremental feed (complete, bad checksum, garbage before sync, reset/reuse, resync on double sync1, overflow protection), round-trip build-then-parse for 128 payload sizes, known byte sequence verification, mini-fuzz (2000 random bytes).

## Test status

- Unit tests (sim): PASS (26 C tests + 11 null tests)
- HIL: N/A

## API convention deviations

None.

## Protocol

UBX binary frame format:

| Field | Size | Value |
|-------|------|-------|
| Sync 1 | 1 | 0xB5 |
| Sync 2 | 1 | 0x62 |
| Class | 1 | Message class |
| ID | 1 | Message ID |
| Length | 2 (LE) | Payload length |
| Payload | N | Message data |
| CK_A | 1 | Fletcher-8 checksum byte A |
| CK_B | 1 | Fletcher-8 checksum byte B |

Checksum computed over Class + ID + Length + Payload (not sync bytes).

## API review

- **Date**: 2026-05-11
- **Key decisions**: Incremental byte-at-a-time feed (suitable for ISR/DMA byte streams); caller-provided buffer (no malloc); zero-copy payload pointer into buffer; separate checksum function exposed for building custom frames.

## Responsible human

@L-X-Yao
