# fifo_queue — typed fixed-size FIFO queue

**Status: candidate**
**Layer: L0 (primitives)**

Circular queue for fixed-size elements (structs, ints, etc.). Complements
byte_buffer (linear bytes) — this wheel handles typed items where each
element has a uniform size set at init. Caller provides backing storage.
No heap, fully reentrant.

## Origin

Proactive build for embedded task queues, sensor sample buffers, and
command pipelines where items are fixed-size structs rather than raw bytes.

## Why self-written?

Searched GitHub (2026-05-02). Found geekfactory/FIFO (MIT). Trivial data structure; dozens of similar implementations exist. Self-written for platform convention consistency (unified error codes, HAL injection, test harness).

## Dependencies

None.

## API

```c
typedef enum {
    FIFO_QUEUE_OK        =  0,
    FIFO_QUEUE_ERR_PARAM = -1,
    FIFO_QUEUE_ERR_FULL  = -2,
    FIFO_QUEUE_ERR_EMPTY = -3,
} fifo_queue_err_t;

typedef struct {
    uint8_t *buf;
    uint16_t capacity;
    uint16_t elem_size;
    volatile uint32_t head;  /* read index: only the consumer stores it  */
    volatile uint32_t tail;  /* write index: only the producer stores it */
} fifo_queue_t;

fifo_queue_err_t fifo_queue_init(fifo_queue_t *q, void *storage,
                           uint16_t capacity, uint16_t elem_size);
fifo_queue_err_t fifo_queue_push(fifo_queue_t *q, const void *item);
fifo_queue_err_t fifo_queue_pop(fifo_queue_t *q, void *item);
fifo_queue_err_t fifo_queue_peek(const fifo_queue_t *q, void *item);
uint16_t   fifo_queue_count(const fifo_queue_t *q);
uint16_t   fifo_queue_free(const fifo_queue_t *q);
bool       fifo_queue_is_empty(const fifo_queue_t *q);
bool       fifo_queue_is_full(const fifo_queue_t *q);
void       fifo_queue_clear(fifo_queue_t *q);
```

One producer and one consumer may run at once with no lock, for example an ISR
and the main loop. `fifo_queue.h` says which side may call what.

## Configuration (compile-time)

- `FIFO_QUEUE_BARRIER()`: the barrier around the index accesses. It defaults to
  a compiler barrier on GCC and Clang. Define it as a real fence if the two
  sides run on different cores, and on any other compiler, where the build
  stops with `#error` until you do.

All other parameters are set at runtime via `fifo_queue_init`.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — emulated only (QEMU mps2, M3, M4 and M7, 2026-11-07), no hardware yet

## Known issues

None.

## How to test

```bash
cd wheels/primitives/fifo_queue/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_fifo_queue
```

## Test status

- Unit tests (sim): 17 tests, all green
- Race test (sim): `test_race_fifo_queue`, host threads for the ISR and main loop
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: uint16_t indices (64K element cap suits embedded); caller-provided storage; memcpy-based element transfer for type safety without generics

## Migration

**2026-11: one producer and one consumer are safe with no lock.** Both sides
used to update one `count` field, so an interrupt between its load and store
lost the update and unread items were overwritten with no error.

- What changed: `count` is gone. `head` and `tail` are `volatile uint32_t` and
  run over `0 .. 2*capacity-1`, so a direct read of them no longer gives a slot.
  `fifo_queue_t` stays 16 bytes on Cortex-M. `fifo_queue_clear` is consumer
  side only and moves `head` up to `tail` instead of zeroing both.
- What a caller has to do: use `fifo_queue_count` and the other functions in
  place of the fields. Call each function from the side `fifo_queue.h` gives it.
  Two producers or two consumers still need a lock. On a compiler other than
  GCC or Clang, define `FIFO_QUEUE_BARRIER()`.
- If they do nothing: code that uses only the functions from one context, or
  from the sides named, keeps working unchanged. Code that read `q.count` stops
  compiling. A `clear` from the producer side now corrupts the queue.

## Responsible human

@L-X-Yao
