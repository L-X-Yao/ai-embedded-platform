# coap_packet

**Status: candidate**
**Layer: L2 (protocols)**

CoAP message encoder/decoder (RFC 7252). Stateless packet-level codec for
CoAP over UDP. Supports CON/NON/ACK/RST message types, delta-encoded options,
payload marker, and token up to 8 bytes.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
#include "coap_packet.h"

coap_packet_err_t coap_packet_parse(const uint8_t *data, size_t len, coap_packet_t *pkt);
coap_packet_err_t coap_packet_build(uint8_t *buf, size_t buf_len, size_t *out_len,
                      coap_packet_type_t type, uint8_t code, uint16_t message_id,
                      const uint8_t *token, uint8_t token_len,
                      const coap_packet_option_t *options, int option_count,
                      const uint8_t *payload, size_t payload_len);
```

## Configuration macros

| Macro | Default | Description |
|-------|---------|-------------|
| `COAP_MAX_OPTIONS` | 8 | Maximum number of CoAP options per packet |

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

None.

## How to test

```bash
cd wheels/protocols/coap_packet/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **10 C cases** — CON GET parse, payload parse, ACK empty, bad version,
  incomplete, build GET, build with payload, overflow, multi-option roundtrip, NULL safety

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@user
