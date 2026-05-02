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

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
typedef enum {
    FIFO_OK         =  0,
    FIFO_ERR_PARAMS = -1,
    FIFO_ERR_FULL   = -2,
    FIFO_ERR_EMPTY  = -3,
} fifo_err_t;

typedef struct {
    uint8_t *buf;
    uint16_t capacity;
    uint16_t elem_size;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} fifo_queue_t;

fifo_err_t fifo_queue_init(fifo_queue_t *q, void *storage,
                           uint16_t capacity, uint16_t elem_size);
fifo_err_t fifo_queue_push(fifo_queue_t *q, const void *item);
fifo_err_t fifo_queue_pop(fifo_queue_t *q, void *item);
fifo_err_t fifo_queue_peek(const fifo_queue_t *q, void *item);
uint16_t   fifo_queue_count(const fifo_queue_t *q);
uint16_t   fifo_queue_free(const fifo_queue_t *q);
bool       fifo_queue_is_empty(const fifo_queue_t *q);
bool       fifo_queue_is_full(const fifo_queue_t *q);
void       fifo_queue_clear(fifo_queue_t *q);
```

## Configuration (compile-time)

None. All parameters set at runtime via `fifo_queue_init`.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/fifo_queue/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_fifo_queue
```

## Test status

- Unit tests (sim): 15 tests, all green
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: uint16_t indices (64K element cap suits embedded); caller-provided storage; memcpy-based element transfer for type safety without generics

## Responsible human

@user
