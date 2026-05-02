# websocket_frame

**Status: candidate**
**Layer: L2 (protocols)**

WebSocket frame encoder/decoder (RFC 6455). Stateless frame-level codec —
does NOT handle HTTP upgrade handshake (use http_parser for that). Supports
masking, FIN/opcode control, 7/16/64-bit payload lengths.
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

## How to test

```bash
cd wheels/protocols/websocket_frame/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **11 C cases** — unmasked/masked text parse, ping, close, 16-bit length,
  incomplete, build unmasked/masked, overflow, roundtrip, NULL safety

## Dependencies

None.

## Origin

Proactive build for common embedded components (2026-04-29 batch).


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Known issues

None.

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## API convention deviations

None.

## Responsible human

@user
