# cbor_lite — lightweight CBOR encoder/decoder (RFC 7049)

**Status: candidate**
**Layer: L2 (protocols)**

Minimal CBOR codec for embedded systems. Covers major types 0-7 (unsigned int, negative int, byte string, text string, array, map, tag, simple/float). Pure C99, no heap, no OS deps. Used in CoAP payloads, COSE, FIDO2, and general binary serialization.

## Origin

Proactive build for IoT protocol stacks (CoAP payload encoding, sensor telemetry serialization).


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
/* Encoder */
void            cbor_lite_enc_init(cbor_lite_encoder_t *enc, uint8_t *buf, size_t cap);
size_t          cbor_lite_enc_len(const cbor_lite_encoder_t *enc);
cbor_lite_err_t cbor_lite_enc_uint(cbor_lite_encoder_t *enc, uint64_t val);
cbor_lite_err_t cbor_lite_enc_nint(cbor_lite_encoder_t *enc, uint64_t val);
cbor_lite_err_t cbor_lite_enc_bstr(cbor_lite_encoder_t *enc, const uint8_t *data, size_t len);
cbor_lite_err_t cbor_lite_enc_tstr(cbor_lite_encoder_t *enc, const char *str, size_t len);
cbor_lite_err_t cbor_lite_enc_array(cbor_lite_encoder_t *enc, size_t count);
cbor_lite_err_t cbor_lite_enc_map(cbor_lite_encoder_t *enc, size_t count);
cbor_lite_err_t cbor_lite_enc_bool(cbor_lite_encoder_t *enc, bool val);
cbor_lite_err_t cbor_lite_enc_null(cbor_lite_encoder_t *enc);
cbor_lite_err_t cbor_lite_enc_float(cbor_lite_encoder_t *enc, float val);

/* Decoder */
void            cbor_lite_dec_init(cbor_lite_decoder_t *dec, const uint8_t *buf, size_t len);
cbor_lite_err_t cbor_lite_dec_next(cbor_lite_decoder_t *dec, cbor_lite_item_t *item);
size_t          cbor_lite_dec_offset(const cbor_lite_decoder_t *dec);
```

## Configuration (compile-time)

None. All buffers are caller-provided.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

- Indefinite-length encoding (major type + additional info 31) is not supported.
- Only float32 is supported (no float16 or float64).

## How to test

```bash
cd wheels/protocols/cbor_lite/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_cbor_lite
```

## Test status

- Unit tests (sim): 19 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Encoder/decoder split with streaming-style API. nint encodes raw value (caller computes -1-val). Float limited to float32 for embedded simplicity.

## Responsible human

@user
