# aes128

**Status: candidate**
**Layer: L0 (primitives)**

AES-128 ECB block encrypt/decrypt per FIPS 197.  Single 16-byte block
primitive -- higher modes (CBC, CTR, GCM) are composed by the caller.

## Origin

Proactively built for the platform crypto primitive layer (ADR-011
ecosystem-driven).  Needed for OTA signature verification, secure
config storage, and any future authenticated encryption needs.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-04):

- **kokke/tiny-AES-c** (MIT, 3.9k stars) -- good reference implementation,
  but mixes ECB/CBC/CTR in one file with global-mode `#define` guards.
  Our wheel needs only ECB (modes are caller's job) and must follow
  platform api-conventions (error codes, NULL guards, file headers).
- **BearSSL / wolfSSL / mbedTLS** -- full TLS stacks, overkill for a
  single-block primitive on resource-constrained MCUs.

Reference: FIPS 197 (2001) + tiny-AES-c algorithm structure verified
against NIST test vectors.

## Dependencies

None.

## API

```c
#include "aes128.h"

typedef struct { uint8_t round_keys[176]; } aes128_t;

aes128_err_t aes128_init(aes128_t *ctx, const uint8_t key[16]);
aes128_err_t aes128_encrypt_block(const aes128_t *ctx,
                                  const uint8_t in[16], uint8_t out[16]);
aes128_err_t aes128_decrypt_block(const aes128_t *ctx,
                                  const uint8_t in[16], uint8_t out[16]);
```

## Configuration (compile-time)

None.  AES-128 is fixed at 10 rounds / 128-bit key.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-04
- [ ] ARM Cortex-M -- not yet verified

## Known issues

- **S-Box timing characteristic**: The SubBytes step uses a 256-byte lookup table
  (`sbox[]`).  On targets with data-dependent cache behaviour (e.g., Cortex-M7
  with D-cache enabled), cache-timing side-channel attacks are theoretically
  possible.  For threat models requiring full timing isolation, use a bitsliced
  or T-table AES implementation.  Accepted for this wheel (embedded targets
  typically run without OS-level shared-cache adversaries).

## How to test

```bash
cd wheels/primitives/aes128/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_aes128
./build/prop_aes128
```

## Test status

- Unit tests (sim): PASS (10 tests, 20+ assertions)
- Property tests (sim): PASS (3 tests, 200 random trials)
- Test vectors:
  - FIPS 197 Appendix B (encrypt + decrypt)
  - NIST SP 800-38A F.1.1 ECB blocks 1-2 (encrypt + decrypt)
  - Zero key + zero plaintext known answer
  - NULL parameter rejection
  - In-place encrypt/decrypt (in == out aliasing)
  - Different keys produce different ciphertexts
- Property tests:
  - Round-trip: encrypt(decrypt(x)) == x for 100 random key/plaintext pairs
  - Determinism: same input always produces same output (50 trials)
  - Avalanche: 1-bit plaintext change alters ciphertext (50 trials)
- HIL: N/A (pure compute)

## API convention deviations

None.

## API review

- **Date**: 2026-05-04
- **Key decisions**:
  - ECB only (single block) -- modes are caller responsibility
  - ctx stores expanded round keys (176 bytes), not original key
  - encrypt/decrypt take separate in/out but allow aliasing (in == out)

## Responsible human

@user

## Features

- AES-128 ECB encrypt and decrypt (FIPS 197 compliant)
- Byte-oriented (no alignment assumptions, portable to 8/16/32-bit MCUs)
- Pure C99, zero dependencies
- Fully reentrant after init (encrypt/decrypt are read-only on ctx)
- In-place operation supported (in == out)

## Performance reference

- Key expansion: 10 rounds, ~44 word operations
- Encrypt/decrypt: 10 rounds per 16-byte block
- ROM footprint: ~1.0 KB (S-Box 256 + inv S-Box 256 + code ~500 bytes)
- RAM: 176 bytes for round keys + 16 bytes stack (state)
