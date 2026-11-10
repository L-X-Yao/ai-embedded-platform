# ring_buffer — byte-level circular buffer

**Status: candidate**
**Layer: L0 (primitives)**

Byte-level circular buffer with a write index and a read index.
Supports partial write/read, peek without consuming, and skip (discard).
Suitable for UART RX buffering, protocol framing, and producer-consumer
byte streams.

## Origin

Proactive build for UART RX buffering and protocol framing across
multiple projects (flybox, future serial-based protocols).

## Why self-written?

Searched GitHub (2026-05-02). Found AndersKaloer/Ring-Buffer (MIT). Dozens of similar implementations exist; textbook data structure. Self-written for platform convention consistency (unified error codes, HAL injection, test harness).

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
- One producer and one consumer may run at once with no lock, for example a
  UART ISR and the main loop. `ring_buffer.h` says which side may call what.

## Configuration (compile-time)

- `RING_BUFFER_BARRIER()`: the barrier around the index accesses. It defaults
  to a compiler barrier on GCC and Clang. Define it as a real fence if the two
  sides run on different cores, and on any other compiler, where the build
  stops with `#error` until you do.

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- emulated only (QEMU mps2, M3, M4 and M7, 2026-11-07), no hardware yet

## Known issues

None.

## How to test

```bash
cd wheels/primitives/ring_buffer/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ring_buffer
```

## Test status

- Unit tests (sim): 33 tests, all passing
- Race test (sim): `test_race_ring_buffer`, host threads for the ISR and main loop
- CBMC: `verify_ring_buffer.c`, every operation from every index state at capacity 8
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Byte-level (not typed) for maximum flexibility.
  uint16_t sizes (64KB max) sufficient for embedded use cases.
  Partial write/read semantics over error codes for data transfer
  functions, matching common embedded ring buffer patterns.

## Migration

**2026-11: one producer and one consumer are safe with no lock.** Both sides
used to update one `count` field, so an interrupt between its load and store
lost the update and unread data was overwritten with no error.

- What changed: `count` is gone. `head` and `tail` are `volatile uint32_t` and
  run over `0 .. 2*capacity-1`, so a direct read of them no longer gives a place
  in `buf`. `ring_buffer_t` grows from 12 to 16 bytes on Cortex-M.
  `ring_buffer_reset` is consumer side only and moves `tail` up to `head`
  instead of zeroing both, so the next write need not land at `buf[0]`.
- What a caller has to do: use `ring_buffer_count` and the other functions in
  place of the fields. Call each function from the side `ring_buffer.h` gives
  it. Two writers or two readers still need a lock. On a compiler other than
  GCC or Clang, define `RING_BUFFER_BARRIER()`.
- If they do nothing: code that uses only the functions from one context, or
  from the sides named, keeps working unchanged. Code that read `rb.count`
  stops compiling. A `reset` from the producer side now corrupts the buffer.
- Not re-proved: the ACSL specs follow the new fields, but Frama-C was not run
  on them, and `volatile` may lower the WP proof rate.

## Responsible human

@user
