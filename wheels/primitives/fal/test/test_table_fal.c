// SPDX-License-Identifier: MIT
/*
 * test_table_fal.c -- fal_init against a partition table that does not fit
 *
 * The table is fixed at compile time, so CMake builds this file once per
 * TABLE_* in bad_table/fal_cfg.h.
 */

#include "test_harness.h"
#include <fal.h>

static int mock_init_calls;
static int mock_write_calls;

static int mock_init(void)
{
    mock_init_calls++;
    return 0;
}

static int mock_read(long offset, uint8_t *buf, size_t size)
{
    (void)offset;
    (void)buf;
    return (int)size;
}

static int mock_write(long offset, const uint8_t *buf, size_t size)
{
    (void)offset;
    (void)buf;
    mock_write_calls++;
    return (int)size;
}

static int mock_erase(long offset, size_t size)
{
    (void)offset;
    return (int)size;
}

const struct fal_flash_dev mock_flash_dev = {
    .name       = "mock_flash",
    .addr       = 0,
    .len        = 0x10000,
    .blk_size   = 4096,
    .ops        = {
        .init  = mock_init,
        .read  = mock_read,
        .write = mock_write,
        .erase = mock_erase,
    },
    .write_gran = 1,
};

#ifdef TABLE_UNKNOWN_DEVICE

/* No device to measure against: init goes ahead, and the partition's own
 * calls say the device is missing. */
TEST(test_table)
{
    const struct fal_partition *bad = fal_partition_find("bad");
    const struct fal_partition *good = fal_partition_find("good");
    uint8_t buf[16] = {0};
    ASSERT_NOT_NULL(bad);
    ASSERT_NOT_NULL(good);

    ASSERT_EQ(fal_init(), 2);
    ASSERT_EQ(fal_init_check(), 1);
    ASSERT_EQ(fal_partition_write(bad, 0, buf, sizeof(buf)), FAL_ERR_NOT_READY);
    ASSERT_EQ(fal_partition_write(good, 0, buf, sizeof(buf)), (int)sizeof(buf));
}

#else

/* Refused before any device is touched, and nothing works afterwards. */
TEST(test_table)
{
    const struct fal_partition *bad = fal_partition_find("bad");
    const struct fal_partition *good = fal_partition_find("good");
    uint8_t buf[16] = {0};
    ASSERT_NOT_NULL(bad);
    ASSERT_NOT_NULL(good);

    ASSERT_EQ(fal_init(), FAL_ERR_PART_TABLE);
    ASSERT_EQ(fal_init_check(), 0);
    ASSERT_EQ(mock_init_calls, 0);

    ASSERT_EQ(fal_partition_write(bad, 0, buf, sizeof(buf)), FAL_ERR_NOT_READY);
    ASSERT_EQ(fal_partition_write(good, 0, buf, sizeof(buf)), FAL_ERR_NOT_READY);
    ASSERT_EQ(mock_write_calls, 0);
}

#endif

TEST_MAIN("fal table")
    RUN(test_table);
TEST_MAIN_END()
