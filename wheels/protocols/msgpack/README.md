# msgpack — MessagePack binary serialization encoder/decoder

**Status: candidate**
**Layer: L2 (protocols)**

Compact binary JSON alternative for IoT telemetry and configuration payloads.
Streaming encoder and token-based decoder. Supports fixint/fixstr/fixarray/fixmap
plus 8/16/32-bit variants. Pure C99, no heap, all buffers caller-provided.
## Origin

Proactive build for IoT telemetry serialization (sensor data, config exchange).


## Why self-written?

Searched GitHub (2026-05-02). Referenced ludocode/mpack (MIT). Gold standard MessagePack library for C. Self-written for platform convention consistency; implementation verified against MessagePack specification (format bytes, fixint/fixstr/fixarray/fixmap encoding).

## Dependencies

None.

## API

```c
/* Encoder */
msgpack_err_t msgpack_enc_init(msgpack_enc_t *enc, uint8_t *buf, size_t cap);
size_t        msgpack_enc_len(const msgpack_enc_t *enc);
msgpack_err_t msgpack_enc_nil(msgpack_enc_t *enc);
msgpack_err_t msgpack_enc_bool(msgpack_enc_t *enc, bool val);
msgpack_err_t msgpack_enc_uint(msgpack_enc_t *enc, uint32_t val);
msgpack_err_t msgpack_enc_int(msgpack_enc_t *enc, int32_t val);
msgpack_err_t msgpack_enc_float(msgpack_enc_t *enc, float val);
msgpack_err_t msgpack_enc_str(msgpack_enc_t *enc, const char *str, size_t len);
msgpack_err_t msgpack_enc_bin(msgpack_enc_t *enc, const uint8_t *data, size_t len);
msgpack_err_t msgpack_enc_array(msgpack_enc_t *enc, uint32_t count);
msgpack_err_t msgpack_enc_map(msgpack_enc_t *enc, uint32_t count);

/* Decoder */
msgpack_err_t msgpack_dec_init(msgpack_dec_t *dec, const uint8_t *buf, size_t len);
msgpack_err_t msgpack_dec_next(msgpack_dec_t *dec, msgpack_token_t *tok);
```

## Configuration (compile-time)

None. No compile-time knobs required.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

- 64-bit integers and float64 intentionally omitted (MCU payloads rarely need them).
- ext type family (0xc7-0xc9, 0xd4-0xd8) not supported; decoder returns ERR_FORMAT.

## How to test

```bash
cd wheels/protocols/msgpack/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_msgpack
```

## Test status

- Unit tests (sim): 59 passed, 0 failed
- libFuzzer: parses raw input, builds frames in exact-size buffers and reads them back
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Streaming encoder (init + pack_* + len) and token-based decoder (init + next). No 64-bit or ext support to keep footprint minimal.

## Responsible human

@user
