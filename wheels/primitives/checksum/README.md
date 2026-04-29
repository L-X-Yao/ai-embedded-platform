# checksum

**STATUS: candidate**
**Layer: L0 (primitives)**

Common checksum algorithms collection: XOR, 8-bit sum, Fletcher-16, LRC
(Longitudinal Redundancy Check), and Internet checksum (IP/TCP/UDP).

## Why not VSF native?

Checked VSF source (2026-04-29): `vsfip` has an internal `__vsfip_checksum`
for Internet checksum, but it's static (not exported) and tied to the IP stack.
No standalone checksum utility.

## API

```c
uint8_t  checksum_xor(const uint8_t *data, size_t len);
uint8_t  checksum_sum8(const uint8_t *data, size_t len);
uint16_t checksum_fletcher16(const uint8_t *data, size_t len);
uint8_t  checksum_lrc(const uint8_t *data, size_t len);
uint16_t checksum_internet(const uint8_t *data, size_t len);
```

## Test status

- Unit tests: **9 C cases** — XOR basic/null, sum8, Fletcher-16 basic/consistent,
  LRC verify, Internet checksum RFC 1071, odd length, null

## Dependencies

None.

## How to test

```bash
cd wheels/primitives/checksum/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Known issues

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
