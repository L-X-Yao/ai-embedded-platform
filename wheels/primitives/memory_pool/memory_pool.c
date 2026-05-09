// SPDX-License-Identifier: MIT
/*
 * memory_pool --- fixed-size block allocator for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "memory_pool.h"
#include "contract.h"
#include <string.h>

/* Each free block stores a pointer to the next free block at its start.
 * This is safe because block_size >= sizeof(void*). */

memory_pool_err_t memory_pool_init(memory_pool_t *pool, uint8_t *buf,
                                   size_t buf_size, size_t block_size)
{
    if (!pool || !buf) {
        return MEMORY_POOL_ERR_NULL;
    }
    if (block_size < sizeof(void *) || buf_size < block_size) {
        return MEMORY_POOL_ERR_SIZE;
    }

    pool->buf         = buf;
    pool->block_size  = block_size;
    pool->block_count = buf_size / block_size;
    pool->used        = 0;

    /* Build the free-list by threading a next-pointer through each block. */
    pool->free_list = NULL;
    for (size_t i = pool->block_count; i > 0; i--) {
        void *block = &buf[(i - 1U) * block_size];
        void **next = (void **)block;
        *next = pool->free_list;
        pool->free_list = block;
    }

    ENSURE(pool->used == 0);
    ENSURE(pool->free_list != NULL);
    return MEMORY_POOL_OK;
}

void *memory_pool_alloc(memory_pool_t *pool)
{
    if (!pool) { return NULL; }

    if (!pool->free_list) {
        return NULL; /* pool is full */
    }

    /* Pop head of free-list. */
    void *block = pool->free_list;
    pool->free_list = *(void **)block;
    pool->used++;

    ENSURE(pool->used <= pool->block_count);
    return block;
}

memory_pool_err_t memory_pool_free(memory_pool_t *pool, void *ptr)
{
    if (!pool || !ptr) {
        return MEMORY_POOL_ERR_NULL;
    }

    uint8_t *p = (uint8_t *)ptr;

    /* Validate pointer is within the pool buffer. */
    if (p < pool->buf || p >= pool->buf + pool->block_count * pool->block_size) {
        return MEMORY_POOL_ERR_INVALID;
    }

    /* Validate pointer is aligned to a block boundary. */
    size_t offset = (size_t)(p - pool->buf);
    if (offset % pool->block_size != 0) {
        return MEMORY_POOL_ERR_INVALID;
    }

    /* Validate the pool isn't already fully free (would mean double-free). */
    if (pool->used == 0) {
        return MEMORY_POOL_ERR_INVALID;
    }

    /* Push block onto free-list head. */
    void **next = (void **)ptr;
    *next = pool->free_list;
    pool->free_list = ptr;
    pool->used--;

    return MEMORY_POOL_OK;
}

size_t memory_pool_available(const memory_pool_t *pool)
{
    if (!pool) { return 0; }
    return pool->block_count - pool->used;
}

size_t memory_pool_used(const memory_pool_t *pool)
{
    if (!pool) { return 0; }
    return pool->used;
}

void memory_pool_reset(memory_pool_t *pool)
{
    if (!pool) { return; }

    pool->used = 0;
    pool->free_list = NULL;

    /* Rebuild the free-list. */
    for (size_t i = pool->block_count; i > 0; i--) {
        void *block = &pool->buf[(i - 1U) * pool->block_size];
        void **next = (void **)block;
        *next = pool->free_list;
        pool->free_list = block;
    }
}
