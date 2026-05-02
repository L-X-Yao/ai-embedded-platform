# bitfield

**Status: candidate**
**Layer: L0 (primitives)**

Bit-level pack/unpack from byte arrays. MSB-first (big-endian bit numbering):
bit 0 is the MSB of byte 0. Useful for protocol parsing (CAN, Modbus register
bit masks, custom telemetry frames) and hardware register manipulation.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
#include "bitfield.h"

int bitfield_get(const uint8_t *data, size_t data_len,
                 size_t bit_offset, size_t bit_count, uint32_t *out);
int bitfield_set(uint8_t *data, size_t data_len,
                 size_t bit_offset, size_t bit_count, uint32_t value);
bool bitfield_test_bit(const uint8_t *data, size_t data_len, size_t bit_index);
void bitfield_set_bit(uint8_t *data, size_t data_len, size_t bit_index);
void bitfield_clear_bit(uint8_t *data, size_t data_len, size_t bit_index);
```

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

None.

## How to test

```bash
cd wheels/primitives/bitfield/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **12 C cases** — aligned/unaligned get, 16/32-bit get, aligned/unaligned
  set, roundtrip, single-bit ops, NULL safety, range check, width check, OOB bit ops

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@user
