# heatshrink -- lightweight LZSS compression for embedded

**Status: candidate**
**Layer: L0 (primitives)**

Streaming LZSS compression/decompression with fixed memory, no malloc,
and configurable window size (256B-2KB).  Designed for embedded systems
where OTA delta, log compaction, or data storage need compression
without heap allocation.

## Origin

Proactively built for the platform compression primitive layer (ADR-011
ecosystem-driven).  Needed for OTA delta compression, flash log
compaction, and sensor data storage on resource-constrained MCUs.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-12):

- **atomicobject/heatshrink** (ISC, 1.3k stars) -- the original
  heatshrink library.  Good reference but uses compile-time `#define`
  for window/lookahead sizes (cannot have two instances with different
  parameters), has its own test framework, and requires adaptation for
  our api-conventions (error codes, NULL guards, streaming API shape).
- **zlib/miniz/uzlib** -- full deflate implementations, too large for
  small MCUs (10-30KB code).  Overkill when only LZSS-level compression
  is needed.
- **lz4/snappy** -- fast but memory-hungry (need 8-64KB dictionary).
  Not suitable for 256B-window embedded use.

Self-written with clean streaming API, runtime-configurable parameters,
and zero heap usage.  Algorithm verified via round-trip tests on
literal, repeated, random, and streaming data.

## Dependencies

None.

## API

```c
#include "heatshrink.h"

/* Encoder */
heatshrink_err_t heatshrink_encoder_init(heatshrink_encoder_t *enc,
    uint8_t *window_buf, uint16_t window_size, uint8_t lookahead_bits);
heatshrink_err_t heatshrink_encoder_reset(heatshrink_encoder_t *enc);
heatshrink_sink_res_t heatshrink_encoder_sink(heatshrink_encoder_t *enc,
    const uint8_t *data, uint16_t len, uint16_t *consumed);
heatshrink_poll_res_t heatshrink_encoder_poll(heatshrink_encoder_t *enc,
    uint8_t *out, uint16_t out_size, uint16_t *produced);
heatshrink_finish_res_t heatshrink_encoder_finish(heatshrink_encoder_t *enc);

/* Decoder */
heatshrink_err_t heatshrink_decoder_init(heatshrink_decoder_t *dec,
    uint8_t *window_buf, uint16_t window_size, uint8_t lookahead_bits);
heatshrink_err_t heatshrink_decoder_reset(heatshrink_decoder_t *dec);
heatshrink_sink_res_t heatshrink_decoder_sink(heatshrink_decoder_t *dec,
    const uint8_t *data, uint16_t len, uint16_t *consumed);
heatshrink_poll_res_t heatshrink_decoder_poll(heatshrink_decoder_t *dec,
    uint8_t *out, uint16_t out_size, uint16_t *produced);
heatshrink_finish_res_t heatshrink_decoder_finish(heatshrink_decoder_t *dec);
```

Typical usage pattern (compress):
```c
uint8_t window[256];
heatshrink_encoder_t enc;
heatshrink_encoder_init(&enc, window, 256, 4);

/* Feed input */
uint16_t consumed;
heatshrink_encoder_sink(&enc, data, data_len, &consumed);

/* Signal end of input */
heatshrink_encoder_finish(&enc);

/* Drain output: the call that returns POLL_EMPTY can still produce bytes */
uint16_t produced;
heatshrink_poll_res_t res;
do {
    res = heatshrink_encoder_poll(&enc, out, out_cap, &produced);
    /* process produced bytes in out[] */
} while (res == HEATSHRINK_POLL_MORE);
```

## Configuration (compile-time)

None.  Window size and lookahead are runtime parameters to init().

| Parameter | Range | Purpose |
|-----------|-------|---------|
| window_size | 256, 512, 1024, 2048 | Sliding window (power-of-2) |
| lookahead_bits | 4..8 | Max match length = 2^lookahead_bits |

Decoder buffer must be `window_size + window_size/2` bytes (extra half
for input staging).

## Supported platforms

- [x] Host (sim) -- passed 2026-05-12
- [ ] ARM Cortex-M -- not yet verified

## Known issues

- **Linear search**: Match finding uses brute-force scan (O(window * lookahead)
  per byte).  Acceptable for small windows (256-2048) typical in embedded.
  For larger windows, a hash-chain or suffix-array optimization would be
  needed.
- **No cross-window matching**: When the window fills completely and new data
  is sunk, the encoder cannot reference data from before the current window
  contents.  For best compression, feed all data before polling, or use a
  larger window.

## How to test

```bash
cd wheels/primitives/heatshrink/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_heatshrink
```

## Test status

- Unit tests (sim): PASS (9 tests)
  - NULL/bad parameter rejection (encoder + decoder)
  - Empty input (finish with no data)
  - Round-trip: "Hello, World!" (short string)
  - Round-trip: 256 bytes repeated "ABAB..." (compresses to ~29 bytes)
  - Round-trip: 128 bytes pseudo-random (incompressible, expands slightly)
  - Streaming: feed/poll in small chunks (3-5 bytes)
  - Reset: encode partial, reset, encode new data, verify clean state
  - Round-trip through 1-byte output chunks (tokens resume across polls)
  - Round-trip of input larger than the window (the window restarts)
- HIL: N/A (pure compute)

## API convention deviations

None.

## Performance reference

- Compression ratio: ~8:1 on repeated data, ~1:0.9 on random data
- Window = 256 bytes: encoder needs ~48 bytes struct + 256 bytes window
- Window = 256 bytes: decoder needs ~48 bytes struct + 384 bytes buffer
- ROM footprint: ~1.5KB code

## Features

- LZSS compression and decompression (streaming)
- Configurable window size (256B-2KB) and lookahead (16-256 match length)
- Zero heap allocation -- all buffers caller-provided
- Pure C99, zero dependencies
- Bit-level I/O for compact encoding
- Reset support for buffer reuse
- Handles literal, repeated, random, and mixed data patterns

## API review

- **Date**: 2026-05-12
- **Key decisions**:
  - Runtime-configurable window/lookahead (not compile-time #define)
  - Encoder window is caller-provided flat buffer
  - Decoder needs window_size * 1.5 buffer (window + input staging)
  - Streaming: sink/poll/finish pattern (no blocking, no internal buffering)
  - Match length encoded as (length - 1), minimum match = 2 bytes
  - Back-ref offset encoded as (offset - 1), 0 = previous byte

## Responsible human

@user
