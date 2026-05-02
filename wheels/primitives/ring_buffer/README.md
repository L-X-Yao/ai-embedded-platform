# ring_buffer — byte-level circular buffer

**Status: candidate**
**Layer: L0 (primitives)**

Byte-level circular buffer with head/tail pointers and explicit count.
Supports partial write/read, peek without consuming, and skip (discard).
Suitable for UART RX buffering, protocol framing, and producer-consumer
byte streams.

## Origin

Proactive build for UART RX buffering and protocol framing across
multiple projects (flybox, future serial-based protocols).

## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
ring_buffer_err_t ring_buffer_init(ring_buffer_t *rb, uint8_t *buf, uint16_t capacity);
uint16_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, uint16_t len);
uint16_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, uint16_t len);
uint16_t ring_buffer_peek(const ring_buffer_t *rb, uint8_t *data, uint16_t len);
uint16_t ring_buffer_skip(ring_buffer_t *rb, uint16_t len);
uint16_t ring_buffer_count(const ring_buffer_t *rb);
uint16_t ring_buffer_free(const ring_buffer_t *rb);
bool     ring_buffer_is_empty(const ring_buffer_t *rb);
bool     ring_buffer_is_full(const ring_buffer_t *rb);
void     ring_buffer_reset(ring_buffer_t *rb);
```

- `write()` / `read()` return bytes actually transferred (partial on full/empty)
- `peek()` reads without consuming
- `skip()` discards bytes without copying
- No malloc -- caller provides backing buffer

## Configuration (compile-time)

None. No compile-time options.

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/ring_buffer/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ring_buffer
```

## Test status

- Unit tests (sim): 18 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Byte-level (not typed) for maximum flexibility.
  uint16_t sizes (64KB max) sufficient for embedded use cases.
  Partial write/read semantics over error codes for data transfer
  functions, matching common embedded ring buffer patterns.

## Responsible human

@user
