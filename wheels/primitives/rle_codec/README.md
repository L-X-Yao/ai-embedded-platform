# rle_codec — run-length encoding / decoding

**Status: candidate**
**Layer: L0 (primitives)**

Simple RLE codec: encodes byte sequences as [count][value] pairs.
Count is uint8_t (1..255). Runs longer than 255 are split into
multiple pairs. Useful for compressing sensor data, bitmaps, or
any data with long runs of repeated bytes.

## Origin

Proactive build for data compression in resource-constrained
environments (sensor data logging, display bitmaps, flash storage).

## Why self-written?

Trivial algorithm. No established embedded RLE library worth vendoring
for 60 lines of code. Self-written for platform convention consistency.

## Dependencies

None.

## API

```c
rle_codec_err_t rle_codec_encode(const uint8_t *src, size_t src_len,
                                 uint8_t *dst, size_t dst_cap, size_t *out_len);
rle_codec_err_t rle_codec_decode(const uint8_t *src, size_t src_len,
                                 uint8_t *dst, size_t dst_cap, size_t *out_len);
size_t          rle_codec_encode_bound(size_t src_len);
size_t          rle_codec_decode_bound(size_t src_len);
```

- `encode()` / `decode()` return error codes; actual length via `*out_len`
- `encode_bound()` / `decode_bound()` compute worst-case output sizes
- Format: `[count][value]` pairs, count in 1..255
- Runs > 255 automatically split into multiple pairs
- No malloc -- pure stateless functions

## Configuration (compile-time)

None. No compile-time options.

## Supported platforms

- [x] Host (sim) -- 2026-05-11
- [ ] Real hardware target -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/rle_codec/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_rle_codec
```

## Test status

- Unit tests (sim): 34 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-11
- **Key decisions**: Stateless pure functions (no context struct needed).
  [count][value] pair format for simplicity. uint8_t count limits
  max run to 255 (auto-split for longer runs). ERR_OVERFLOW returned
  when output buffer is too small. Odd-length encoded input rejected
  as ERR_PARAM.

## Responsible human

@user
