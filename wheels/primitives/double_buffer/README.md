# double_buffer — ping-pong double buffer

**Status: candidate**
**Layer: L0 (primitives)**

Ping-pong / double buffer for DMA or producer-consumer patterns.
Two caller-owned buffers alternate between write and read roles.
Typical use: DMA fills the write buffer while the application
processes the read buffer, then swap().

## Origin

Proactive build for DMA transfer patterns and producer-consumer
buffering across sensor and communication projects.

## Why self-written?

Textbook data structure. Trivial to implement correctly with platform
conventions (error codes, NULL guards, no heap). Self-written for
consistency.

## Dependencies

None.

## API

```c
double_buffer_err_t double_buffer_init(double_buffer_t *ctx, uint8_t *buf_a,
                                       uint8_t *buf_b, size_t buf_size);
uint8_t            *double_buffer_get_write(const double_buffer_t *ctx);
uint8_t            *double_buffer_get_read(const double_buffer_t *ctx);
size_t              double_buffer_get_size(const double_buffer_t *ctx);
double_buffer_err_t double_buffer_swap(double_buffer_t *ctx);
bool                double_buffer_is_ready(const double_buffer_t *ctx);
double_buffer_err_t double_buffer_reset(double_buffer_t *ctx);
```

- `get_write()` / `get_read()` return pointers to current write/read buffers
- `swap()` atomically (from caller's perspective) flips write/read roles
- `is_ready()` returns true after at least one swap (read buffer has valid data)
- No malloc -- caller provides both buffers

## Configuration (compile-time)

None. No compile-time options.

## Supported platforms

- [x] Host (sim) -- 2026-05-11
- [ ] Real hardware target -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/double_buffer/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_double_buffer
```

## Test status

- Unit tests (sim): 25 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-11
- **Key decisions**: Caller provides both buffers (no heap).
  write_idx uses XOR swap for constant-time toggle.
  ready flag tracks whether read buffer has valid data.
  buf_a == buf_b rejected as parameter error.

## Responsible human

@user
