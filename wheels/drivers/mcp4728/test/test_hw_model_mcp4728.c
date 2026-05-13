/*
 * test_hw_model_mcp4728.c — tests using hardware model
 *
 * Validates MCP4728 driver against a datasheet-accurate model.
 * The model enforces fast write command byte encoding and channel
 * mapping, rejecting malformed protocol bytes.
 */

#include "test_harness.h"
#include "hw_model_mcp4728.h"
#include "../mcp4728.h"

static mcp4728_model_t model;

static const mcp4728_i2c_t model_i2c = {
    .write_regs = mcp4728_model_write_regs,
    .read_regs  = mcp4728_model_read_regs,
    .ctx        = &model,
};

static mcp4728_t dev;

static const mcp4728_cfg_t default_cfg = {
    .addr = MCP4728_ADDR_DEFAULT,
};

static void setup(void)
{
    mcp4728_model_init(&model);
    mcp4728_init(&dev, &model_i2c, &default_cfg);
}

/* --- init --- */

TEST(test_hw_init_ok)
{
    mcp4728_model_init(&model);
    mcp4728_t d;
    ASSERT_EQ(mcp4728_init(&d, &model_i2c, &default_cfg), MCP4728_OK);
    /* No I2C traffic on init (DAC has no chip ID) */
    ASSERT_EQ(model.write_count, 0);
    ASSERT_EQ(model.read_count, 0);
}

/* --- single channel writes --- */

TEST(test_hw_write_channel_a_zero)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 0), MCP4728_OK);
    ASSERT_EQ(model.channel_value[0], 0);
}

TEST(test_hw_write_channel_a_midscale)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 2048), MCP4728_OK);
    ASSERT_EQ(model.channel_value[0], 2048);
}

TEST(test_hw_write_channel_a_max)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 4095), MCP4728_OK);
    ASSERT_EQ(model.channel_value[0], 4095);
}

TEST(test_hw_write_channel_b)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 1, 1500), MCP4728_OK);
    ASSERT_EQ(model.channel_value[1], 1500);
    /* Channel A should remain at POR default (0) */
    ASSERT_EQ(model.channel_value[0], 0);
}

TEST(test_hw_write_channel_c)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 2, 3000), MCP4728_OK);
    ASSERT_EQ(model.channel_value[2], 3000);
}

TEST(test_hw_write_channel_d)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 3, 4000), MCP4728_OK);
    ASSERT_EQ(model.channel_value[3], 4000);
}

/* --- write_all --- */

TEST(test_hw_write_all_varied)
{
    setup();
    const uint16_t vals[4] = {0, 1000, 2500, 4095};
    ASSERT_EQ(mcp4728_write_all(&dev, vals), MCP4728_OK);
    ASSERT_EQ(model.channel_value[0], 0);
    ASSERT_EQ(model.channel_value[1], 1000);
    ASSERT_EQ(model.channel_value[2], 2500);
    ASSERT_EQ(model.channel_value[3], 4095);
    ASSERT_EQ(model.write_count, 4);
}

TEST(test_hw_write_all_then_overwrite_one)
{
    setup();
    const uint16_t vals[4] = {100, 200, 300, 400};
    mcp4728_write_all(&dev, vals);

    mcp4728_write_channel(&dev, 2, 999);

    ASSERT_EQ(model.channel_value[0], 100);
    ASSERT_EQ(model.channel_value[1], 200);
    ASSERT_EQ(model.channel_value[2], 999);
    ASSERT_EQ(model.channel_value[3], 400);
}

/* --- edge cases --- */

TEST(test_hw_sequential_writes_same_channel)
{
    setup();
    for (uint16_t v = 0; v < 4096; v += 500) {
        mcp4728_write_channel(&dev, 0, v);
        ASSERT_EQ(model.channel_value[0], v);
    }
}

TEST(test_hw_all_channels_max)
{
    setup();
    const uint16_t vals[4] = {4095, 4095, 4095, 4095};
    mcp4728_write_all(&dev, vals);
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(model.channel_value[i], 4095);
    }
}

TEST(test_hw_i2c_fail_injection)
{
    setup();
    model.fail_next = true;
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 100), MCP4728_ERR_I2C);
    /* Value should not have been written */
    ASSERT_EQ(model.channel_value[0], 0);
}

TEST_MAIN("mcp4728 hardware model")
    RUN(test_hw_init_ok);
    RUN(test_hw_write_channel_a_zero);
    RUN(test_hw_write_channel_a_midscale);
    RUN(test_hw_write_channel_a_max);
    RUN(test_hw_write_channel_b);
    RUN(test_hw_write_channel_c);
    RUN(test_hw_write_channel_d);
    RUN(test_hw_write_all_varied);
    RUN(test_hw_write_all_then_overwrite_one);
    RUN(test_hw_sequential_writes_same_channel);
    RUN(test_hw_all_channels_max);
    RUN(test_hw_i2c_fail_injection);
TEST_MAIN_END()
