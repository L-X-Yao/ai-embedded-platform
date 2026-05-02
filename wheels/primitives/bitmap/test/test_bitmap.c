// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "bitmap.h"

/* ---- init ---- */

TEST(test_init_basic)
{
    uint8_t buf[4];
    bitmap_t bm;
    ASSERT_EQ(BITMAP_OK, bitmap_init(&bm, buf, 32));
    ASSERT_EQ(32, bitmap_size(&bm));
    ASSERT_EQ(0, bitmap_count_set(&bm));
}

TEST(test_init_null_params)
{
    uint8_t buf[1];
    bitmap_t bm;
    ASSERT_EQ(BITMAP_ERR_PARAM, bitmap_init(NULL, buf, 8));
    ASSERT_EQ(BITMAP_ERR_PARAM, bitmap_init(&bm, NULL, 8));
    ASSERT_EQ(BITMAP_ERR_PARAM, bitmap_init(&bm, buf, 0));
}

TEST(test_zero_init_safety)
{
    bitmap_t bm;
    memset(&bm, 0, sizeof(bm));
    /* NULL data — should not crash, should return error/safe defaults */
    ASSERT_EQ(false, bitmap_test(&bm, 0));
    ASSERT_EQ(BITMAP_ERR_PARAM, bitmap_set(&bm, 0));
    ASSERT_EQ(-1, bitmap_find_first_set(&bm));
    ASSERT_EQ(0, bitmap_count_set(&bm));
    ASSERT_EQ(0, bitmap_size(&bm));
}

/* ---- set / clear / test ---- */

TEST(test_set_clear_test)
{
    uint8_t buf[2];
    bitmap_t bm;
    bitmap_init(&bm, buf, 16);

    ASSERT_EQ(false, bitmap_test(&bm, 0));
    ASSERT_EQ(BITMAP_OK, bitmap_set(&bm, 0));
    ASSERT_EQ(true, bitmap_test(&bm, 0));
    ASSERT_EQ(BITMAP_OK, bitmap_clear(&bm, 0));
    ASSERT_EQ(false, bitmap_test(&bm, 0));

    /* Middle bit */
    ASSERT_EQ(BITMAP_OK, bitmap_set(&bm, 10));
    ASSERT_EQ(true, bitmap_test(&bm, 10));
    ASSERT_EQ(false, bitmap_test(&bm, 9));
}

/* ---- toggle ---- */

TEST(test_toggle)
{
    uint8_t buf[1];
    bitmap_t bm;
    bitmap_init(&bm, buf, 8);

    ASSERT_EQ(false, bitmap_test(&bm, 3));
    ASSERT_EQ(BITMAP_OK, bitmap_toggle(&bm, 3));
    ASSERT_EQ(true, bitmap_test(&bm, 3));
    ASSERT_EQ(BITMAP_OK, bitmap_toggle(&bm, 3));
    ASSERT_EQ(false, bitmap_test(&bm, 3));
}

/* ---- range checks ---- */

TEST(test_range_check)
{
    uint8_t buf[1];
    bitmap_t bm;
    bitmap_init(&bm, buf, 8);

    ASSERT_EQ(BITMAP_ERR_RANGE, bitmap_set(&bm, 8));
    ASSERT_EQ(BITMAP_ERR_RANGE, bitmap_clear(&bm, 8));
    ASSERT_EQ(BITMAP_ERR_RANGE, bitmap_toggle(&bm, 8));
    ASSERT_EQ(false, bitmap_test(&bm, 8));

    ASSERT_EQ(BITMAP_ERR_RANGE, bitmap_set(&bm, 100));
}

/* ---- set_all / clear_all ---- */

TEST(test_set_all_clear_all)
{
    uint8_t buf[2];
    bitmap_t bm;
    bitmap_init(&bm, buf, 16);

    bitmap_set_all(&bm);
    ASSERT_EQ(16, bitmap_count_set(&bm));
    for (uint16_t i = 0; i < 16; i++) {
        ASSERT_EQ(true, bitmap_test(&bm, i));
    }

    bitmap_clear_all(&bm);
    ASSERT_EQ(0, bitmap_count_set(&bm));
    for (uint16_t i = 0; i < 16; i++) {
        ASSERT_EQ(false, bitmap_test(&bm, i));
    }
}

/* ---- find_first_set / find_first_clear ---- */

TEST(test_find_first_set)
{
    uint8_t buf[2];
    bitmap_t bm;
    bitmap_init(&bm, buf, 16);

    ASSERT_EQ(-1, bitmap_find_first_set(&bm));

    bitmap_set(&bm, 5);
    ASSERT_EQ(5, bitmap_find_first_set(&bm));

    bitmap_set(&bm, 2);
    ASSERT_EQ(2, bitmap_find_first_set(&bm));
}

TEST(test_find_first_clear)
{
    uint8_t buf[2];
    bitmap_t bm;
    bitmap_init(&bm, buf, 16);

    ASSERT_EQ(0, bitmap_find_first_clear(&bm));

    bitmap_set_all(&bm);
    ASSERT_EQ(-1, bitmap_find_first_clear(&bm));

    bitmap_clear(&bm, 7);
    ASSERT_EQ(7, bitmap_find_first_clear(&bm));
}

/* ---- count_set / count_clear ---- */

TEST(test_count)
{
    uint8_t buf[2];
    bitmap_t bm;
    bitmap_init(&bm, buf, 16);

    ASSERT_EQ(0, bitmap_count_set(&bm));
    ASSERT_EQ(16, bitmap_count_clear(&bm));

    bitmap_set(&bm, 0);
    bitmap_set(&bm, 5);
    bitmap_set(&bm, 15);
    ASSERT_EQ(3, bitmap_count_set(&bm));
    ASSERT_EQ(13, bitmap_count_clear(&bm));
}

/* ---- alloc ---- */

TEST(test_alloc_sequential)
{
    uint8_t buf[1];
    bitmap_t bm;
    bitmap_init(&bm, buf, 8);

    /* Alloc should return 0, 1, 2, ... in order */
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(i, bitmap_alloc(&bm));
    }
    ASSERT_EQ(8, bitmap_count_set(&bm));
}

TEST(test_alloc_full_returns_minus1)
{
    uint8_t buf[1];
    bitmap_t bm;
    bitmap_init(&bm, buf, 8);

    bitmap_set_all(&bm);
    ASSERT_EQ(-1, bitmap_alloc(&bm));
}

/* ---- free + re-alloc ---- */

TEST(test_free_and_realloc)
{
    uint8_t buf[1];
    bitmap_t bm;
    bitmap_init(&bm, buf, 8);

    /* Fill up */
    for (int i = 0; i < 8; i++) {
        bitmap_alloc(&bm);
    }
    ASSERT_EQ(-1, bitmap_alloc(&bm));

    /* Free slot 3, re-alloc should return 3 */
    ASSERT_EQ(BITMAP_OK, bitmap_free(&bm, 3));
    ASSERT_EQ(3, bitmap_alloc(&bm));
    ASSERT_EQ(-1, bitmap_alloc(&bm));
}

/* ---- 1-bit bitmap ---- */

TEST(test_1bit_bitmap)
{
    uint8_t buf[1];
    bitmap_t bm;
    bitmap_init(&bm, buf, 1);

    ASSERT_EQ(1, bitmap_size(&bm));
    ASSERT_EQ(0, bitmap_alloc(&bm));
    ASSERT_EQ(-1, bitmap_alloc(&bm));
    ASSERT_EQ(true, bitmap_test(&bm, 0));
    ASSERT_EQ(BITMAP_ERR_RANGE, bitmap_set(&bm, 1));

    bitmap_free(&bm, 0);
    ASSERT_EQ(false, bitmap_test(&bm, 0));
}

/* ---- 8-bit boundary (exact byte) ---- */

TEST(test_8bit_boundary)
{
    uint8_t buf[1];
    bitmap_t bm;
    bitmap_init(&bm, buf, 8);

    bitmap_set_all(&bm);
    ASSERT_EQ(8, bitmap_count_set(&bm));
    ASSERT_EQ(-1, bitmap_find_first_clear(&bm));

    bitmap_clear(&bm, 7);
    ASSERT_EQ(7, bitmap_find_first_clear(&bm));
    ASSERT_EQ(7, bitmap_count_set(&bm));
}

/* ---- non-byte-aligned size (13 bits) ---- */

TEST(test_non_aligned_13bits)
{
    uint8_t buf[2];  /* 13 bits needs 2 bytes */
    bitmap_t bm;
    bitmap_init(&bm, buf, 13);

    ASSERT_EQ(13, bitmap_size(&bm));

    /* set_all should only set 13 bits, not 16 */
    bitmap_set_all(&bm);
    ASSERT_EQ(13, bitmap_count_set(&bm));
    ASSERT_EQ(-1, bitmap_find_first_clear(&bm));

    /* Bits 13, 14, 15 should be out of range */
    ASSERT_EQ(BITMAP_ERR_RANGE, bitmap_set(&bm, 13));
    ASSERT_EQ(false, bitmap_test(&bm, 13));

    /* Clear and re-count */
    bitmap_clear_all(&bm);
    ASSERT_EQ(0, bitmap_count_set(&bm));
    ASSERT_EQ(13, bitmap_count_clear(&bm));

    /* Alloc all 13 slots */
    for (int i = 0; i < 13; i++) {
        ASSERT_EQ(i, bitmap_alloc(&bm));
    }
    ASSERT_EQ(-1, bitmap_alloc(&bm));
}

/* ---- bitmap full error ---- */

TEST(test_bitmap_err_full)
{
    uint8_t buf[1];
    bitmap_t bm;
    bitmap_init(&bm, buf, 8);

    /* Verify BITMAP_ERR_FULL enum value exists and equals -3 */
    ASSERT_EQ(BITMAP_ERR_FULL, -3);

    /* Fill all bits via set */
    for (uint16_t i = 0; i < 8; i++) {
        ASSERT_EQ(BITMAP_OK, bitmap_set(&bm, i));
    }

    /* Bitmap is full: no clear bits, alloc fails */
    ASSERT_EQ(bitmap_count_clear(&bm), 0);
    ASSERT_EQ(bitmap_count_set(&bm), 8);
    ASSERT_EQ(bitmap_find_first_clear(&bm), -1);
    ASSERT_EQ(bitmap_alloc(&bm), -1);

    /* Free one slot, verify no longer full */
    ASSERT_EQ(BITMAP_OK, bitmap_free(&bm, 4));
    ASSERT_EQ(bitmap_count_clear(&bm), 1);
    ASSERT_EQ(bitmap_find_first_clear(&bm), 4);
    ASSERT_EQ(bitmap_alloc(&bm), 4);

    /* Full again after re-alloc */
    ASSERT_EQ(bitmap_alloc(&bm), -1);
    ASSERT_EQ(bitmap_count_clear(&bm), 0);
}

/* ---- null bitmap to other APIs ---- */

TEST(test_null_bitmap_apis)
{
    ASSERT_EQ(BITMAP_ERR_PARAM, bitmap_set(NULL, 0));
    ASSERT_EQ(BITMAP_ERR_PARAM, bitmap_clear(NULL, 0));
    ASSERT_EQ(BITMAP_ERR_PARAM, bitmap_toggle(NULL, 0));
    ASSERT_EQ(false, bitmap_test(NULL, 0));
    ASSERT_EQ(-1, bitmap_find_first_set(NULL));
    ASSERT_EQ(-1, bitmap_find_first_clear(NULL));
    ASSERT_EQ(0, bitmap_count_set(NULL));
    ASSERT_EQ(0, bitmap_count_clear(NULL));
    ASSERT_EQ(0, bitmap_size(NULL));
    ASSERT_EQ(-1, bitmap_alloc(NULL));
    ASSERT_EQ(BITMAP_ERR_PARAM, bitmap_free(NULL, 0));
    /* set_all / clear_all with NULL should not crash */
    bitmap_set_all(NULL);
    bitmap_clear_all(NULL);
}

TEST_MAIN("bitmap")
    RUN(test_init_basic);
    RUN(test_init_null_params);
    RUN(test_zero_init_safety);
    RUN(test_set_clear_test);
    RUN(test_toggle);
    RUN(test_range_check);
    RUN(test_set_all_clear_all);
    RUN(test_find_first_set);
    RUN(test_find_first_clear);
    RUN(test_count);
    RUN(test_alloc_sequential);
    RUN(test_alloc_full_returns_minus1);
    RUN(test_free_and_realloc);
    RUN(test_1bit_bitmap);
    RUN(test_8bit_boundary);
    RUN(test_non_aligned_13bits);
    RUN(test_bitmap_err_full);
    RUN(test_null_bitmap_apis);
TEST_MAIN_END()
