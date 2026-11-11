# websocket_frame

**Status: candidate**
**Layer: L2 (protocols)**

WebSocket frame encoder/decoder (RFC 6455). Stateless frame-level codec —
does NOT handle HTTP upgrade handshake (use http_parser for that). Supports
masking, FIN/opcode control, 7/16/64-bit payload lengths.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). Referenced cjhdev/wic (MIT). Full WebSocket implementation is heavier than needed; our frame-only scope is correct for L2 codec. Self-written for platform convention consistency; implementation verified against RFC 6455 (FIN/opcode bits, masking, 7/16/64-bit payload length encoding).

## Dependencies

None.

## API

```c
#include "websocket_frame.h"

websocket_frame_err_t websocket_frame_parse(const uint8_t *data, size_t len, websocket_frame_t *frame);
websocket_frame_err_t websocket_frame_build(uint8_t *buf, size_t buf_len, size_t *out_len,
                        bool fin, websocket_frame_opcode_t opcode,
                        const uint8_t *mask_key,
                        const uint8_t *payload, size_t payload_len);
void     websocket_frame_mask_payload(uint8_t *data, size_t len, const uint8_t mask_key[4]);
```

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

None.

## How to test

```bash
cd wheels/protocols/websocket_frame/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **19 C cases** — unmasked/masked text parse, ping, close, 16-bit length,
  incomplete, build unmasked/masked, overflow, roundtrip, NULL safety, illegal control frames
- libFuzzer: parses raw input, builds frames in exact-size buffers and reads them back

## API convention deviations

None.

## API review

- Proposal date: 2026-04-29
- Reviewer: @L-X-Yao (batch-generated, pending review)

## Responsible human

@L-X-Yao
