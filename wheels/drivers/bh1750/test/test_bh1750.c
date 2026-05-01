#include "test_harness.h"
#include "bh1750.h"
#include <string.h>
#include <math.h>

static uint8_t mock_cmd_log[16];
static int mock_cmd_count;
static uint16_t mock_raw_value;
static bool mock_i2c_fail;

static bool mock_write(void *ctx, uint8_t dev_addr,
                       const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_fail) return false;
    if (len == 1 && mock_cmd_count < 16)
        mock_cmd_log[mock_cmd_count++] = data[0];
    return true;
}

static bool mock_read(void *ctx, uint8_t dev_addr,
                      uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_fail) return false;
    if (len == 2) {
        data[0] = (uint8_t)(mock_raw_value >> 8);
        data[1] = (uint8_t)(mock_raw_value & 0xFF);
    }
    return true;
}

static bh1750_i2c_t mock_i2c = {
    .write = mock_write,
    .read  = mock_read,
    .ctx   = NULL
};

static void mock_reset(void)
{
    mock_cmd_count = 0;
    memset(mock_cmd_log, 0, sizeof(mock_cmd_log));
    mock_raw_value = 0;
    mock_i2c_fail = false;
}

TEST(test_init_null)
{
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    ASSERT_EQ(bh1750_init(NULL, &mock_i2c, &cfg), BH1750_ERR_PARAM);
    bh1750_t dev;
    ASSERT_EQ(bh1750_init(&dev, NULL, &cfg), BH1750_ERR_PARAM);
    ASSERT_EQ(bh1750_init(&dev, &mock_i2c, NULL), BH1750_ERR_PARAM);
}

TEST(test_init_sends_power_on_and_mode)
{
    mock_reset();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    ASSERT_EQ(bh1750_init(&dev, &mock_i2c, &cfg), BH1750_OK);
    ASSERT_EQ(mock_cmd_count, 2);
    ASSERT_EQ(mock_cmd_log[0], 0x01);
    ASSERT_EQ(mock_cmd_log[1], 0x10);
}

TEST(test_read_lux_h_res)
{
    mock_reset();
    mock_raw_value = 1200;
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    ASSERT_EQ(bh1750_init(&dev, &mock_i2c, &cfg), BH1750_OK);
    float lux;
    ASSERT_EQ(bh1750_read_lux(&dev, &lux), BH1750_OK);
    ASSERT_NEAR(lux, 1000.0f, 1.0f);
}

TEST(test_read_lux_h_res2)
{
    mock_reset();
    mock_raw_value = 1200;
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES2 };
    ASSERT_EQ(bh1750_init(&dev, &mock_i2c, &cfg), BH1750_OK);
    float lux;
    ASSERT_EQ(bh1750_read_lux(&dev, &lux), BH1750_OK);
    ASSERT_NEAR(lux, 500.0f, 1.0f);
}

TEST(test_read_lux_zero)
{
    mock_reset();
    mock_raw_value = 0;
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    ASSERT_EQ(bh1750_init(&dev, &mock_i2c, &cfg), BH1750_OK);
    float lux;
    ASSERT_EQ(bh1750_read_lux(&dev, &lux), BH1750_OK);
    ASSERT_NEAR(lux, 0.0f, 0.01f);
}

TEST(test_i2c_failure)
{
    mock_reset();
    mock_i2c_fail = true;
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    ASSERT_EQ(bh1750_init(&dev, &mock_i2c, &cfg), BH1750_ERR_I2C);
}

TEST(test_power_off_on)
{
    mock_reset();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    ASSERT_EQ(bh1750_init(&dev, &mock_i2c, &cfg), BH1750_OK);
    mock_cmd_count = 0;
    ASSERT_EQ(bh1750_power_off(&dev), BH1750_OK);
    ASSERT_EQ(mock_cmd_log[0], 0x00);
    ASSERT_EQ(bh1750_power_on(&dev), BH1750_OK);
    ASSERT_EQ(mock_cmd_log[1], 0x01);
}

TEST(test_read_null)
{
    bh1750_t dev = {0};
    ASSERT_EQ(bh1750_read_lux(&dev, NULL), BH1750_ERR_PARAM);
    ASSERT_EQ(bh1750_read_lux(NULL, NULL), BH1750_ERR_PARAM);
}

TEST(test_reset)
{
    mock_reset();
    bh1750_t dev;
    bh1750_cfg_t cfg = { .addr = BH1750_ADDR_LOW, .mode = BH1750_CONT_H_RES };
    bh1750_init(&dev, &mock_i2c, &cfg);
    mock_cmd_count = 0;
    ASSERT_EQ(bh1750_reset(&dev), BH1750_OK);
    ASSERT_EQ(mock_cmd_log[0], 0x07);
    ASSERT_EQ(bh1750_reset(NULL), BH1750_ERR_PARAM);
}

TEST_MAIN("bh1750")
    RUN(test_init_null);
    RUN(test_init_sends_power_on_and_mode);
    RUN(test_read_lux_h_res);
    RUN(test_read_lux_h_res2);
    RUN(test_read_lux_zero);
    RUN(test_i2c_failure);
    RUN(test_power_off_on);
    RUN(test_read_null);
    RUN(test_reset);
TEST_MAIN_END()
