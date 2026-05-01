# cobs — Consistent Overhead Byte Stuffing encoder/decoder

**Status: candidate**
**Layer: L0 (primitives)**

Zero-allocation byte stuffing that eliminates 0x00 from payload, enabling reliable frame delimiting on serial links. Overhead is at most 1 byte per 254 bytes of input. Pure C99, no malloc, no OS dependency.

## Origin

Proactive build for serial framing (UART, RS-485, etc.). COBS is the standard byte-stuffing algorithm for packet-based serial protocols.

## Why not VSF native?

VSF does not provide a COBS codec. `vsf/source/service/` and `vsf/source/utilities/` have no byte-stuffing or COBS implementation.

## Dependencies

None.

## API

```c
typedef enum {
    COBS_OK           =  0,
    COBS_ERR_PARAMS   = -1,
    COBS_ERR_OVERFLOW = -2,
    COBS_ERR_INVALID  = -3,
} cobs_err_t;

/* Encode data with COBS. out_len: in=capacity, out=written. */
cobs_err_t cobs_encode(const uint8_t *data, uint16_t data_len,
                       uint8_t *out, uint16_t *out_len);

/* Decode COBS-encoded data. out_len: in=capacity, out=written. */
cobs_err_t cobs_decode(const uint8_t *encoded, uint16_t enc_len,
                       uint8_t *out, uint16_t *out_len);

/* Max encoded size for a given input length. */
uint16_t cobs_max_encoded_size(uint16_t data_len);
```

## Configuration (compile-time)

No compile-time configuration required.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/cobs/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_cobs
```

## Test status

- Unit tests (sim): 18 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Standard COBS algorithm. `uint16_t` lengths (max 64KB frames, sufficient for embedded serial). Caller-provided buffers (no malloc). Error enum with params/overflow/invalid categories.

## Responsible human

@user
