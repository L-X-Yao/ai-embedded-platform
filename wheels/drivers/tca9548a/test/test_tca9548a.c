// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "tca9548a.h"

/* ---------- chip-specific mock setup ---------- */

/* The chip has one control byte and no register numbers (SCPS207H p16):
 * mock_regs[0x00] holds it, and every byte written lands there.         */
#define BUS_LOG_MAX 4

static uint8_t  bus_addr;
static uint16_t bus_write_len;
static uint8_t  bus_write[BUS_LOG_MAX];

static void mock_reset(void)
{
    mock_i2c_reset();
    bus_addr = 0;
    bus_write_len = 0;
    memset(bus_write, 0, sizeof(bus_write));
}

static bool mock_write_raw(void *ctx, uint8_t dev_addr,
                           const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (mock_i2c_should_fail()) return false;
    mock_write_count++;
    bus_addr = dev_addr;
    bus_write_len = len;
    for (uint16_t i = 0; i < len; i++) {
        if (i < BUS_LOG_MAX) bus_write[i] = data[i];
        mock_regs[0x00] = data[i];
    }
    return true;
}

static bool mock_read_raw(void *ctx, uint8_t dev_addr,
                          uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (mock_i2c_should_fail()) return false;
    mock_read_count++;
    bus_addr = dev_addr;
    for (uint16_t i = 0; i < len; i++) data[i] = mock_regs[0x00];
    return true;
}

static const tca9548a_i2c_t mock_i2c = {
    .ctx       = NULL,
    .write_raw = mock_write_raw,
    .read_raw  = mock_read_raw,
};

static tca9548a_t dev;

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(tca9548a_init(&dev, &mock_i2c, 0x70), TCA9548A_OK);
    /* Init should disable all channels */
    ASSERT_EQ(dev.current_channel, 0x00);
    ASSERT_EQ(dev.dev_addr, 0x70);
}

TEST(test_init_null_dev)
{
    mock_reset();
    ASSERT_EQ(tca9548a_init(NULL, &mock_i2c, 0x70), TCA9548A_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    mock_reset();
    ASSERT_EQ(tca9548a_init(&dev, NULL, 0x70), TCA9548A_ERR_PARAM);
}

TEST(test_init_addr_too_low)
{
    mock_reset();
    ASSERT_EQ(tca9548a_init(&dev, &mock_i2c, 0x6F), TCA9548A_ERR_PARAM);
}

TEST(test_init_addr_too_high)
{
    mock_reset();
    ASSERT_EQ(tca9548a_init(&dev, &mock_i2c, 0x78), TCA9548A_ERR_PARAM);
}

TEST(test_init_all_valid_addrs)
{
    /* 0x70 .. 0x77 should all succeed */
    for (uint8_t a = 0x70; a <= 0x77; a++) {
        mock_reset();
        ASSERT_EQ(tca9548a_init(&dev, &mock_i2c, a), TCA9548A_OK);
    }
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;  /* fail the disable-all write during init */
    ASSERT_EQ(tca9548a_init(&dev, &mock_i2c, 0x70), TCA9548A_ERR_I2C);
}

TEST(test_init_null_write_raw)
{
    static const tca9548a_i2c_t hal = { .read_raw = mock_read_raw };
    mock_reset();
    ASSERT_EQ(tca9548a_init(&dev, &hal, 0x70), TCA9548A_ERR_PARAM);
    ASSERT_EQ(mock_op_count, 0);
}

TEST(test_init_refuses_a_register_hal)
{
    /* No fallback: write_reg would send a stray byte ahead of the mask */
    static const tca9548a_i2c_t hal = {
        .write_reg = mock_i2c_write_reg,
        .read_regs = mock_i2c_read_regs,
    };
    mock_reset();
    ASSERT_EQ(tca9548a_init(&dev, &hal, 0x70), TCA9548A_ERR_PARAM);
    ASSERT_EQ(mock_op_count, 0);
}

/* ---------- select tests ---------- */

TEST(test_select_sends_the_mask_alone)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x71);
    ASSERT_EQ(bus_write_len, 1);
    ASSERT_EQ(bus_write[0], 0x00);

    ASSERT_EQ(tca9548a_select(&dev, 6), TCA9548A_OK);
    ASSERT_EQ(bus_addr, 0x71);
    ASSERT_EQ(bus_write_len, 1);
    ASSERT_EQ(bus_write[0], 0x40);
}

TEST(test_select_channel_0)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    ASSERT_EQ(tca9548a_select(&dev, 0), TCA9548A_OK);
    ASSERT_EQ(dev.current_channel, 0x01);
    ASSERT_EQ(mock_regs[0x00], 0x01);
}

TEST(test_select_channel_7)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    ASSERT_EQ(tca9548a_select(&dev, 7), TCA9548A_OK);
    ASSERT_EQ(dev.current_channel, 0x80);
    ASSERT_EQ(mock_regs[0x00], 0x80);
}

TEST(test_select_channel_invalid)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    ASSERT_EQ(tca9548a_select(&dev, 8), TCA9548A_ERR_PARAM);
    /* Channel should remain at 0 from init */
    ASSERT_EQ(dev.current_channel, 0x00);
}

TEST(test_select_null_dev)
{
    ASSERT_EQ(tca9548a_select(NULL, 0), TCA9548A_ERR_PARAM);
}

TEST(test_select_cached_skip)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    tca9548a_select(&dev, 3);
    int writes_before = mock_write_count;
    /* Selecting same channel again should skip the I2C write */
    ASSERT_EQ(tca9548a_select(&dev, 3), TCA9548A_OK);
    ASSERT_EQ(mock_write_count, writes_before);
}

TEST(test_select_i2c_fail)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    mock_fail_next = 1;
    ASSERT_EQ(tca9548a_select(&dev, 2), TCA9548A_ERR_I2C);
    /* Cache should NOT update on failure */
    ASSERT_EQ(dev.current_channel, 0x00);
}

/* ---------- select_multi tests ---------- */

TEST(test_select_multi_ok)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    /* Enable channels 0, 2, 5 simultaneously */
    ASSERT_EQ(tca9548a_select_multi(&dev, 0x25), TCA9548A_OK);
    ASSERT_EQ(dev.current_channel, 0x25);
    ASSERT_EQ(mock_regs[0x00], 0x25);
}

TEST(test_select_multi_all)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    ASSERT_EQ(tca9548a_select_multi(&dev, 0xFF), TCA9548A_OK);
    ASSERT_EQ(dev.current_channel, 0xFF);
}

TEST(test_select_multi_cached_skip)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    tca9548a_select_multi(&dev, 0x0F);
    int writes_before = mock_write_count;
    ASSERT_EQ(tca9548a_select_multi(&dev, 0x0F), TCA9548A_OK);
    ASSERT_EQ(mock_write_count, writes_before);
}

TEST(test_select_multi_null)
{
    ASSERT_EQ(tca9548a_select_multi(NULL, 0x01), TCA9548A_ERR_PARAM);
}

/* ---------- disable_all tests ---------- */

TEST(test_disable_all)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    tca9548a_select(&dev, 4);
    ASSERT_EQ(tca9548a_disable_all(&dev), TCA9548A_OK);
    ASSERT_EQ(dev.current_channel, 0x00);
    ASSERT_EQ(mock_regs[0x00], 0x00);
}

TEST(test_disable_all_already_disabled)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    int writes_before = mock_write_count;
    /* Already at 0x00 from init — should skip write */
    ASSERT_EQ(tca9548a_disable_all(&dev), TCA9548A_OK);
    ASSERT_EQ(mock_write_count, writes_before);
}

TEST(test_disable_all_null)
{
    ASSERT_EQ(tca9548a_disable_all(NULL), TCA9548A_ERR_PARAM);
}

/* ---------- get_channel tests ---------- */

TEST(test_get_channel_ok)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    tca9548a_select(&dev, 6);

    uint8_t mask = 0;
    ASSERT_EQ(tca9548a_get_channel(&dev, &mask), TCA9548A_OK);
    /* mock_regs[0] was set to 0x40 by select(6) */
    ASSERT_EQ(mask, 0x40);
}

TEST(test_get_channel_writes_nothing)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x72);
    tca9548a_select_multi(&dev, 0xA5);
    int writes_before = mock_write_count;

    uint8_t mask = 0;
    ASSERT_EQ(tca9548a_get_channel(&dev, &mask), TCA9548A_OK);
    ASSERT_EQ(mask, 0xA5);
    ASSERT_EQ(bus_addr, 0x72);
    ASSERT_EQ(mock_write_count, writes_before);
    ASSERT_EQ(mock_regs[0x00], 0xA5);
}

TEST(test_get_channel_null_read_raw)
{
    static const tca9548a_i2c_t hal = { .write_raw = mock_write_raw };
    mock_reset();
    ASSERT_EQ(tca9548a_init(&dev, &hal, 0x70), TCA9548A_OK);
    ASSERT_EQ(tca9548a_select(&dev, 2), TCA9548A_OK);
    int ops_before = mock_op_count;

    uint8_t mask = 0x5A;
    ASSERT_EQ(tca9548a_get_channel(&dev, &mask), TCA9548A_ERR_PARAM);
    ASSERT_EQ(mock_op_count, ops_before);
    ASSERT_EQ(mask, 0x5A);
    ASSERT_EQ(dev.current_channel, 0x04);
}

TEST(test_get_channel_resyncs_cache)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);

    /* Simulate external change: poke mock register directly */
    mock_regs[0x00] = 0x03;

    uint8_t mask = 0;
    ASSERT_EQ(tca9548a_get_channel(&dev, &mask), TCA9548A_OK);
    ASSERT_EQ(mask, 0x03);
    /* Cache should be re-synced */
    ASSERT_EQ(dev.current_channel, 0x03);
}

TEST(test_get_channel_i2c_fail)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    mock_fail_next = 1;
    uint8_t mask = 0xFF;
    ASSERT_EQ(tca9548a_get_channel(&dev, &mask), TCA9548A_ERR_I2C);
    /* mask should not be changed on failure */
    ASSERT_EQ(mask, 0xFF);
}

TEST(test_get_channel_null_dev)
{
    uint8_t mask = 0;
    ASSERT_EQ(tca9548a_get_channel(NULL, &mask), TCA9548A_ERR_PARAM);
}

TEST(test_get_channel_null_mask)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x70);
    ASSERT_EQ(tca9548a_get_channel(&dev, NULL), TCA9548A_ERR_PARAM);
}

/* ---------- combined workflow ---------- */

TEST(test_workflow_select_read_disable)
{
    mock_reset();
    tca9548a_init(&dev, &mock_i2c, 0x71);

    /* Select ch3, verify, then disable */
    ASSERT_EQ(tca9548a_select(&dev, 3), TCA9548A_OK);

    uint8_t mask = 0;
    ASSERT_EQ(tca9548a_get_channel(&dev, &mask), TCA9548A_OK);
    ASSERT_EQ(mask, 0x08);

    ASSERT_EQ(tca9548a_disable_all(&dev), TCA9548A_OK);
    ASSERT_EQ(tca9548a_get_channel(&dev, &mask), TCA9548A_OK);
    ASSERT_EQ(mask, 0x00);
}

/* ---------- main ---------- */

TEST_MAIN("tca9548a")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_addr_too_low);
    RUN(test_init_addr_too_high);
    RUN(test_init_all_valid_addrs);
    RUN(test_init_i2c_fail);
    RUN(test_init_null_write_raw);
    RUN(test_init_refuses_a_register_hal);
    RUN(test_select_sends_the_mask_alone);
    RUN(test_select_channel_0);
    RUN(test_select_channel_7);
    RUN(test_select_channel_invalid);
    RUN(test_select_null_dev);
    RUN(test_select_cached_skip);
    RUN(test_select_i2c_fail);
    RUN(test_select_multi_ok);
    RUN(test_select_multi_all);
    RUN(test_select_multi_cached_skip);
    RUN(test_select_multi_null);
    RUN(test_disable_all);
    RUN(test_disable_all_already_disabled);
    RUN(test_disable_all_null);
    RUN(test_get_channel_ok);
    RUN(test_get_channel_writes_nothing);
    RUN(test_get_channel_null_read_raw);
    RUN(test_get_channel_resyncs_cache);
    RUN(test_get_channel_i2c_fail);
    RUN(test_get_channel_null_dev);
    RUN(test_get_channel_null_mask);
    RUN(test_workflow_select_read_disable);
TEST_MAIN_END()
