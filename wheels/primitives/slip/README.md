# slip — SLIP (Serial Line Internet Protocol) framing encoder/decoder

**Status: candidate**
**Layer: L0 (primitives)**

RFC 1055 byte stuffing with END/ESC framing. Supports block encode/decode and a streaming decoder for byte-at-a-time UART reception. Pure C99, no malloc, no OS dependency.

## Origin

Proactive build for serial framing. SLIP is the standard framing protocol for serial links (RFC 1055), complementing the existing `cobs` wheel (COBS is block-oriented; SLIP offers a streaming decoder for ISR/DMA byte-at-a-time reception).

## Why self-written?

Searched GitHub (2026-05-02). Referenced marcinbor85/slip (MIT) for algorithm verification. marcinbor85/slip adds CRC-CCITT on top of framing; our implementation is RFC-1055-only for minimal scope. Self-written for platform convention consistency; algorithm cross-validated against reference.

## Dependencies

None.

## API

```c
typedef enum {
    SLIP_OK           =  0,
    SLIP_ERR_PARAM    = -1,
    SLIP_ERR_OVERFLOW = -2,
    SLIP_ERR_INVALID  = -3,
    SLIP_ERR_DROPPED  = -4,
} slip_err_t;

/* SLIP special bytes (RFC 1055) */
#define SLIP_END      0xC0u
#define SLIP_ESC      0xDBu
#define SLIP_ESC_END  0xDCu
#define SLIP_ESC_ESC  0xDDu

/* Block encode/decode */
slip_err_t slip_encode(const uint8_t *data, uint16_t data_len,
                       uint8_t *out, uint16_t *out_len);
slip_err_t slip_decode(const uint8_t *encoded, uint16_t enc_len,
                       uint8_t *out, uint16_t *out_len);
uint16_t slip_max_encoded_size(uint16_t data_len);

/* Streaming decoder (byte-at-a-time) */
slip_err_t slip_decoder_init(slip_decoder_t *dec, uint8_t *buf, uint16_t capacity);
slip_err_t slip_decoder_feed(slip_decoder_t *dec, uint8_t byte);
bool       slip_decoder_ready(const slip_decoder_t *dec);
uint16_t   slip_decoder_frame(const slip_decoder_t *dec, const uint8_t **data);
void       slip_decoder_reset(slip_decoder_t *dec);
```

## Configuration (compile-time)

No compile-time configuration required.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## Migration

- After `SLIP_ERR_OVERFLOW` or `SLIP_ERR_INVALID`, the streaming decoder drops
  everything up to the next END, so the tail of a broken frame no longer comes out
  as a frame. Each dropped byte returns the new `SLIP_ERR_DROPPED`.
- A byte fed while a frame waits for `slip_decoder_reset()` now returns
  `SLIP_ERR_DROPPED`, not `SLIP_OK`, and costs the whole of its frame. An END there
  returns `SLIP_OK` and costs nothing. Call reset as soon as you have read a frame.
- Reset does not end a drop, because the rest of that frame is still arriving. Use
  `slip_decoder_init()` to start afresh, after a link restart for instance.
- A caller that treats any feed error as fatal should let `SLIP_ERR_DROPPED` through.
  One that ignores the result needs no change and just stops seeing broken frames.
- `slip_decoder_t` has one more field, so rebuild whatever holds one.

## How to test

```bash
cd wheels/primitives/slip/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_slip
```

## Test status

- Unit tests (sim): 35 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: RFC 1055 SLIP framing. `uint16_t` lengths (max 64KB frames). Caller-provided buffers (no malloc). Double-END variant (leading + trailing END) for robustness. Streaming decoder for ISR/DMA byte-at-a-time reception with explicit reset between frames.

## Responsible human

@user
