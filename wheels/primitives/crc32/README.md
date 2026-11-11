# crc32

**Status: candidate**
**Layer: L0 (primitives)**

CRC32 (IEEE 802.3 / Ethernet / zlib-compatible) table-based computation. Drop-in
for OTA image checksums, file integrity, Ethernet FCS, and any CRC32 with
poly 0xEDB88320 / init 0xFFFFFFFF / final XOR 0xFFFFFFFF.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). Found lammertb/libcrc (MIT). Standard textbook CRC32 IEEE 802.3 algorithm. Self-written for platform convention consistency (unified error codes, HAL injection, test harness).

## Dependencies

None.

## API

```c
#include "crc32.h"

#define CRC32_INIT  0xFFFFFFFFu

uint32_t crc32_compute(const uint8_t *data, size_t len);
uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len);
uint32_t crc32_finalize(uint32_t crc);
```

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

None.

## How to test

```bash
cd wheels/primitives/crc32/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **10 C cases** — standard vector, empty, single byte, streaming/bulk
  equivalence, segmented streaming, all-zeros, all-0xFF, CRC residue self-check,
  known string, 1024-byte consistency
- Standard test vector: "123456789" → 0xCBF43926 (IEEE 802.3)
- CRC residue: append CRC LE → compute over frame → magic 0x2144DF1C

## API convention deviations

None.

## API review

- Proposal date: 2026-04-29
- Reviewer: @L-X-Yao (batch-generated, pending review)

## Responsible human

@L-X-Yao
