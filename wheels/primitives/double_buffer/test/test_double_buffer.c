// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "double_buffer.h"

/* ── init ───────────────────────────────────────────────────────── */

TEST(test_init_valid)
{
    uint8_t a[16], b[16];
    double_buffer_t ctx;
    ASSERT_EQ(double_buffer_init(&ctx, a, b, 16), DOUBLE_BUFFER_OK);
    ASSERT_FALSE(double_buffer_is_ready(&ctx));
    ASSERT_EQ(double_buffer_get_size(&ctx), 16);
}

TEST(test_init_null_ctx)
{
    uint8_t a[16], b[16];
    ASSERT_EQ(double_buffer_init(NULL, a, b, 16), DOUBLE_BUFFER_ERR_PARAM);
}

TEST(test_init_null_buf_a)
{
    uint8_t b[16];
    double_buffer_t ctx;
    ASSERT_EQ(double_buffer_init(&ctx, NULL, b, 16), DOUBLE_BUFFER_ERR_PARAM);
}

TEST(test_init_null_buf_b)
{
    uint8_t a[16];
    double_buffer_t ctx;
    ASSERT_EQ(double_buffer_init(&ctx, a, NULL, 16), DOUBLE_BUFFER_ERR_PARAM);
}

TEST(test_init_zero_size)
{
    uint8_t a[1], b[1];
    double_buffer_t ctx;
    ASSERT_EQ(double_buffer_init(&ctx, a, b, 0), DOUBLE_BUFFER_ERR_PARAM);
}

TEST(test_init_same_buffer)
{
    uint8_t buf[16];
    double_buffer_t ctx;
    ASSERT_EQ(double_buffer_init(&ctx, buf, buf, 16), DOUBLE_BUFFER_ERR_PARAM);
}

/* ── get_write / get_read after init ────────────────────────────── */

TEST(test_initial_write_is_buf_a)
{
    uint8_t a[8], b[8];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 8);
    ASSERT_TRUE(double_buffer_get_write(&ctx) == a);
    ASSERT_TRUE(double_buffer_get_read(&ctx) == b);
}

/* ── swap ───────────────────────────────────────────────────────── */

TEST(test_swap_flips_buffers)
{
    uint8_t a[8], b[8];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 8);

    /* Before swap: write=a, read=b */
    ASSERT_TRUE(double_buffer_get_write(&ctx) == a);
    ASSERT_TRUE(double_buffer_get_read(&ctx) == b);

    ASSERT_EQ(double_buffer_swap(&ctx), DOUBLE_BUFFER_OK);

    /* After swap: write=b, read=a */
    ASSERT_TRUE(double_buffer_get_write(&ctx) == b);
    ASSERT_TRUE(double_buffer_get_read(&ctx) == a);
}

TEST(test_swap_sets_ready)
{
    uint8_t a[8], b[8];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 8);

    ASSERT_FALSE(double_buffer_is_ready(&ctx));
    double_buffer_swap(&ctx);
    ASSERT_TRUE(double_buffer_is_ready(&ctx));
}

TEST(test_double_swap_returns_to_original)
{
    uint8_t a[8], b[8];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 8);

    double_buffer_swap(&ctx);
    double_buffer_swap(&ctx);

    /* Back to original assignment */
    ASSERT_TRUE(double_buffer_get_write(&ctx) == a);
    ASSERT_TRUE(double_buffer_get_read(&ctx) == b);
    ASSERT_TRUE(double_buffer_is_ready(&ctx));
}

TEST(test_swap_null_ctx)
{
    ASSERT_EQ(double_buffer_swap(NULL), DOUBLE_BUFFER_ERR_PARAM);
}

/* ── data isolation ─────────────────────────────────────────────── */

TEST(test_write_read_data_isolation)
{
    uint8_t a[4], b[4];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 4);

    /* Fill write buffer (a) with pattern */
    uint8_t *wr = double_buffer_get_write(&ctx);
    memset(wr, 0xAA, 4);

    /* swap: a becomes read, b becomes write */
    double_buffer_swap(&ctx);

    /* Read buffer should have the pattern we wrote */
    uint8_t *rd = double_buffer_get_read(&ctx);
    ASSERT_EQ(rd[0], 0xAA);
    ASSERT_EQ(rd[1], 0xAA);
    ASSERT_EQ(rd[2], 0xAA);
    ASSERT_EQ(rd[3], 0xAA);

    /* Write to new write buffer (b) with different pattern */
    wr = double_buffer_get_write(&ctx);
    memset(wr, 0xBB, 4);

    /* Read buffer still has old data */
    rd = double_buffer_get_read(&ctx);
    ASSERT_EQ(rd[0], 0xAA);

    /* Swap again: b becomes read */
    double_buffer_swap(&ctx);
    rd = double_buffer_get_read(&ctx);
    ASSERT_EQ(rd[0], 0xBB);
}

/* ── DMA-like usage pattern ─────────────────────────────────────── */

TEST(test_dma_pattern)
{
    uint8_t a[8], b[8];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 8);

    /* Simulate 3 DMA cycles */
    for (int cycle = 0; cycle < 3; cycle++) {
        uint8_t *wr = double_buffer_get_write(&ctx);
        /* DMA fills write buffer */
        for (int i = 0; i < 8; i++) {
            wr[i] = (uint8_t)(cycle * 10 + i);
        }
        double_buffer_swap(&ctx);

        /* App reads the completed buffer */
        uint8_t *rd = double_buffer_get_read(&ctx);
        for (int i = 0; i < 8; i++) {
            ASSERT_EQ(rd[i], (uint8_t)(cycle * 10 + i));
        }
    }
}

/* ── get_size ───────────────────────────────────────────────────── */

TEST(test_get_size)
{
    uint8_t a[32], b[32];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 32);
    ASSERT_EQ(double_buffer_get_size(&ctx), 32);
}

TEST(test_get_size_null)
{
    ASSERT_EQ(double_buffer_get_size(NULL), 0);
}

TEST(test_get_size_one)
{
    uint8_t a[1], b[1];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 1);
    ASSERT_EQ(double_buffer_get_size(&ctx), 1);
}

/* ── is_ready ───────────────────────────────────────────────────── */

TEST(test_is_ready_null)
{
    ASSERT_FALSE(double_buffer_is_ready(NULL));
}

TEST(test_is_ready_stays_true_after_multiple_swaps)
{
    uint8_t a[4], b[4];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 4);

    double_buffer_swap(&ctx);
    ASSERT_TRUE(double_buffer_is_ready(&ctx));
    double_buffer_swap(&ctx);
    ASSERT_TRUE(double_buffer_is_ready(&ctx));
    double_buffer_swap(&ctx);
    ASSERT_TRUE(double_buffer_is_ready(&ctx));
}

/* ── reset ──────────────────────────────────────────────────────── */

TEST(test_reset)
{
    uint8_t a[8], b[8];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 8);

    double_buffer_swap(&ctx);
    ASSERT_TRUE(double_buffer_is_ready(&ctx));

    ASSERT_EQ(double_buffer_reset(&ctx), DOUBLE_BUFFER_OK);
    ASSERT_FALSE(double_buffer_is_ready(&ctx));
    /* write goes back to buf_a */
    ASSERT_TRUE(double_buffer_get_write(&ctx) == a);
    ASSERT_TRUE(double_buffer_get_read(&ctx) == b);
}

TEST(test_reset_null)
{
    ASSERT_EQ(double_buffer_reset(NULL), DOUBLE_BUFFER_ERR_PARAM);
}

TEST(test_reset_then_reuse)
{
    uint8_t a[4], b[4];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 4);

    /* Use it */
    memset(double_buffer_get_write(&ctx), 0xAA, 4);
    double_buffer_swap(&ctx);

    /* Reset and reuse */
    double_buffer_reset(&ctx);
    ASSERT_FALSE(double_buffer_is_ready(&ctx));

    memset(double_buffer_get_write(&ctx), 0xBB, 4);
    double_buffer_swap(&ctx);
    ASSERT_TRUE(double_buffer_is_ready(&ctx));

    uint8_t *rd = double_buffer_get_read(&ctx);
    ASSERT_EQ(rd[0], 0xBB);
}

/* ── get_write / get_read null ──────────────────────────────────── */

TEST(test_get_write_null)
{
    ASSERT_TRUE(double_buffer_get_write(NULL) == NULL);
}

TEST(test_get_read_null)
{
    ASSERT_TRUE(double_buffer_get_read(NULL) == NULL);
}

/* ── many swaps stability ───────────────────────────────────────── */

TEST(test_many_swaps)
{
    uint8_t a[4], b[4];
    double_buffer_t ctx;
    double_buffer_init(&ctx, a, b, 4);

    for (int i = 0; i < 100; i++) {
        double_buffer_swap(&ctx);
    }
    /* 100 swaps (even) → back to original */
    ASSERT_TRUE(double_buffer_get_write(&ctx) == a);
    ASSERT_TRUE(double_buffer_get_read(&ctx) == b);

    double_buffer_swap(&ctx);
    /* 101 swaps (odd) → flipped */
    ASSERT_TRUE(double_buffer_get_write(&ctx) == b);
    ASSERT_TRUE(double_buffer_get_read(&ctx) == a);
}

/* ── error enum value ───────────────────────────────────────────── */

TEST(test_error_enum_values)
{
    ASSERT_EQ(DOUBLE_BUFFER_OK, 0);
    ASSERT_EQ(DOUBLE_BUFFER_ERR_PARAM, -1);
}

TEST_MAIN("double_buffer")
    RUN(test_init_valid);
    RUN(test_init_null_ctx);
    RUN(test_init_null_buf_a);
    RUN(test_init_null_buf_b);
    RUN(test_init_zero_size);
    RUN(test_init_same_buffer);
    RUN(test_initial_write_is_buf_a);
    RUN(test_swap_flips_buffers);
    RUN(test_swap_sets_ready);
    RUN(test_double_swap_returns_to_original);
    RUN(test_swap_null_ctx);
    RUN(test_write_read_data_isolation);
    RUN(test_dma_pattern);
    RUN(test_get_size);
    RUN(test_get_size_null);
    RUN(test_get_size_one);
    RUN(test_is_ready_null);
    RUN(test_is_ready_stays_true_after_multiple_swaps);
    RUN(test_reset);
    RUN(test_reset_null);
    RUN(test_reset_then_reuse);
    RUN(test_get_write_null);
    RUN(test_get_read_null);
    RUN(test_many_swaps);
    RUN(test_error_enum_values);
TEST_MAIN_END()
