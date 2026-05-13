#include "test_harness.h"
#include "hw_model_bmm150.h"
#include "bmm150.h"
#include <string.h>

static const bmm150_i2c_t mock_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

static const bmm150_cfg_t default_cfg = { .addr = BMM150_ADDR_DEFAULT };
static bmm150_t dev;

TEST(test_init_ok)
{
    hw_model_reset();
    ASSERT_EQ(bmm150_init(&dev, &mock_i2c, &default_cfg), BMM150_OK);
}

TEST(test_init_null)
{
    ASSERT_EQ(bmm150_init(NULL, &mock_i2c, &default_cfg), BMM150_ERR_PARAM);
    ASSERT_EQ(bmm150_init(&dev, NULL, &default_cfg), BMM150_ERR_PARAM);
    ASSERT_EQ(bmm150_init(&dev, &mock_i2c, NULL), BMM150_ERR_PARAM);
}

TEST(test_init_zero_hal)
{
    bmm150_i2c_t zero = {0};
    ASSERT_TRUE(bmm150_init(&dev, &zero, &default_cfg) < 0);
}

TEST(test_init_wrong_id)
{
    hw_model_reset();
    hw_regs[0x40] = 0xFF;
    ASSERT_EQ(bmm150_init(&dev, &mock_i2c, &default_cfg), BMM150_ERR_ID);
}

TEST(test_init_powers_on)
{
    hw_model_reset();
    bmm150_init(&dev, &mock_i2c, &default_cfg);
    ASSERT_TRUE(hw_regs[0x4B] & 0x01);
}

TEST(test_error_codes)
{
    ASSERT_EQ((int)BMM150_OK, 0);
    ASSERT_EQ((int)BMM150_ERR_I2C, -1);
    ASSERT_EQ((int)BMM150_ERR_ID, -2);
    ASSERT_EQ((int)BMM150_ERR_PARAM, -3);
}

TEST(test_read_zero)
{
    hw_model_reset();
    bmm150_init(&dev, &mock_i2c, &default_cfg);
    hw_model_inject_mag(0.0f, 0.0f, 0.0f);
    bmm150_data_t data;
    ASSERT_EQ(bmm150_read(&dev, &data), BMM150_OK);
    ASSERT_NEAR(data.x_ut, 0.0f, 0.5f);
    ASSERT_NEAR(data.y_ut, 0.0f, 0.5f);
    ASSERT_NEAR(data.z_ut, 0.0f, 0.5f);
}

TEST(test_read_earth_field)
{
    hw_model_reset();
    bmm150_init(&dev, &mock_i2c, &default_cfg);
    hw_model_inject_mag(25.0f, 0.0f, 45.0f);
    bmm150_data_t data;
    ASSERT_EQ(bmm150_read(&dev, &data), BMM150_OK);
    ASSERT_NEAR(data.x_ut, 25.0f, 1.0f);
    ASSERT_NEAR(data.z_ut, 45.0f, 1.0f);
}

TEST(test_read_negative)
{
    hw_model_reset();
    bmm150_init(&dev, &mock_i2c, &default_cfg);
    hw_model_inject_mag(-30.0f, -20.0f, -50.0f);
    bmm150_data_t data;
    bmm150_read(&dev, &data);
    ASSERT_NEAR(data.x_ut, -30.0f, 1.0f);
    ASSERT_NEAR(data.y_ut, -20.0f, 1.0f);
    ASSERT_NEAR(data.z_ut, -50.0f, 1.0f);
}

TEST(test_read_null)
{
    ASSERT_EQ(bmm150_read(NULL, &(bmm150_data_t){0}), BMM150_ERR_PARAM);
    hw_model_reset();
    bmm150_init(&dev, &mock_i2c, &default_cfg);
    ASSERT_EQ(bmm150_read(&dev, NULL), BMM150_ERR_PARAM);
}

TEST(test_reset_ok)
{
    hw_model_reset();
    bmm150_init(&dev, &mock_i2c, &default_cfg);
    ASSERT_EQ(bmm150_reset(&dev), BMM150_OK);
    ASSERT_TRUE(hw_regs[0x4B] & 0x82);
}

TEST(test_reset_null)
{
    ASSERT_EQ(bmm150_reset(NULL), BMM150_ERR_PARAM);
}

TEST_MAIN("bmm150")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_init_zero_hal);
    RUN(test_init_wrong_id);
    RUN(test_init_powers_on);
    RUN(test_error_codes);
    RUN(test_read_zero);
    RUN(test_read_earth_field);
    RUN(test_read_negative);
    RUN(test_read_null);
    RUN(test_reset_ok);
    RUN(test_reset_null);
TEST_MAIN_END()
