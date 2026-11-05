// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_bmm150.h"
#include "bmm150.h"

static void no_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    (void)ms;
}

static const bmm150_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = no_delay,
    .ctx       = NULL,
};

static const bmm150_cfg_t cfg = { .addr = BMM150_ADDR_DEFAULT };
static bmm150_t dev;

TEST(test_hw_init_por)
{
    hw_model_reset();
    ASSERT_EQ(bmm150_init(&dev, &hw_i2c, &cfg), BMM150_OK);
}

TEST(test_hw_roundtrip)
{
    hw_model_reset();
    bmm150_init(&dev, &hw_i2c, &cfg);
    hw_model_inject_mag(30.0f, -15.0f, 45.0f);
    bmm150_data_t data;
    bmm150_read(&dev, &data);
    ASSERT_NEAR(data.x_ut, 30.0f, 1.0f);
    ASSERT_NEAR(data.y_ut, -15.0f, 1.0f);
    ASSERT_NEAR(data.z_ut, 45.0f, 1.0f);
}

TEST_MAIN("bmm150_hw_model")
    RUN(test_hw_init_por);
    RUN(test_hw_roundtrip);
TEST_MAIN_END()
