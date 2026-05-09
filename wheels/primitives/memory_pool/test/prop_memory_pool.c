/*
 * prop_memory_pool.c --- property-based tests for memory_pool
 *
 * Build: gcc -std=c99 -Wall -Wextra -I.. -I../../../../tooling/harness prop_memory_pool.c ../memory_pool.c -o prop_memory_pool
 */

#include "test_harness.h"
#include "property_test.h"
#include "../memory_pool.h"
#include <string.h>

#define BLOCK_SIZE  sizeof(void *)  /* minimum allowed */
#define MAX_BLOCKS  32
#define BUF_SIZE    (BLOCK_SIZE * MAX_BLOCKS)

/* Property: alloc/free roundtrip -- alloc N blocks, free them all, pool restores */
PROPERTY(alloc_free_roundtrip, 5000) {
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    size_t n = ARBI_SIZE(seed, MAX_BLOCKS - 1) + 1;
    void *ptrs[MAX_BLOCKS];

    for (size_t i = 0; i < n; i++) {
        ptrs[i] = memory_pool_alloc(&pool);
        PROP_ASSERT(ptrs[i] != NULL);
    }
    PROP_ASSERT_EQ((long long)memory_pool_used(&pool), (long long)n);

    for (size_t i = 0; i < n; i++) {
        PROP_ASSERT_EQ(memory_pool_free(&pool, ptrs[i]), MEMORY_POOL_OK);
    }
    PROP_ASSERT_EQ((long long)memory_pool_used(&pool), 0);
    PROP_ASSERT_EQ((long long)memory_pool_available(&pool), (long long)MAX_BLOCKS);
}

/* Property: no double-alloc -- all returned pointers are distinct */
PROPERTY(no_double_alloc, 5000) {
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    size_t n = ARBI_SIZE(seed, MAX_BLOCKS - 1) + 1;
    void *ptrs[MAX_BLOCKS];

    for (size_t i = 0; i < n; i++) {
        ptrs[i] = memory_pool_alloc(&pool);
        PROP_ASSERT(ptrs[i] != NULL);
        /* Check against all previously allocated */
        for (size_t j = 0; j < i; j++) {
            PROP_ASSERT(ptrs[i] != ptrs[j]);
        }
    }
}

/* Property: reset restores all blocks */
PROPERTY(reset_restores_all, 5000) {
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    /* Allocate random number of blocks */
    size_t n = ARBI_SIZE(seed, MAX_BLOCKS);
    for (size_t i = 0; i < n; i++) {
        memory_pool_alloc(&pool);
    }

    memory_pool_reset(&pool);
    PROP_ASSERT_EQ((long long)memory_pool_used(&pool), 0);
    PROP_ASSERT_EQ((long long)memory_pool_available(&pool), (long long)MAX_BLOCKS);

    /* All blocks should be allocatable again */
    for (size_t i = 0; i < MAX_BLOCKS; i++) {
        void *p = memory_pool_alloc(&pool);
        PROP_ASSERT(p != NULL);
    }
}

/* Property: available + used = total always holds */
PROPERTY(available_plus_used_eq_total, 5000) {
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);
    void *ptrs[MAX_BLOCKS];

    size_t n = ARBI_SIZE(seed, MAX_BLOCKS);
    for (size_t i = 0; i < n; i++) {
        ptrs[i] = memory_pool_alloc(&pool);
        PROP_ASSERT((long long)(memory_pool_available(&pool) + memory_pool_used(&pool))
                    == (long long)MAX_BLOCKS);
    }

    /* Free in random order */
    for (size_t i = n; i > 0; i--) {
        size_t idx = ARBI_SIZE(seed, i - 1);
        memory_pool_free(&pool, ptrs[idx]);
        ptrs[idx] = ptrs[i - 1]; /* swap-remove */
        PROP_ASSERT((long long)(memory_pool_available(&pool) + memory_pool_used(&pool))
                    == (long long)MAX_BLOCKS);
    }
}

/* Property: pool full returns NULL, no crash */
PROPERTY(pool_full_returns_null, 1000) {
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    for (size_t i = 0; i < MAX_BLOCKS; i++) {
        PROP_ASSERT(memory_pool_alloc(&pool) != NULL);
    }
    PROP_ASSERT(memory_pool_alloc(&pool) == NULL);
    PROP_ASSERT_EQ((long long)memory_pool_used(&pool), (long long)MAX_BLOCKS);
    PROP_ASSERT_EQ((long long)memory_pool_available(&pool), 0);
}

/* Property: free with invalid pointer returns error */
PROPERTY(free_invalid_ptr_rejected, 1000) {
    uint8_t buf[BUF_SIZE];
    uint8_t other[64];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    void *p = memory_pool_alloc(&pool);
    PROP_ASSERT(p != NULL);

    /* Pointer outside pool */
    PROP_ASSERT_EQ(memory_pool_free(&pool, other), MEMORY_POOL_ERR_INVALID);
    /* NULL pointer */
    PROP_ASSERT_EQ(memory_pool_free(&pool, NULL), MEMORY_POOL_ERR_NULL);
}

TEST_MAIN("memory_pool properties") {
    RUN(alloc_free_roundtrip);
    RUN(no_double_alloc);
    RUN(reset_restores_all);
    RUN(available_plus_used_eq_total);
    RUN(pool_full_returns_null);
    RUN(free_invalid_ptr_rejected);
}
TEST_MAIN_END()
