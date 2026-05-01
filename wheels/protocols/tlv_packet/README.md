# tlv_packet — Type-Length-Value encoder/decoder

**Status: candidate**
**Layer: L2 (protocols)**

Variable-size TLV codec for smart cards, NFC, BLE, and embedded protocols.
Supports 1-byte or 2-byte tags and lengths, zero-copy decode, iterative
decoding, and tag search. Pure C99, no heap.

## Origin

Proactive build for embedded protocol framing (smart card APDUs, NFC NDEF,
BLE GATT attributes, custom binary protocols).

## Why not VSF native?

VSF has no general-purpose TLV codec. Its BLE/NFC stacks handle TLV
internally but do not expose a reusable encoder/decoder API.

## Dependencies

None.

## API

```c
typedef enum {
    TLV_OK            =  0,
    TLV_ERR_PARAMS    = -1,
    TLV_ERR_OVERFLOW  = -2,
    TLV_ERR_TRUNCATED = -3,
    TLV_ERR_FORMAT    = -4,
} tlv_err_t;

typedef struct {
    uint16_t       tag;
    const uint8_t *value;   /* zero-copy pointer into input buffer */
    uint16_t       length;
} tlv_t;

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

/* Search */
tlv_err_t tlv_find(const uint8_t *buf, uint16_t len, uint16_t tag, tlv_t *result);
```

## Wire format

- **Tag**: 1 byte if < 0x80; 2 bytes if >= 0x80 (bit 7 set = 2-byte tag, `tag = ((b0 & 0x7F) << 8) | b1`)
- **Length**: 1 byte if < 0x80; 2 bytes if >= 0x80 (same scheme as tag)
- **Value**: `length` raw bytes

## Configuration (compile-time)

None. All buffer sizes are caller-provided.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified (pure C99, no platform deps)

## Known issues

None.

## How to test

```bash
cd wheels/protocols/tlv_packet/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_tlv_packet
```

## Test status

- Unit tests (sim): 19 PASS, 0 FAIL
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Variable tag/length size (bit-7 scheme); zero-copy decode; big-endian integer encoding; `TLV_ERR_FORMAT` for tag-not-found in `tlv_find`

## Responsible human

@user
