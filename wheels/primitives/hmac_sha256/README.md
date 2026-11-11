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
- [ ] ARM Cortex-M — emulated only (QEMU mps2, M3, M4 and M7, 2026-11-07), no hardware yet

## Known issues

None.

## How to test

```bash
cd wheels/primitives/hmac_sha256/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_hmac_sha256
```

## Test status

- Unit tests (sim): 22 tests (RFC 4231 vectors, streaming, edge cases, NULL guards, context placement)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-07
- **Key decisions**: The inner hash is a real sha256_t, not opaque bytes, so the type has the alignment it needs; see Migration. Constant-time verify uses volatile accumulator.

## Migration

**2026-11: `hmac_sha256_t` holds a `sha256_t`.** It kept the inner hash in a
byte array, so the type had alignment 1, and a context inside a caller's struct
took a HardFault on Cortex-M.

- What changed: `hmac_sha256.h` includes `sha256.h`. The field `i_ctx_buf` is
  gone, replaced by `sha256_t inner`. The type's alignment goes from 1 to 8.
  Its size stays 168 bytes.
- What a caller has to do: put the `sha256` directory on the include path of
  every file that includes `hmac_sha256.h`. Everything already linking this
  wheel links `sha256.c`, so most builds have it.
- If they do nothing: a build missing that path stops with a clear error. A
  struct holding a context may grow, because the compiler now pads for it.

## Responsible human

@L-X-Yao
