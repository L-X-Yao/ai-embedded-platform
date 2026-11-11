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
- `swap()` flips write/read roles; what the producer wrote first is in the buffer by the time the reader sees the flip
- `is_ready()` returns true after at least one swap (read buffer has valid data)
- No malloc -- caller provides both buffers
- One producer and one reader may run at once with no lock, for example a
  DMA-complete ISR and the main loop. `double_buffer.h` says which side may
  call what

## Configuration (compile-time)

- `DOUBLE_BUFFER_BARRIER()`: the barrier around the role change. It defaults to
  a compiler barrier on GCC and Clang. Define it as a real fence if the two
  sides run on different cores, and on any other compiler, where the build
  stops with `#error` until you do.

## Supported platforms

- [x] Host (sim) -- 2026-05-11
- [ ] Real hardware target -- not yet verified

## Known issues

- A swap hands the buffer the reader holds to the producer. No lock in here can
  prevent that: the reader must finish before the next swap.
- `is_ready()` stays true after the first swap, so it does not say a new frame
  has come. A changed `get_read()` pointer does, unless two swaps fell between
  looks.
- The barrier orders CPU accesses only. If DMA fills a buffer on a core with a
  data cache, such as a Cortex-M7, cache upkeep is the caller's job.

## How to test

```bash
cd wheels/primitives/double_buffer/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_double_buffer
```

## Test status

- Unit tests (sim): 25 tests, all passing
- Race test (sim): `test_race_double_buffer`, host threads, built -O2 -flto
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-11
- **Key decisions**: Caller provides both buffers (no heap).
  write_idx uses XOR swap for constant-time toggle.
  ready flag tracks whether read buffer has valid data.
  buf_a == buf_b rejected as parameter error.

## Migration

**2026-11: a swap is visible to a poller.** `write_idx` and `ready` were plain
fields, so a main loop polling `is_ready` hung once the compiler could see
through the call.

- What changed: `write_idx` is volatile. `ready` is `volatile uint8_t`, not
  `bool`, because GCC 13's `-fsanitize=bool` drops the volatile from a bool
  load. `swap` has a barrier. The struct keeps its size.
- What a caller has to do: call each function from the side `double_buffer.h`
  gives it. On a compiler other than GCC or Clang, define
  `DOUBLE_BUFFER_BARRIER()`.
- If they do nothing: existing calls keep working. `if (db.ready)` still works.

## Responsible human

@L-X-Yao
