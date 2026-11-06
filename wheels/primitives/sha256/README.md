# sha256

**Status: candidate**
**Layer: L0 (primitives)**

SHA-256 cryptographic hash per FIPS 180-4.  Streaming interface
(init/update/final) plus one-shot convenience function.

## Origin

Proactively built for the platform crypto primitive layer (ADR-011
ecosystem-driven).  Needed for firmware integrity checks, OTA image
verification, HMAC construction, and certificate fingerprinting.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-04):

- **B-Con/crypto-algorithms** (public domain) -- minimal reference
  implementation, but no streaming API and no error handling.
- **ogay/sha256** -- single file, but non-standard API and no maintained
  test suite.
- **mbedTLS / wolfSSL / BearSSL** -- full TLS stacks with SHA-256
  inside, overkill for a standalone hash primitive.

Reference: FIPS 180-4 (2015) algorithm, NIST CSRC test vectors.
Self-written for platform convention consistency (error codes, NULL
guards, streaming API, file headers).

## Dependencies

None.

## API

```c
#include "sha256.h"

typedef struct {
    uint32_t state[8];
    uint8_t  buffer[64];
    uint64_t count;
} sha256_t;

sha256_err_t sha256_init(sha256_t *ctx);
sha256_err_t sha256_update(sha256_t *ctx, const uint8_t *data, size_t len);
sha256_err_t sha256_final(sha256_t *ctx, uint8_t hash[32]);
void sha256_hash(const uint8_t *data, size_t len, uint8_t hash[32]);
```

## Configuration (compile-time)

None.  SHA-256 is a fixed algorithm (64 rounds, 256-bit digest).

## Supported platforms

- [x] Host (sim) -- passed 2026-05-04
- [ ] ARM Cortex-M -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/sha256/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sha256
./build/prop_sha256
```

## Test status

- Unit tests (sim): PASS (12 tests, 15+ assertions)
- Property tests (sim): PASS (4 tests, 300 random trials)
- Test vectors:
  - NIST "abc" (3 bytes)
  - Empty string (0 bytes)
  - "abcdbcdecdefdefg..." (448-bit, two-block message)
  - 1,000,000 x 'a' (long message stress test)
  - "The quick brown fox..." known answer
  - Streaming byte-by-byte == one-shot
  - Chunked streaming (7-byte chunks) == one-shot
  - NULL parameter rejection, including NULL data with a non-zero length, for which `sha256_hash` gives all zeros
  - Zero-length update is harmless
- Property tests:
  - Determinism: same input always produces same hash (100 trials)
  - Collision resistance: 1-bit flip changes hash (100 trials)
  - Length sensitivity: prefix vs full message differ (50 trials)
  - Streaming chunking: random chunk sizes match one-shot (50 trials)
- HIL: N/A (pure compute)

## API convention deviations

None.

## Performance reference

- Compression: 64 rounds per 512-bit (64-byte) block
- ROM footprint: ~1.3 KB (K table 256 bytes + code ~1 KB)
- RAM: 108 bytes context (sha256_t) + ~320 bytes stack (W[64] schedule)

## Features

- SHA-256 hash (FIPS 180-4 compliant)
- Streaming and one-shot API
- Byte-oriented (no alignment assumptions, portable to 8/16/32-bit MCUs)
- Pure C99, zero dependencies
- Context wiped after sha256_final (basic side-channel hygiene)
- Handles messages up to 2^64 - 1 bits

## API review

- **Date**: 2026-05-04
- **Key decisions**:
  - Streaming (init/update/final) as primary interface for large data
  - One-shot `sha256_hash()` convenience for common case
  - Context wiped to zero after final (defense in depth)
  - No HMAC in this wheel (HMAC is a separate L0 or L2 wheel)

## Responsible human

@user
