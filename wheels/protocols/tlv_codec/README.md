# tlv_codec — Tag-Length-Value encoder/decoder for embedded protocols

**Status: candidate**
**Layer: L2 (protocols)**

Fixed-width TLV codec: 2-byte tag (big-endian) + 2-byte length (big-endian) + N-byte value.
Typed encode helpers for u8/u16/u32/string, iterative zero-copy decoder.
Common in custom embedded protocols, smart cards, NFC.

## Origin

Proactive build for custom embedded protocol framing (telemetry, config, NFC NDEF-like payloads).

## Why self-written?

Searched GitHub (2026-05-02). Found Carrigan/stlv (varies license). TLV is a trivial format with converging implementations. Self-written for unified error handling and zero-allocation guarantee.

## Dependencies

None.

## API

```c
/* Error codes */
typedef enum {
    TLV_OK, TLV_ERR_PARAMS, TLV_ERR_OVERFLOW,
    TLV_ERR_TRUNCATED, TLV_ERR_INVALID,
} tlv_err_t;

/* Decoded element — value points into decode buffer (zero-copy) */
typedef struct { uint16_t tag; uint16_t length; const uint8_t *value; } tlv_t;

/* Encoder */
void      tlv_encoder_init(tlv_encoder_t *enc, uint8_t *buf, uint16_t capacity);
tlv_err_t tlv_encode(tlv_encoder_t *enc, uint16_t tag, const uint8_t *value, uint16_t length);
tlv_err_t tlv_encode_u8(tlv_encoder_t *enc, uint16_t tag, uint8_t value);
tlv_err_t tlv_encode_u16(tlv_encoder_t *enc, uint16_t tag, uint16_t value);
tlv_err_t tlv_encode_u32(tlv_encoder_t *enc, uint16_t tag, uint32_t value);
tlv_err_t tlv_encode_string(tlv_encoder_t *enc, uint16_t tag, const char *str);
uint16_t  tlv_encoder_len(const tlv_encoder_t *enc);

/* Decoder (iterative) */
void      tlv_decoder_init(tlv_decoder_t *dec, const uint8_t *buf, uint16_t len);
tlv_err_t tlv_decode_next(tlv_decoder_t *dec, tlv_t *tlv);
bool      tlv_decoder_done(const tlv_decoder_t *dec);

/* Value extraction helpers */
uint8_t   tlv_get_u8(const tlv_t *tlv);
uint16_t  tlv_get_u16(const tlv_t *tlv);
uint32_t  tlv_get_u32(const tlv_t *tlv);
```

## Configuration (compile-time)

No compile-time configuration needed.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/protocols/tlv_codec/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_tlv_codec
```

## Test status

- Unit tests (sim): 17 pass, 0 fail
- libFuzzer: parses raw input, builds frames in exact-size buffers and reads them back
- HIL: N/A

## API convention deviations

None.

## Wire format

```
+-------+-------+--------+--------+--- ... ---+
| tag_h | tag_l | len_h  | len_l  |   value   |
+-------+-------+--------+--------+--- ... ---+
   1B      1B      1B       1B      len bytes
```

All multi-byte fields are big-endian. Empty value (length=0) is valid.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Fixed 2+2 header (not BER variable-length); big-endian to match network byte order; zero-copy decode (value pointers into input buffer).

## Responsible human

@L-X-Yao
