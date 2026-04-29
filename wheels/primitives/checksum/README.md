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
