// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "adxl345.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x00] = 0xE5;  /* DEVID */
}

static const adxl345_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static adxl345_t dev;
static adxl345_cfg_t default_cfg = {
    .addr      = ADXL345_ADDR_PRIMARY,
    .range     = ADXL345_RANGE_2G,
    .data_rate = ADXL345_RATE_100HZ,
};

static void setup(void)
{
    mock_reset();
    adxl345_init(&dev, &mock_i2c, &default_cfg);
}

/* ---------- plant raw data into mock registers ---------- */

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
    ASSERT_EQ(adxl345_set_range(NULL, ADXL345_RANGE_2G), ADXL345_ERR_PARAM);
}

TEST(test_read_zero)
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
    /* 1g at full-res = 256 LSB (256 * 0.0039 = 0.9984g) */
    plant_accel(0, 0, 256);
    adxl345_data_t data;
    adxl345_err_t e = adxl345_read(&dev, &data);
    ASSERT_EQ(e, ADXL345_OK);
    ASSERT_NEAR(data.x_g, 0.0f, 0.001f);
    ASSERT_NEAR(data.y_g, 0.0f, 0.001f);
    ASSERT_NEAR(data.z_g, 0.9984f, 0.01f);
}

TEST(test_read_negative)
{
    setup();
    plant_accel(-256, 0, 0);
    adxl345_data_t data;
    adxl345_err_t e = adxl345_read(&dev, &data);
    ASSERT_EQ(e, ADXL345_OK);
    ASSERT_NEAR(data.x_g, -0.9984f, 0.01f);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    adxl345_data_t data;
    adxl345_err_t e = adxl345_read(&dev, &data);
    ASSERT_EQ(e, ADXL345_ERR_I2C);
}

TEST(test_init_registers)
{
    setup();
    /* BW_RATE should be set to data rate */
    ASSERT_EQ(mock_regs[0x2C], ADXL345_RATE_100HZ);
    /* DATA_FORMAT: FULL_RES | range */
    ASSERT_EQ(mock_regs[0x31], 0x08);  /* FULL_RES=0x08, range=0 */
    /* POWER_CTL: measure bit */
    ASSERT_EQ(mock_regs[0x2D], 0x08);
}

TEST(test_set_range)
{
    setup();
    adxl345_err_t e = adxl345_set_range(&dev, ADXL345_RANGE_16G);
    ASSERT_EQ(e, ADXL345_OK);
    ASSERT_EQ(mock_regs[0x31], 0x0B);  /* FULL_RES | 3 */
}

TEST(test_set_range_invalid)
{
    setup();
    adxl345_err_t e = adxl345_set_range(&dev, (adxl345_range_t)99);
    ASSERT_EQ(e, ADXL345_ERR_PARAM);
}

TEST(test_set_range_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    adxl345_err_t e = adxl345_set_range(&dev, ADXL345_RANGE_4G);
    ASSERT_EQ(e, ADXL345_ERR_I2C);
}

TEST(test_addr_alt)
{
    mock_reset();
    adxl345_cfg_t cfg = {
        .addr = ADXL345_ADDR_ALT,
        .range = ADXL345_RANGE_2G,
        .data_rate = ADXL345_RATE_100HZ,
    };
    adxl345_t d;
    adxl345_err_t e = adxl345_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, ADXL345_OK);
}

TEST(test_init_invalid_range)
{
    mock_reset();
    adxl345_cfg_t cfg = {
        .addr = ADXL345_ADDR_PRIMARY,
        .range = (adxl345_range_t)99,
        .data_rate = ADXL345_RATE_100HZ,
    };
    adxl345_t d;
    adxl345_err_t e = adxl345_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, ADXL345_ERR_PARAM);
}

TEST(test_init_bw_rate_i2c_fail)
{
    mock_reset();
    /* ID read succeeds (op 0), BW_RATE write fails (op 1) */
    mock_fail_at = 1;
    adxl345_t d;
    adxl345_err_t e = adxl345_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, ADXL345_ERR_I2C);
}

TEST(test_full_scale_16g)
{
    mock_reset();
    adxl345_cfg_t cfg = {
        .addr = ADXL345_ADDR_PRIMARY,
        .range = ADXL345_RANGE_16G,
        .data_rate = ADXL345_RATE_100HZ,
    };
    adxl345_t d;
    adxl345_init(&d, &mock_i2c, &cfg);

    /* At 16g full-res, max val is ~4096 LSB (4096*0.0039=15.97g) */
    plant_accel(4096, -4096, 0);
    adxl345_data_t data;
    adxl345_err_t e = adxl345_read(&d, &data);
    ASSERT_EQ(e, ADXL345_OK);
    ASSERT_NEAR(data.x_g, 15.974f, 0.1f);
    ASSERT_NEAR(data.y_g, -15.974f, 0.1f);
}

TEST(test_init_refuses_rates_i2c_cannot_carry)
{
    adxl345_cfg_t cfg = default_cfg;
    mock_reset();
    cfg.data_rate = ADXL345_RATE_1600HZ;
    ASSERT_EQ(adxl345_init(&dev, &mock_i2c, &cfg), ADXL345_ERR_PARAM);
    cfg.data_rate = ADXL345_RATE_3200HZ;
    ASSERT_EQ(adxl345_init(&dev, &mock_i2c, &cfg), ADXL345_ERR_PARAM);
}

TEST_MAIN("adxl345")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_zero);
    RUN(test_read_1g_z);
    RUN(test_read_negative);
    RUN(test_read_i2c_fail);
    RUN(test_init_registers);
    RUN(test_set_range);
    RUN(test_set_range_invalid);
    RUN(test_set_range_i2c_fail);
    RUN(test_addr_alt);
    RUN(test_init_invalid_range);
    RUN(test_init_bw_rate_i2c_fail);
    RUN(test_full_scale_16g);
    RUN(test_init_refuses_rates_i2c_cannot_carry);
TEST_MAIN_END()
