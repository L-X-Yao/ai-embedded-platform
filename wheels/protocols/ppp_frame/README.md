# ppp_frame -- PPP HDLC-like framing codec (RFC 1662)

**Status: candidate**
**Layer: L2 (protocols)**

Encodes and decodes PPP frames with HDLC byte-stuffing and FCS-16 (CRC-16/HDLC).
Handles the data-link layer framing: flag sequence, address/control, protocol field,
byte stuffing of control characters, and frame check sequence. No state machine for
PPP negotiation -- this is purely the framing codec.

## Origin

Proactive build for embedded cellular/modem interfaces. PPP framing is used by AT-command
modems, serial links, and some IoT protocols.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- lwIP includes PPP but it is tightly coupled to the lwIP stack.
- No standalone zero-dependency C99 PPP framing codec found.
- RFC 1662 framing is well-specified and compact to implement.

Reference: RFC 1662 (PPP in HDLC-like Framing).

## Dependencies

None. Pure C99, no external dependency.

## API

```c
/* Encode */
uint8_t payload[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
uint8_t frame[128];
size_t frame_len;
ppp_frame_encode(0x0021, payload, sizeof(payload),
                 frame, sizeof(frame), &frame_len);
/* frame contains: flag + stuffed(addr+ctrl+proto+payload) + stuffed(FCS) + flag */

/* Decode */
uint16_t protocol;
uint8_t decoded[128];
size_t decoded_len;
ppp_frame_err_t err = ppp_frame_decode(frame, frame_len,
                                        &protocol, decoded, sizeof(decoded),
                                        &decoded_len);
/* protocol = 0x0021 (IP), decoded = "Hello", decoded_len = 5 */

/* Standalone FCS */
uint16_t fcs = ppp_frame_fcs16(data, len);
```

## Configuration (compile-time)

No compile-time configuration needed.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] Real hardware target -- not yet verified

## Known issues

- Decode uses a 1500-byte stack buffer for de-stuffing. On very constrained targets,
  this may need to be reduced or made configurable.
- Only supports standard HDLC framing with address=0xFF and control=0x03.
- Address/control field compression (RFC 1661) is not implemented.
- ACCM (async control character map) is not configurable -- all bytes < 0x20 are escaped.

## How to test

```bash
cd wheels/protocols/ppp_frame/test
cmake -B build -S . && cmake --build build
./build/test_ppp_frame
./build/test_null_ppp_frame
```

23 tests + 8 null tests: FCS-16 (RFC test vector "123456789" = 0x906E, empty, null, single byte), encode (empty payload, simple, flag/escape bytes, control chars, overflow, null), decode (null, too short, missing flags), round-trip (empty, simple, with escapes, all protocol types, 256-byte payload), decode errors (bad FCS, payload overflow, trailing escape, embedded flag).

## Test status

- Unit tests (sim): PASS (23 C tests + 8 null tests)
- HIL: N/A

## API convention deviations

None.

## Protocol

PPP HDLC-like frame format (RFC 1662):

| Field | Size | Value |
|-------|------|-------|
| Flag | 1 | 0x7E |
| Address | 1 | 0xFF |
| Control | 1 | 0x03 |
| Protocol | 2 | e.g., 0x0021 (IP), 0xC021 (LCP) |
| Payload | N | Information field |
| FCS | 2 | CRC-16/HDLC (little-endian) |
| Flag | 1 | 0x7E |

Byte stuffing: 0x7E -> 0x7D 0x5E, 0x7D -> 0x7D 0x5D, byte < 0x20 -> 0x7D (byte ^ 0x20).

## API review

- **Date**: 2026-05-11
- **Key decisions**: Encode produces complete frame (with flags); decode expects complete frame (with flags); HDLC byte stuffing per RFC 1662 (escape 0x7E, 0x7D, and all < 0x20); FCS-16 with standard HDLC polynomial (0x8408); FCS appended little-endian.

## Responsible human

@user
