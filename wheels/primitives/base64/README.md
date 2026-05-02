# base64 — Base64 encoder/decoder for embedded systems

**Status: candidate**
**Layer: L0 (primitives)**

RFC 4648 compliant Base64 encoder/decoder. Zero allocation -- caller provides
all output buffers. Handles both padded and unpadded input on decode.

## Origin

Proactive build for embedded protocols (MQTT auth, HTTP headers, OTA payloads).

## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
typedef enum {
    BASE64_OK           =  0,
    BASE64_ERR_PARAMS   = -1,   /* NULL pointer or invalid argument */
    BASE64_ERR_OVERFLOW = -2,   /* output buffer too small          */
    BASE64_ERR_INVALID  = -3,   /* invalid Base64 character         */
} base64_err_t;

base64_err_t base64_encode(const uint8_t *data, uint16_t data_len,
                           char *out, uint16_t *out_len);

base64_err_t base64_decode(const char *b64, uint16_t b64_len,
                           uint8_t *out, uint16_t *out_len);

uint16_t base64_encoded_size(uint16_t data_len);
uint16_t base64_decoded_size(uint16_t b64_len);
```

## Configuration (compile-time)

None.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/base64/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_base64
```

## Test status

- Unit tests (sim): 17 pass (5 encode + 6 decode + binary roundtrip + 4 error + size calc)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: uint16_t lengths (sufficient for embedded payloads), standard alphabet only (no URL-safe variant)

## Responsible human

@user
