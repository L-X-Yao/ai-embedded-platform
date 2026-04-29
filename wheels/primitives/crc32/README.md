# crc32

**STATUS: candidate**
**Layer: L0 (primitives)**

CRC32 (IEEE 802.3 / Ethernet / zlib-compatible) table-based computation. Drop-in
for OTA image checksums, file integrity, Ethernet FCS, and any CRC32 with
poly 0xEDB88320 / init 0xFFFFFFFF / final XOR 0xFFFFFFFF.

## Why not VSF native?

Checked VSF source (2026-04-29):

- `vsf/source/component/3rd-party/zlib/` contains `crc32.c` but it's part of the
  zlib compression library — pulling zlib for a standalone CRC32 adds ~40 KB bloat.
- No standalone CRC32 utility in `vsf/source/service/` or `vsf/source/utilities/`.

This wheel provides a minimal 1 KB ROM footprint CRC32 with streaming API.

## API

```c
#include "crc32.h"

#define CRC32_INIT  0xFFFFFFFFu

uint32_t crc32_compute(const uint8_t *data, size_t len);
uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len);
uint32_t crc32_finalize(uint32_t crc);
```

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

## Dependencies

None.

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## API convention deviations

无。

## Responsible human

@user
