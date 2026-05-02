# byte_buffer

**Status: candidate**
**Layer: L0 (primitives)**

Linear read/write byte buffer with separate cursors for protocol packet
assembly, UART accumulation, and serialization. Supports u8/u16/u32
big-endian put/get, bulk byte operations, read pointer access, and
compaction. Caller provides backing storage. No heap, fully reentrant.
## Origin

Proactive build. Linear byte buffers are used in every protocol stack
for packet assembly (write header → write payload → send) and parsing
(receive → read header → read payload).


## Why self-written?

Searched GitHub (2026-05-02). No lightweight zero-dependency C99 implementation found. Ali-Mirghasemi/Stream is over-engineered for this use case; this wheel is 133 lines trivial. Self-written to match platform conventions.

## Dependencies

None. Pure C99 with `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<string.h>`.

## API

```c
byte_buffer_err_t byte_buffer_init(byte_buffer_t *buf, uint8_t *storage, size_t capacity);
byte_buffer_err_t byte_buffer_reset(byte_buffer_t *buf);
byte_buffer_err_t byte_buffer_put_u8(byte_buffer_t *buf, uint8_t val);
byte_buffer_err_t byte_buffer_put_u16_be(byte_buffer_t *buf, uint16_t val);
byte_buffer_err_t byte_buffer_put_u32_be(byte_buffer_t *buf, uint32_t val);
byte_buffer_err_t byte_buffer_put_bytes(byte_buffer_t *buf, const uint8_t *src, size_t len);
byte_buffer_err_t byte_buffer_get_u8(byte_buffer_t *buf, uint8_t *val);
byte_buffer_err_t byte_buffer_get_u16_be(byte_buffer_t *buf, uint16_t *val);
byte_buffer_err_t byte_buffer_get_u32_be(byte_buffer_t *buf, uint32_t *val);
byte_buffer_err_t byte_buffer_get_bytes(byte_buffer_t *buf, uint8_t *dst, size_t len);
size_t            byte_buffer_readable(const byte_buffer_t *buf);
size_t            byte_buffer_writable(const byte_buffer_t *buf);
const uint8_t    *byte_buffer_read_ptr(const byte_buffer_t *buf);
size_t            byte_buffer_written(const byte_buffer_t *buf);
byte_buffer_err_t byte_buffer_compact(byte_buffer_t *buf);
```

## Configuration (compile-time)

None. Buffer capacity is runtime.

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/byte_buffer/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_byte_buffer
```

56 assertions across 13 test cases: init, null safety (11 paths), u8/u16/u32
put+get round-trip with endianness verification, bulk bytes, overflow, underflow,
reset, read pointer tracking, compact (move unread to front), mixed types,
written count.

## Test status

- Unit tests (sim): PASS (13 C tests, 56 assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Big-endian only (network byte order, most common in protocols); linear not circular (use vsf_fifo or ring_buffer for circular); compact moves unread data to buffer start (reclaims space without reset); read_ptr exposes raw pointer for zero-copy access; separate read/write cursors (not consume-on-read).

## Responsible human
@user
