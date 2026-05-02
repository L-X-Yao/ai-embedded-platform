# hash_fnv — FNV-1a hash function (32-bit and 64-bit)

**Status: candidate**
**Layer: L0 (primitives)**

Simple, fast, no-table FNV-1a hash for hash maps, checksums, and string
hashing. Provides both 32-bit and 64-bit variants with one-shot, string,
and incremental (streaming) APIs.
## Origin

Proactive build for hash map key hashing, string deduplication, and fast
non-cryptographic checksums across wheels and projects.


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
#include "hash_fnv.h"

/* Constants */
#define FNV1A_32_OFFSET_BASIS  0x811C9DC5u
#define FNV1A_32_PRIME         0x01000193u
#define FNV1A_64_OFFSET_BASIS  0xCBF29CE484222325ULL
#define FNV1A_64_PRIME         0x00000100000001B3ULL

/* One-shot bulk computation */
uint32_t fnv1a_32(const uint8_t *data, size_t len);
uint64_t fnv1a_64(const uint8_t *data, size_t len);

/* NUL-terminated string convenience */
uint32_t fnv1a_32_string(const char *str);
uint64_t fnv1a_64_string(const char *str);

/* Incremental / streaming */
uint32_t fnv1a_32_update(uint32_t hash, const uint8_t *data, size_t len);
uint64_t fnv1a_64_update(uint64_t hash, const uint8_t *data, size_t len);
```

## Configuration (compile-time)

None. No configurable defines.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] ARM Cortex-M — not yet verified (pure C99, expected trivial)

## Known issues

None. Pure stateless computation.

## How to test

```bash
cd wheels/primitives/hash_fnv/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_hash_fnv
```

## Test status

- Unit tests (sim): 15 tests, all passing
- Test vectors: canonical FNV-1a values for "", "a", "foobar", single bytes
- Streaming equivalence: segmented input matches one-shot
- HIL: N/A (pure compute)

## Performance reference

- Time complexity: O(n), 1 XOR + 1 multiply per byte
- ROM footprint: ~120 bytes code (no lookup table)
- Stack usage: < 16 bytes
- Suitable for MCUs with no hardware multiplier (multiply is software but
  still fast for typical key lengths)

## API convention deviations

None. Function naming `fnv1a_<width>[_verb]` follows api-conventions.md.

## API review

- **Date**: 2026-05-01
- **Key decisions**: 32-bit and 64-bit variants; update API for streaming;
  string convenience wrappers; no error codes (pure functions always succeed)

## Responsible human

@user
