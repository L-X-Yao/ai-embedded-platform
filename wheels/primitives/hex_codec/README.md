# hex_codec — hex string encoder/decoder

**Status: candidate**
**Layer: L0 (primitives)**

Encodes binary data to hex strings (lower/uppercase) and decodes hex strings
back to binary. Complement to base64 for human-readable binary representation.
Common for debug output, MAC addresses, register dumps.

## Origin

Proactive build for debug/diagnostic use cases across projects.

## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
typedef enum {
    HEX_OK             =  0,
    HEX_ERR_PARAMS     = -1,
    HEX_ERR_OVERFLOW   = -2,
    HEX_ERR_INVALID    = -3,
    HEX_ERR_ODD_LENGTH = -4,
} hex_err_t;

/* Encode binary to lowercase hex string */
hex_err_t hex_encode(const uint8_t *data, uint16_t data_len,
                     char *out, uint16_t out_size);

/* Encode binary to uppercase hex string */
hex_err_t hex_encode_upper(const uint8_t *data, uint16_t data_len,
                           char *out, uint16_t out_size);

/* Decode hex string to binary (mixed case accepted) */
hex_err_t hex_decode(const char *hex, uint16_t hex_len,
                     uint8_t *out, uint16_t *out_len);

/* Size calculations */
uint16_t hex_encoded_size(uint16_t data_len);   /* data_len * 2 */
uint16_t hex_decoded_size(uint16_t hex_len);     /* hex_len / 2  */

/* Single-byte helpers */
char hex_nibble_to_char(uint8_t nibble, bool uppercase);
int  hex_char_to_nibble(char c);  /* 0-15 or -1 */
```

## Configuration (compile-time)

None. No compile-time defines needed.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/hex_codec/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_hex_codec
```

## Test status

- Unit tests (sim): 16 PASS, 0 FAIL
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Separate lower/upper encode functions rather than a
  flag parameter for clarity. `hex_decode` accepts mixed case. Error enum
  uses `HEX_` prefix (not `HEX_CODEC_`) for brevity at call sites.

## Responsible human

@user
