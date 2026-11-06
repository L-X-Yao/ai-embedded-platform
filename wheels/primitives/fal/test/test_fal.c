// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include <fal.h>

/* ---------- test harness ---------- */
/* ---------- mock flash device ---------- */

#define MOCK_FLASH_SIZE  (64 * 1024)
#define MOCK_BLOCK_SIZE  4096

static uint8_t mock_flash_buf[MOCK_FLASH_SIZE];

static int mock_init_rc;    /* what mock_init reports */

static int mock_init(void)
{
    memset(mock_flash_buf, 0xFF, sizeof(mock_flash_buf));
    return mock_init_rc;
}

static int mock_fail_next;
static int mock_calls;      /* read, write and erase calls that reached us */

static int mock_read(long offset, uint8_t *buf, size_t size)
{
    mock_calls++;
    if (mock_fail_next) { mock_fail_next = 0; return -1; }
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) return -1;
    memcpy(buf, mock_flash_buf + offset, size);
    return (int)size;
}

static int mock_write(long offset, const uint8_t *buf, size_t size)
{
    mock_calls++;
    if (mock_fail_next) { mock_fail_next = 0; return -1; }
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) return -1;
    for (size_t i = 0; i < size; i++)
        mock_flash_buf[(size_t)offset + i] &= buf[i];
    return (int)size;
}

static int mock_erase(long offset, size_t size)
{
    mock_calls++;
    if (mock_fail_next) { mock_fail_next = 0; return -1; }
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) return -1;
    memset(mock_flash_buf + offset, 0xFF, size);
    return (int)size;
}

const struct fal_flash_dev mock_flash_dev = {
    .name      = "mock_flash",
    .addr      = 0,
    .len       = MOCK_FLASH_SIZE,
    .blk_size  = MOCK_BLOCK_SIZE,
    .ops       = {
        .init  = mock_init,
        .read  = mock_read,
        .write = mock_write,
        .erase = mock_erase,
    },
    .write_gran = 1,
};

/* ---------- tests ---------- */

/* Has to run first: nothing has called fal_init yet. */
TEST(test_partition_calls_before_init)
{
    const struct fal_partition *p = fal_partition_find("config");
    ASSERT_NOT_NULL(p);
    uint8_t buf[16] = {0};

    ASSERT_EQ(fal_init_check(), 0);
    ASSERT_EQ(fal_partition_read(p, 0, buf, sizeof(buf)), FAL_ERR_NOT_READY);
    ASSERT_EQ(fal_partition_write(p, 0, buf, sizeof(buf)), FAL_ERR_NOT_READY);
    ASSERT_EQ(fal_partition_erase(p, 0, 4096), FAL_ERR_NOT_READY);
    ASSERT_EQ(fal_partition_erase_all(p), FAL_ERR_NOT_READY);
    ASSERT_EQ(mock_calls, 0);
}

/* Has to run before the first fal_init that succeeds. */
TEST(test_init_failure_can_be_retried)
{
    const struct fal_partition *p = fal_partition_find("config");
    ASSERT_NOT_NULL(p);
    uint8_t buf[16] = {0};

    mock_init_rc = -1;
    ASSERT_EQ(fal_init(), FAL_ERR_HAL);
    ASSERT_EQ(fal_init_check(), 0);
    ASSERT_EQ(fal_partition_read(p, 0, buf, sizeof(buf)), FAL_ERR_NOT_READY);

    mock_init_rc = 0;
    ASSERT_EQ(fal_init(), 3);
    ASSERT_EQ(fal_init_check(), 1);
}

TEST(test_init)
{
    int n = fal_init();
    ASSERT_TRUE(n == 3);
    ASSERT_TRUE(fal_init_check() == 1);
}

TEST(test_find_partition)
{
    const struct fal_partition *p;

    p = fal_partition_find("config");
    ASSERT_TRUE(p != NULL);
    ASSERT_TRUE(p->len == 4096);
    ASSERT_TRUE(p->offset == 0);

    p = fal_partition_find("kvdb");
    ASSERT_TRUE(p != NULL);
    ASSERT_TRUE(p->len == 32768);

    p = fal_partition_find("firmware");
    ASSERT_TRUE(p != NULL);
    ASSERT_TRUE(p->offset == 4096 + 32768);
}

TEST(test_find_not_exist)
{
    const struct fal_partition *p = fal_partition_find("nonexistent");
    ASSERT_TRUE(p == NULL);
}

TEST(test_find_flash_device)
{
    const struct fal_flash_dev *d = fal_flash_device_find("mock_flash");
    ASSERT_TRUE(d != NULL);
    ASSERT_TRUE(d->len == MOCK_FLASH_SIZE);

    d = fal_flash_device_find("no_such_dev");
    ASSERT_TRUE(d == NULL);
}

TEST(test_get_partition_table)
{
    size_t len = 0;
    const struct fal_partition *t = fal_get_partition_table(&len);
    ASSERT_TRUE(t != NULL);
    ASSERT_TRUE(len == 3);
}

TEST(test_write_read)
{
    const struct fal_partition *p = fal_partition_find("config");

    fal_partition_erase(p, 0, p->len);

    uint8_t wbuf[] = {0xDE, 0xAD, 0xBE, 0xEF};
    int ret = fal_partition_write(p, 0, wbuf, 4);
    ASSERT_TRUE(ret == 4);

    uint8_t rbuf[4] = {0};
    ret = fal_partition_read(p, 0, rbuf, 4);
    ASSERT_TRUE(ret == 4);
    ASSERT_TRUE(memcmp(rbuf, wbuf, 4) == 0);
}

TEST(test_write_read_offset)
{
    const struct fal_partition *p = fal_partition_find("kvdb");

    fal_partition_erase(p, 0, p->len);

    uint8_t data[] = {0x01, 0x02, 0x03};
    fal_partition_write(p, 100, data, 3);

    uint8_t rbuf[3] = {0};
    fal_partition_read(p, 100, rbuf, 3);
    ASSERT_TRUE(rbuf[0] == 0x01 && rbuf[1] == 0x02 && rbuf[2] == 0x03);
}

TEST(test_erase)
{
    const struct fal_partition *p = fal_partition_find("config");

    uint8_t data[] = {0x55, 0xAA};
    fal_partition_write(p, 0, data, 2);

    fal_partition_erase(p, 0, MOCK_BLOCK_SIZE);

    uint8_t rbuf[2];
    fal_partition_read(p, 0, rbuf, 2);
    ASSERT_TRUE(rbuf[0] == 0xFF && rbuf[1] == 0xFF);
}

TEST(test_erase_all)
{
    const struct fal_partition *p = fal_partition_find("kvdb");

    uint8_t data = 0x42;
    fal_partition_write(p, 0, &data, 1);
    fal_partition_write(p, (uint32_t)(p->len - 1u), &data, 1);

    fal_partition_erase_all(p);

    uint8_t r1, r2;
    fal_partition_read(p, 0, &r1, 1);
    fal_partition_read(p, (uint32_t)(p->len - 1u), &r2, 1);
    ASSERT_TRUE(r1 == 0xFF && r2 == 0xFF);
}

TEST(test_out_of_bounds)
{
    const struct fal_partition *p = fal_partition_find("config");
    uint8_t buf[8];

    int ret = fal_partition_read(p, (uint32_t)(p->len - 2u), buf, 8);
    ASSERT_TRUE(ret == FAL_ERR_OVERFLOW);

    ret = fal_partition_write(p, (uint32_t)p->len, buf, 1);
    ASSERT_TRUE(ret == FAL_ERR_OVERFLOW);

    ret = fal_partition_erase(p, (uint32_t)(p->len - 10u), 20);
    ASSERT_TRUE(ret == FAL_ERR_OVERFLOW);
}

TEST(test_partition_isolation)
{
    const struct fal_partition *config = fal_partition_find("config");
    const struct fal_partition *kvdb = fal_partition_find("kvdb");

    fal_partition_erase_all(config);
    fal_partition_erase_all(kvdb);

    uint8_t data = 0xAB;
    fal_partition_write(config, (uint32_t)(config->len - 1u), &data, 1);

    uint8_t rbuf;
    fal_partition_read(kvdb, 0, &rbuf, 1);
    ASSERT_TRUE(rbuf == 0xFF);
}

TEST(test_flash_write_semantics)
{
    const struct fal_partition *p = fal_partition_find("config");

    fal_partition_erase(p, 0, MOCK_BLOCK_SIZE);

    uint8_t w1 = 0xF0;
    fal_partition_write(p, 0, &w1, 1);

    uint8_t w2 = 0x0F;
    fal_partition_write(p, 0, &w2, 1);

    uint8_t r;
    fal_partition_read(p, 0, &r, 1);
    ASSERT_TRUE(r == 0x00);
}

TEST(test_show_part_table)
{
    fal_show_part_table();
    ASSERT_TRUE(1);
}

TEST(test_err_param)
{
    uint8_t buf[4] = {0};
    /* NULL part triggers FAL_ERR_PARAM */
    ASSERT_TRUE(fal_partition_read(NULL, 0, buf, 4)        == FAL_ERR_PARAM);
    ASSERT_TRUE(fal_partition_write(NULL, 0, buf, 4)       == FAL_ERR_PARAM);
    ASSERT_TRUE(fal_partition_erase(NULL, 0, 4)            == FAL_ERR_PARAM);
    ASSERT_TRUE(fal_partition_erase_all(NULL)              == FAL_ERR_PARAM);

    const struct fal_partition *p = fal_partition_find("config");
    /* NULL buf triggers FAL_ERR_PARAM */
    ASSERT_TRUE(fal_partition_read(p, 0, NULL, 4)          == FAL_ERR_PARAM);
    ASSERT_TRUE(fal_partition_write(p, 0, NULL, 4)         == FAL_ERR_PARAM);
}

TEST(test_err_not_ready)
{
    /* Partition pointing at a flash device that does not exist. */
    static const struct fal_partition orphan = {
        .name       = "orphan",
        .flash_name = "no_such_flash",
        .offset     = 0,
        .len        = 4096,
    };
    uint8_t buf[4] = {0};
    ASSERT_TRUE(fal_partition_read(&orphan, 0, buf, 4)     == FAL_ERR_NOT_READY);
    ASSERT_TRUE(fal_partition_write(&orphan, 0, buf, 4)    == FAL_ERR_NOT_READY);
    ASSERT_TRUE(fal_partition_erase(&orphan, 0, 4)         == FAL_ERR_NOT_READY);
}

TEST(test_err_hal)
{
    const struct fal_partition *p = fal_partition_find("config");
    ASSERT_NOT_NULL(p);
    uint8_t buf[4] = {0};

    mock_fail_next = 1;
    ASSERT_EQ(fal_partition_read(p, 0, buf, 4), FAL_ERR_HAL);

    mock_fail_next = 1;
    ASSERT_EQ(fal_partition_write(p, 0, buf, 4), FAL_ERR_HAL);

    mock_fail_next = 1;
    ASSERT_EQ(fal_partition_erase(p, 0, 4096), FAL_ERR_HAL);
}

/* ---------- main ---------- */

TEST_MAIN("fal")
    RUN(test_partition_calls_before_init);
    RUN(test_init_failure_can_be_retried);
    RUN(test_init);
    RUN(test_find_partition);
    RUN(test_find_not_exist);
    RUN(test_find_flash_device);
    RUN(test_get_partition_table);
    RUN(test_write_read);
    RUN(test_write_read_offset);
    RUN(test_erase);
    RUN(test_erase_all);
    RUN(test_out_of_bounds);
    RUN(test_partition_isolation);
    RUN(test_flash_write_semantics);
    RUN(test_show_part_table);
    RUN(test_err_param);
    RUN(test_err_not_ready);
    RUN(test_err_hal);
TEST_MAIN_END()

