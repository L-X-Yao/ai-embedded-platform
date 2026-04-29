# coap_packet

**STATUS: candidate**
**Layer: L2 (protocols)**

CoAP message encoder/decoder (RFC 7252). Stateless packet-level codec for
CoAP over UDP. Supports CON/NON/ACK/RST message types, delta-encoded options,
payload marker, and token up to 8 bytes.

## Why not VSF native?

Checked VSF source (2026-04-29): no CoAP implementation in VSF.

## API

```c
#include "coap_packet.h"

coap_err_t coap_parse(const uint8_t *data, size_t len, coap_packet_t *pkt);
coap_err_t coap_build(uint8_t *buf, size_t buf_len, size_t *out_len,
                      coap_type_t type, uint8_t code, uint16_t message_id,
                      const uint8_t *token, uint8_t token_len,
                      const coap_option_t *options, int option_count,
                      const uint8_t *payload, size_t payload_len);
```

## How to test

```bash
cd wheels/protocols/coap_packet/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **10 C cases** — CON GET parse, payload parse, ACK empty, bad version,
  incomplete, build GET, build with payload, overflow, multi-option roundtrip, NULL safety

## Dependencies

None.
