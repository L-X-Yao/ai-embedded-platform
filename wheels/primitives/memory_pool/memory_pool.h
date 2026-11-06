// SPDX-License-Identifier: MIT
/*
 * memory_pool --- fixed-size block allocator for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * O(1) alloc and free via an intrusive free-list embedded in block
 * storage.  Caller provides the backing buffer.  No heap, no OS.
 * Suitable for ISR-safe allocation when wrapped with a critical section.
 * Thread safety: not thread-safe.  Callers must serialize access.
 */

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MEMORY_POOL_OK          =  0,
    MEMORY_POOL_ERR_NULL    = -1,  /* NULL pointer argument            */
    MEMORY_POOL_ERR_SIZE    = -2,  /* block_size or buf too small or misaligned */
    MEMORY_POOL_ERR_FULL    = -3,  /* no free blocks                   */
    MEMORY_POOL_ERR_INVALID = -4,  /* ptr not from this pool           */
} memory_pool_err_t;

/* ---------- instance ---------- */

typedef struct {
    uint8_t *buf;          /* internal: caller-owned backing buffer   */
    size_t   block_size;   /* internal: bytes per block               */
    size_t   block_count;  /* internal: total number of blocks        */
    void    *free_list;    /* internal: singly-linked list of free blocks */
    size_t   used;         /* internal: currently allocated count     */
} memory_pool_t;

/* ---------- API ---------- */

/** Initialize pool over caller-provided buffer.
 *  block_size must be >= sizeof(void*) and a multiple of the alignment of void*.
 *  buf must be aligned for void*.  A plain uint8_t array has no such guarantee;
 *  declare the storage as union { void *align; uint8_t bytes[N]; }.
 *  buf_size must be >= block_size (determines block_count = buf_size / block_size).
 *  Returns MEMORY_POOL_ERR_SIZE if any of these does not hold.
 *  @thread_safe: no @isr_safe: no */
memory_pool_err_t memory_pool_init(memory_pool_t *pool, uint8_t *buf,
                                   size_t buf_size, size_t block_size);

/** Allocate one block.  Returns NULL if pool is full.
 *  @thread_safe: no @isr_safe: yes (if caller serializes) */
void *memory_pool_alloc(memory_pool_t *pool);

/** Free a previously allocated block.
 *  Validates pointer is within pool bounds and properly aligned.
 *  Double-free is detected only when the pool is fully free (used==0).
 *  When other blocks are allocated, double-free corrupts the free list;
 *  callers must track ownership to avoid it.
 *  @thread_safe: no @isr_safe: yes (if caller serializes) */
memory_pool_err_t memory_pool_free(memory_pool_t *pool, void *ptr);

/** Return number of free blocks. @thread_safe: no @isr_safe: yes */
size_t memory_pool_available(const memory_pool_t *pool);

/** Return number of allocated blocks. @thread_safe: no @isr_safe: yes */
size_t memory_pool_used(const memory_pool_t *pool);

/** Reset pool to initial state (all blocks free).
 *  @thread_safe: no @isr_safe: no */
void memory_pool_reset(memory_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_POOL_H */
