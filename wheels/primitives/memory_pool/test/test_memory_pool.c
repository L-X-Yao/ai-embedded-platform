// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "memory_pool.h"

/* Helper: buffer for 8 blocks of 16 bytes each = 128 bytes */
#define BLOCK_SIZE  16
#define BLOCK_COUNT  8
#define BUF_SIZE    (BLOCK_SIZE * BLOCK_COUNT)

/* --- init tests --- */

TEST(test_init_valid)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    ASSERT_EQ(memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE), MEMORY_POOL_OK);
    ASSERT_EQ(memory_pool_available(&pool), BLOCK_COUNT);
    ASSERT_EQ(memory_pool_used(&pool), 0);
}

TEST(test_init_null_pool)
{
    uint8_t buf[BUF_SIZE];
    ASSERT_EQ(memory_pool_init(NULL, buf, BUF_SIZE, BLOCK_SIZE), MEMORY_POOL_ERR_NULL);
}

TEST(test_init_null_buf)
{
    memory_pool_t pool;
    ASSERT_EQ(memory_pool_init(&pool, NULL, BUF_SIZE, BLOCK_SIZE), MEMORY_POOL_ERR_NULL);
}

TEST(test_init_block_size_too_small)
{
    uint8_t buf[64];
    memory_pool_t pool;
    /* block_size < sizeof(void*) */
    ASSERT_EQ(memory_pool_init(&pool, buf, 64, 1), MEMORY_POOL_ERR_SIZE);
}

TEST(test_init_buf_too_small)
{
    uint8_t buf[4];
    memory_pool_t pool;
    /* buf_size < block_size */
    ASSERT_EQ(memory_pool_init(&pool, buf, 4, 32), MEMORY_POOL_ERR_SIZE);
}

TEST(test_init_zero_buf_size)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    /* buf_size = 0 < block_size */
    ASSERT_EQ(memory_pool_init(&pool, buf, 0, BLOCK_SIZE), MEMORY_POOL_ERR_SIZE);
}

/* --- alloc tests --- */

TEST(test_alloc_single)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    void *p = memory_pool_alloc(&pool);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(memory_pool_used(&pool), 1);
    ASSERT_EQ(memory_pool_available(&pool), BLOCK_COUNT - 1);
}

TEST(test_alloc_until_full)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    void *ptrs[BLOCK_COUNT];
    for (int i = 0; i < BLOCK_COUNT; i++) {
        ptrs[i] = memory_pool_alloc(&pool);
        ASSERT_NOT_NULL(ptrs[i]);
    }
    ASSERT_EQ(memory_pool_used(&pool), BLOCK_COUNT);
    ASSERT_EQ(memory_pool_available(&pool), 0);

    /* All pointers must be distinct */
    for (int i = 0; i < BLOCK_COUNT; i++) {
        for (int j = i + 1; j < BLOCK_COUNT; j++) {
            ASSERT_TRUE(ptrs[i] != ptrs[j]);
        }
    }
}

TEST(test_alloc_when_full_returns_null)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    for (int i = 0; i < BLOCK_COUNT; i++) {
        memory_pool_alloc(&pool);
    }

    void *p = memory_pool_alloc(&pool);
    ASSERT_TRUE(p == NULL);
    ASSERT_EQ(memory_pool_used(&pool), BLOCK_COUNT);
    ASSERT_EQ(memory_pool_available(&pool), 0);
}

TEST(test_alloc_null_pool)
{
    void *p = memory_pool_alloc(NULL);
    ASSERT_TRUE(p == NULL);
}

/* --- free tests --- */

TEST(test_free_then_realloc)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    void *p1 = memory_pool_alloc(&pool);
    ASSERT_NOT_NULL(p1);
    ASSERT_EQ(memory_pool_free(&pool, p1), MEMORY_POOL_OK);
    ASSERT_EQ(memory_pool_used(&pool), 0);
    ASSERT_EQ(memory_pool_available(&pool), BLOCK_COUNT);

    /* Re-alloc should return the same block (LIFO free-list) */
    void *p2 = memory_pool_alloc(&pool);
    ASSERT_TRUE(p2 == p1);
}

TEST(test_free_invalid_pointer_outside_pool)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    memory_pool_alloc(&pool); /* need at least 1 used */
    uint8_t other[16];
    ASSERT_EQ(memory_pool_free(&pool, other), MEMORY_POOL_ERR_INVALID);
}

TEST(test_free_misaligned_pointer)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    void *p = memory_pool_alloc(&pool);
    ASSERT_NOT_NULL(p);
    /* Offset by 1 byte = misaligned */
    ASSERT_EQ(memory_pool_free(&pool, (uint8_t *)p + 1), MEMORY_POOL_ERR_INVALID);
}

TEST(test_free_null_ptr)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);
    ASSERT_EQ(memory_pool_free(&pool, NULL), MEMORY_POOL_ERR_NULL);
}

TEST(test_free_null_pool)
{
    uint8_t dummy[16];
    ASSERT_EQ(memory_pool_free(NULL, dummy), MEMORY_POOL_ERR_NULL);
}

TEST(test_double_free_detection)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    /* Alloc one, free it, then free again.  Pool has used=0, so double-free
     * is detected via the used==0 check. */
    void *p = memory_pool_alloc(&pool);
    ASSERT_EQ(memory_pool_free(&pool, p), MEMORY_POOL_OK);
    ASSERT_EQ(memory_pool_free(&pool, p), MEMORY_POOL_ERR_INVALID);
}

/* --- reset tests --- */

TEST(test_reset_restores_all_blocks)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    /* Alloc all blocks */
    for (int i = 0; i < BLOCK_COUNT; i++) {
        memory_pool_alloc(&pool);
    }
    ASSERT_EQ(memory_pool_available(&pool), 0);

    memory_pool_reset(&pool);
    ASSERT_EQ(memory_pool_available(&pool), BLOCK_COUNT);
    ASSERT_EQ(memory_pool_used(&pool), 0);

    /* Should be able to alloc all blocks again after reset */
    for (int i = 0; i < BLOCK_COUNT; i++) {
        void *p = memory_pool_alloc(&pool);
        ASSERT_NOT_NULL(p);
    }
}

TEST(test_reset_null_pool)
{
    memory_pool_reset(NULL); /* should not crash */
}

/* --- available/used tracking --- */

TEST(test_available_used_tracking)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    void *ptrs[BLOCK_COUNT];
    for (int i = 0; i < BLOCK_COUNT; i++) {
        ptrs[i] = memory_pool_alloc(&pool);
        ASSERT_EQ(memory_pool_used(&pool), (size_t)(i + 1));
        ASSERT_EQ(memory_pool_available(&pool), (size_t)(BLOCK_COUNT - i - 1));
    }

    for (int i = 0; i < BLOCK_COUNT; i++) {
        memory_pool_free(&pool, ptrs[i]);
        ASSERT_EQ(memory_pool_used(&pool), (size_t)(BLOCK_COUNT - i - 1));
        ASSERT_EQ(memory_pool_available(&pool), (size_t)(i + 1));
    }
}

TEST(test_available_null)
{
    ASSERT_EQ(memory_pool_available(NULL), 0);
}

TEST(test_used_null)
{
    ASSERT_EQ(memory_pool_used(NULL), 0);
}

/* --- error code definitions --- */

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)MEMORY_POOL_OK);
    ASSERT_EQ(-1, (int)MEMORY_POOL_ERR_NULL);
    ASSERT_EQ(-2, (int)MEMORY_POOL_ERR_SIZE);
    ASSERT_EQ(-3, (int)MEMORY_POOL_ERR_FULL);
    ASSERT_EQ(-4, (int)MEMORY_POOL_ERR_INVALID);
}

/* --- write data into allocated blocks --- */

TEST(test_write_data_into_blocks)
{
    uint8_t buf[BUF_SIZE];
    memory_pool_t pool;
    memory_pool_init(&pool, buf, BUF_SIZE, BLOCK_SIZE);

    /* Alloc two blocks, write distinct patterns, verify data isolation */
    uint8_t *p1 = (uint8_t *)memory_pool_alloc(&pool);
    uint8_t *p2 = (uint8_t *)memory_pool_alloc(&pool);
    ASSERT_NOT_NULL(p1);
    ASSERT_NOT_NULL(p2);

    memset(p1, 0xAA, BLOCK_SIZE);
    memset(p2, 0x55, BLOCK_SIZE);

    for (int i = 0; i < BLOCK_SIZE; i++) {
        ASSERT_EQ(p1[i], 0xAA);
        ASSERT_EQ(p2[i], 0x55);
    }
}

/* --- block_size equals sizeof(void*) (minimum) --- */

TEST(test_min_block_size)
{
    size_t min_bs = sizeof(void *);
    uint8_t buf[256];
    memory_pool_t pool;
    ASSERT_EQ(memory_pool_init(&pool, buf, sizeof(buf), min_bs), MEMORY_POOL_OK);

    size_t expected_count = sizeof(buf) / min_bs;
    ASSERT_EQ(memory_pool_available(&pool), expected_count);

    void *p = memory_pool_alloc(&pool);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(memory_pool_free(&pool, p), MEMORY_POOL_OK);
}

TEST_MAIN("memory_pool")
    RUN(test_init_valid);
    RUN(test_init_null_pool);
    RUN(test_init_null_buf);
    RUN(test_init_block_size_too_small);
    RUN(test_init_buf_too_small);
    RUN(test_init_zero_buf_size);
    RUN(test_alloc_single);
    RUN(test_alloc_until_full);
    RUN(test_alloc_when_full_returns_null);
    RUN(test_alloc_null_pool);
    RUN(test_free_then_realloc);
    RUN(test_free_invalid_pointer_outside_pool);
    RUN(test_free_misaligned_pointer);
    RUN(test_free_null_ptr);
    RUN(test_free_null_pool);
    RUN(test_double_free_detection);
    RUN(test_reset_restores_all_blocks);
    RUN(test_reset_null_pool);
    RUN(test_available_used_tracking);
    RUN(test_available_null);
    RUN(test_used_null);
    RUN(test_error_codes);
    RUN(test_write_data_into_blocks);
    RUN(test_min_block_size);
TEST_MAIN_END()
