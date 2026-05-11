// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "adxl345.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x00] = 0xE5;   /* DEVID = ADXL345 chip ID */
}

static const adxl345_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

static adxl345_t dev;
static adxl345_cfg_t default_cfg = {
    .addr  = ADXL345_DEV_ADDR,
    .range = ADXL345_RANGE_2G,
};

static void setup(void)
{
    mock_reset();
    adxl345_init(&dev, &mock_i2c, &default_cfg);
}

/* Plant raw acceleration data into mock registers (little-endian) */
static void plant_accel(int16_t x, int16_t y, int16_t z)
{
    mock_regs[0x32] = (uint8_t)(x & 0xFF);
    mock_regs[0x33] = (uint8_t)((x >> 8) & 0xFF);
    mock_regs[0x34] = (uint8_t)(y & 0xFF);
    mock_regs[0x35] = (uint8_t)((y >> 8) & 0xFF);
    mock_regs[0x36] = (uint8_t)(z & 0xFF);
    mock_regs[0x37] = (uint8_t)((z >> 8) & 0xFF);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    adxl345_t d;
    adxl345_err_t e = adxl345_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, ADXL345_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x00] = 0x00;
    adxl345_t d;
    adxl345_err_t e = adxl345_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, ADXL345_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    adxl345_t d;
    adxl345_err_t e = adxl345_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, ADXL345_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(adxl345_init(NULL, &mock_i2c, &default_cfg), ADXL345_ERR_PARAM);
    adxl345_t d;
    ASSERT_EQ(adxl345_init(&d, NULL, &default_cfg), ADXL345_ERR_PARAM);
    ASSERT_EQ(adxl345_init(&d, &mock_i2c, NULL), ADXL345_ERR_PARAM);
    adxl345_data_t data;
    ASSERT_EQ(adxl345_read(NULL, &data), ADXL345_ERR_PARAM);
    ASSERT_EQ(adxl345_read(&d, NULL), ADXL345_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    adxl345_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    adxl345_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(adxl345_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)ADXL345_ERR_I2C);
    ASSERT_EQ(-2, (int)ADXL345_ERR_ID);
    ASSERT_EQ(-3, (int)ADXL345_ERR_PARAM);
}

TEST(test_init_sets_data_format)
{
    mock_reset();
    adxl345_t d;
    adxl345_cfg_t cfg = { .addr = ADXL345_DEV_ADDR, .range = ADXL345_RANGE_16G };
    adxl345_init(&d, &mock_i2c, &cfg);
    /* DATA_FORMAT: FULL_RES (0x08) | range 3 = 0x0B */
    ASSERT_EQ(mock_regs[0x31], 0x0B);
}

TEST(test_init_enables_measurement)
{
    mock_reset();
    adxl345_t d;
    adxl345_init(&d, &mock_i2c, &default_cfg);
    /* POWER_CTL: MEASURE bit = 0x08 */
    ASSERT_EQ(mock_regs[0x2D], 0x08);
}

TEST(test_read_zero_accel)
{
    setup();
    plant_accel(0, 0, 0);
    adxl345_data_t data;
    adxl345_err_t e = adxl345_read(&dev, &data);
    ASSERT_EQ(e, ADXL345_OK);
    ASSERT_NEAR(data.x_g, 0.0f, 0.001f);
    ASSERT_NEAR(data.y_g, 0.0f, 0.001f);
    ASSERT_NEAR(data.z_g, 0.0f, 0.001f);
}

TEST(test_read_1g_z)
{
    setup();
    /* 1g at full-res 3.9mg/LSB = 256 LSB */
    plant_accel(0, 0, 256);
    adxl345_data_t data;
    adxl345_err_t e = adxl345_read(&dev, &data);
    ASSERT_EQ(e, ADXL345_OK);
    ASSERT_NEAR(data.z_g, 1.0f, 0.05f);
    ASSERT_NEAR(data.x_g, 0.0f, 0.001f);
}

TEST(test_read_negative_accel)
{
    setup();
    /* -1g = -256 LSB */
    plant_accel(-256, 0, 0);
    adxl345_data_t data;
    adxl345_read(&dev, &data);
    ASSERT_NEAR(data.x_g, -1.0f, 0.05f);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    adxl345_data_t data;
    adxl345_err_t e = adxl345_read(&dev, &data);
    ASSERT_EQ(e, ADXL345_ERR_I2C);
}

TEST(test_invalid_range)
{
    mock_reset();
    adxl345_t d;
    adxl345_cfg_t cfg = { .addr = ADXL345_DEV_ADDR, .range = (adxl345_range_t)5 };
    adxl345_err_t e = adxl345_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, ADXL345_ERR_PARAM);
}

TEST(test_read_all_axes)
{
    setup();
    plant_accel(128, -128, 512);
    adxl345_data_t data;
    adxl345_read(&dev, &data);
    /* 128 * 0.0039 = 0.4992 g */
    ASSERT_NEAR(data.x_g, 0.4992f, 0.01f);
    ASSERT_NEAR(data.y_g, -0.4992f, 0.01f);
    ASSERT_NEAR(data.z_g, 1.9968f, 0.01f);
}

TEST(test_init_data_format_fail)
{
    mock_reset();
    mock_fail_at = 1;  /* fail on DATA_FORMAT write */
    adxl345_t d;
    adxl345_err_t e = adxl345_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, ADXL345_ERR_I2C);
}

TEST(test_init_power_ctl_fail)
{
    mock_reset();
    mock_fail_at = 2;  /* fail on POWER_CTL write */
    adxl345_t d;
    adxl345_err_t e = adxl345_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, ADXL345_ERR_I2C);
}

TEST_MAIN("adxl345")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_init_sets_data_format);
    RUN(test_init_enables_measurement);
    RUN(test_read_zero_accel);
    RUN(test_read_1g_z);
    RUN(test_read_negative_accel);
    RUN(test_read_i2c_fail);
    RUN(test_invalid_range);
    RUN(test_read_all_axes);
    RUN(test_init_data_format_fail);
    RUN(test_init_power_ctl_fail);
TEST_MAIN_END()
