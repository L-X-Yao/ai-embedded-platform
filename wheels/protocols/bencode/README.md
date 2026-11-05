# bencode — Bencode encoder/decoder (BitTorrent encoding format)

**Status: candidate**
**Layer: L2 (protocols)**

Stateless encoder/decoder for the Bencode binary format used by BitTorrent.
Supports all four types: integers (`i42e`), byte strings (`3:foo`),
lists (`l...e`), and dictionaries (`d...e`). Encoder writes tokens
sequentially to a caller-provided buffer. Decoder is an iterative
SAX-style token parser (zero-copy for strings).

## Origin

Proactive build for embedded IoT/P2P use cases requiring compact
structured data serialization.

## Why self-written?

Searched GitHub (2026-05-02). Found skeeto/bencode-c (Public domain). Trivial format with converging implementations. Self-written for unified error handling and zero-allocation guarantee.

## Dependencies

None.

## API

```c
/* Encoder */
bencode_err_t bencode_encoder_init(bencode_encoder_t *enc,
                                   uint8_t *buf, size_t cap);
bencode_err_t bencode_encode_int(bencode_encoder_t *enc, int64_t value);
bencode_err_t bencode_encode_string(bencode_encoder_t *enc,
                                    const uint8_t *data, size_t len);
bencode_err_t bencode_encode_list_begin(bencode_encoder_t *enc);
bencode_err_t bencode_encode_list_end(bencode_encoder_t *enc);
bencode_err_t bencode_encode_dict_begin(bencode_encoder_t *enc);
bencode_err_t bencode_encode_dict_end(bencode_encoder_t *enc);
size_t        bencode_encoder_len(const bencode_encoder_t *enc);

/* Decoder */
bencode_err_t bencode_decoder_init(bencode_decoder_t *dec,
                                   const uint8_t *data, size_t len);
bencode_err_t bencode_decode_next(bencode_decoder_t *dec,
                                  bencode_token_t *token);
```

## Configuration (compile-time)

`BENCODE_MAX_DEPTH` (default 16) is the deepest nesting the decoder tracks. One
level more returns `BENCODE_ERR_OVERFLOW`.

## Supported platforms

- [x] Host (sim) — 2026-05-01

## Known issues

None.

## How to test

```bash
cd wheels/protocols/bencode/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bencode
```

## Test status

- Unit tests (sim): 23 pass, 0 fail
- libFuzzer: parses raw input, builds frames in exact-size buffers and reads them back
- HIL: N/A (pure software codec)

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: SAX-style iterative decoder (no tree allocation);
  encoder uses sequential buffer writes; zero-copy string decoding.

## Responsible human

@user
