# protobuf_lite — lightweight Protocol Buffers wire format encoder/decoder

**Status: candidate**
**Layer: L2 (protocols)**

Manual varint/zigzag/length-delimited encoding and decoding without .proto compiler
or code generation. Caller provides field numbers directly. Zero allocation, streaming
decoder, suitable for IoT telemetry payloads.
## Origin

Proactive build for IoT telemetry use cases (cloud gateways, MQTT payloads).

## Dependencies

None.

## API

```c
typedef enum {
    PB_OK           =  0,
    PB_ERR_PARAMS   = -1,
    PB_ERR_OVERFLOW = -2,
    PB_ERR_MALFORMED = -3,
    PB_ERR_TRUNCATED = -4,
} pb_err_t;

typedef enum {
    PB_WIRE_VARINT = 0,
    PB_WIRE_64BIT  = 1,
    PB_WIRE_LENGTH = 2,
    PB_WIRE_32BIT  = 5,
} pb_wire_type_t;

/* Encoder — writes to caller-provided buffer */
void     pb_encoder_init(pb_encoder_t *enc, uint8_t *buf, uint16_t capacity);
pb_err_t pb_encode_varint(pb_encoder_t *enc, uint8_t field_num, uint64_t value);
pb_err_t pb_encode_sint32(pb_encoder_t *enc, uint8_t field_num, int32_t value);
pb_err_t pb_encode_sint64(pb_encoder_t *enc, uint8_t field_num, int64_t value);
pb_err_t pb_encode_fixed32(pb_encoder_t *enc, uint8_t field_num, uint32_t value);
pb_err_t pb_encode_fixed64(pb_encoder_t *enc, uint8_t field_num, uint64_t value);
pb_err_t pb_encode_float(pb_encoder_t *enc, uint8_t field_num, float value);
pb_err_t pb_encode_bytes(pb_encoder_t *enc, uint8_t field_num,
                         const uint8_t *data, uint16_t len);
pb_err_t pb_encode_string(pb_encoder_t *enc, uint8_t field_num, const char *str);
uint16_t pb_encoder_len(const pb_encoder_t *enc);

/* Decoder — reads sequentially, returns one field at a time */
void     pb_decoder_init(pb_decoder_t *dec, const uint8_t *buf, uint16_t len);
pb_err_t pb_decode_field(pb_decoder_t *dec, pb_field_t *field);
bool     pb_decoder_done(const pb_decoder_t *dec);
int32_t  pb_zigzag_decode32(uint32_t n);
int64_t  pb_zigzag_decode64(uint64_t n);
```

## Configuration (compile-time)

No compile-time configuration. All buffer sizes are caller-provided.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/protocols/protobuf_lite/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_protobuf_lite
```

## Test status

- Unit tests (sim): 24 pass / 0 fail
- HIL: N/A

## API convention deviations

Uses `pb_` prefix instead of `protobuf_lite_` for brevity — all protobuf wire format
functions share this namespace and there is no collision risk.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Codec-only (no .proto compiler); streaming decoder (one field at a time); ZigZag exposed as separate decode helpers; float encoded as fixed32 via memcpy.

## Responsible human

@user
