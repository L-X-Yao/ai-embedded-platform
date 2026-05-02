# checksum

**Status: candidate**
**Layer: L0 (primitives)**

Common checksum algorithms collection: XOR, 8-bit sum, Fletcher-16, LRC
(Longitudinal Redundancy Check), and Internet checksum (IP/TCP/UDP).

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). Found lammertb/libcrc (MIT). Covers fletcher/adler checksums; 69 lines barely justifies vendoring a dependency. Self-written for platform convention consistency (unified error codes, HAL injection, test harness).

## Dependencies

None.

## API

```c
uint8_t  checksum_xor(const uint8_t *data, size_t len);
uint8_t  checksum_sum8(const uint8_t *data, size_t len);
uint16_t checksum_fletcher16(const uint8_t *data, size_t len);
uint8_t  checksum_lrc(const uint8_t *data, size_t len);
uint16_t checksum_internet(const uint8_t *data, size_t len);
```

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

None.

## How to test

```bash
cd wheels/primitives/checksum/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **9 C cases** — XOR basic/null, sum8, Fletcher-16 basic/consistent,
  LRC verify, Internet checksum RFC 1071, odd length, null

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@user
