# memory_pool

**Status: candidate**
**Layer: L0 (primitives)**

Fixed-size block allocator with O(1) alloc and free via an intrusive free-list.
Caller provides the backing buffer -- no heap, no OS. Suitable for ISR-safe
allocation when wrapped with a critical section. Validates pointer bounds
and alignment on free.

## Origin

Proactive build (ADR-011). Fixed-size block allocator with O(1) alloc and free via an intrusive free-list.


## Why self-written?

existing open-source has its own pool allocator tied to the VSF kernel. This wheel is a
standalone zero-dependency allocator usable in any RTOS or bare-metal context.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

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
/* 64 blocks of 32 bytes; the union makes the storage pointer-aligned */
static union { void *align; uint8_t bytes[64 * 32]; } backing;
memory_pool_t pool;
memory_pool_init(&pool, backing.bytes, sizeof(backing.bytes), 32);

void *blk = memory_pool_alloc(&pool);  /* O(1) */
if (blk) {
    /* use block */
    memory_pool_free(&pool, blk);      /* O(1) */
}
```

Every free block holds a pointer, so `buf` must be aligned for `void *`, and
`block_size` must be at least `sizeof(void *)` and a multiple of that alignment.
`memory_pool_init` returns `MEMORY_POOL_ERR_SIZE` otherwise. A plain `uint8_t`
array has no alignment guarantee.

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/primitives\memory_pool/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_memory_pool
```


## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.


## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
