# memory_pool

**Status: candidate**
**Layer: L0 (primitives)**

Fixed-size block allocator with O(1) alloc and free via an intrusive free-list.
Caller provides the backing buffer -- no heap, no OS. Suitable for ISR-safe
allocation when wrapped with a critical section. Validates pointer bounds
and alignment on free.

## Why not VSF native?

VSF has its own pool allocator tied to the VSF kernel. This wheel is a
standalone zero-dependency allocator usable in any RTOS or bare-metal context.

## API

```c
#include "memory_pool.h"

memory_pool_err_t memory_pool_init(memory_pool_t *pool, uint8_t *buf,
                                   size_t buf_size, size_t block_size);
void             *memory_pool_alloc(memory_pool_t *pool);
memory_pool_err_t memory_pool_free(memory_pool_t *pool, void *ptr);
size_t            memory_pool_available(const memory_pool_t *pool);
size_t            memory_pool_used(const memory_pool_t *pool);
void              memory_pool_reset(memory_pool_t *pool);
```

## Usage

```c
static uint8_t backing[64 * 32];  /* 64 blocks of 32 bytes */
memory_pool_t pool;
memory_pool_init(&pool, backing, sizeof(backing), 32);

void *blk = memory_pool_alloc(&pool);  /* O(1) */
if (blk) {
    /* use block */
    memory_pool_free(&pool, blk);      /* O(1) */
}
```

## Dependencies

None. Pure C99.

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Test status

- Unit tests (sim): PASS
- HIL: N/A
