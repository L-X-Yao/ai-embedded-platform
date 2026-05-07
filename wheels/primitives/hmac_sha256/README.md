# hmac_sha256 — HMAC-SHA256 message authentication (RFC 2104)

**Status: candidate**
**Layer: L0 (primitives)**

Keyed-hash message authentication code using SHA-256 as the underlying hash.
Streaming interface (init/update/final) plus one-shot convenience. Includes
constant-time verification to prevent timing side-channels.

## Origin

Proactive build for OTA firmware signature verification and general message
authentication. Enables `ota_manager` verify callback with HMAC-based signing.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-07):

- Many HMAC-SHA256 implementations exist but all either pull in their own SHA256
  or depend on OpenSSL/mbedTLS — too heavy for our zero-dependency constraint.
- Algorithm is 50 lines on top of our existing `sha256` wheel; vendoring would
  add more complexity than writing.

Reference: RFC 2104, RFC 4231 (test vectors).

## Dependencies

- `sha256` (L0 primitive) — streaming hash used for inner/outer passes.

## API

```c
hmac_sha256_err_t hmac_sha256_init(hmac_sha256_t *ctx,
                                   const uint8_t *key, size_t key_len);
hmac_sha256_err_t hmac_sha256_update(hmac_sha256_t *ctx,
                                     const uint8_t *data, size_t len);
hmac_sha256_err_t hmac_sha256_final(hmac_sha256_t *ctx, uint8_t mac[32]);

void hmac_sha256_compute(const uint8_t *key, size_t key_len,
                         const uint8_t *data, size_t data_len,
                         uint8_t mac[32]);
bool hmac_sha256_verify(const uint8_t a[32], const uint8_t b[32]);
```

## Configuration (compile-time)

None. No compile-time options.

## Supported platforms

- [x] Host (sim) — 2026-05-07
- [ ] ARM Cortex-M — not yet verified (pure C99, expected to work)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/hmac_sha256/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_hmac_sha256
```

## Test status

- Unit tests (sim): 17 tests (6 RFC 4231 vectors + streaming + edge cases + 7 NULL guards)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-07
- **Key decisions**: Opaque storage for inner sha256_ctx_t avoids exposing sha256.h in public header. Constant-time verify uses volatile accumulator.

## Responsible human

@user
