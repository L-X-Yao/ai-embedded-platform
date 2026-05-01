# endian — portable byte-order conversion utilities

**Status: candidate**
**Layer: L0 (primitives)**

Host/big/little-endian conversion, unaligned buffer read/write, and byte swap.
All shift-based for portability across architectures — no pointer casts, no
alignment assumptions. Pure C99, no OS, no malloc.

## Origin

Proactive build for protocol implementations (MQTT, Modbus, CoAP, etc.) that
use big-endian wire format on little-endian MCUs.

## Why not VSF native?

VSF does not provide a portable endian conversion API. `<endian.h>` is
POSIX/glibc-specific and unavailable on bare-metal toolchains.

## Dependencies

None.

## API

```c
/* Byte swap */
uint16_t endian_swap16(uint16_t val);
uint32_t endian_swap32(uint32_t val);
uint64_t endian_swap64(uint64_t val);

/* Runtime endianness detection */
bool endian_is_little(void);

/* Host <-> big-endian */
uint16_t endian_htobe16(uint16_t val);
uint32_t endian_htobe32(uint32_t val);
uint64_t endian_htobe64(uint64_t val);
uint16_t endian_be16toh(uint16_t val);
uint32_t endian_be32toh(uint32_t val);
uint64_t endian_be64toh(uint64_t val);

/* Host <-> little-endian */
uint16_t endian_htole16(uint16_t val);
uint32_t endian_htole32(uint32_t val);
uint64_t endian_htole64(uint64_t val);
uint16_t endian_le16toh(uint16_t val);
uint32_t endian_le32toh(uint32_t val);
uint64_t endian_le64toh(uint64_t val);

/* Unaligned buffer read (safe on any alignment) */
uint16_t endian_read_be16(const uint8_t *buf);
uint32_t endian_read_be32(const uint8_t *buf);
uint16_t endian_read_le16(const uint8_t *buf);
uint32_t endian_read_le32(const uint8_t *buf);

/* Unaligned buffer write (safe on any alignment) */
void endian_write_be16(uint8_t *buf, uint16_t val);
void endian_write_be32(uint8_t *buf, uint32_t val);
void endian_write_le16(uint8_t *buf, uint16_t val);
void endian_write_le32(uint8_t *buf, uint32_t val);
```

## Configuration (compile-time)

No configuration needed. Endianness is auto-detected at compile time via
`__BYTE_ORDER__` (GCC/Clang) or common architecture macros, with a runtime
fallback.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] GD32F405 — not yet verified (expected: works, ARM is little-endian)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/endian/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_endian
```

## Test status

- Unit tests (sim): 21 pass, 0 fail
- HIL: N/A

## API convention deviations

None. No error enum — all functions are pure and cannot fail (no NULL
checks needed since callers always provide valid buffers/values).

## API review

- **Date**: 2026-05-01
- **Key decisions**: Shift-based only (no memcpy/cast); no error return
  since all functions are pure; buffer API limited to 16/32-bit (64-bit
  buffer ops can be added if needed).

## Responsible human

@user
