// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "lis3mdl.h"
#include <math.h>

/* ---------- chip-specific mock setup ---------- */

#define MOCK_WHO_AM_I_REG 0x0Fu
#define MOCK_WHO_AM_I_VAL 0x3Du
#define MOCK_OUT_X_L      0x28u

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[MOCK_WHO_AM_I_REG] = MOCK_WHO_AM_I_VAL;
}

static const lis3mdl_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static lis3mdl_t dev;
static const lis3mdl_cfg_t default_cfg = {
    .range = LIS3MDL_RANGE_4G,
    .addr  = LIS3MDL_DEV_ADDR,
};

/* Helper: set mock data registers (little-endian: XL,XH,YL,YH,ZL,ZH) */
static void set_mock_data(int16_t x, int16_t y, int16_t z)
{
    /* mock_i2c reads at reg & 0x7F for auto-increment, but also place at
     * both the base (0x28) and auto-inc (0xA8) ranges since mock_i2c
     * just uses reg+i as index. The driver sends 0xA8 (0x28 | 0x80),
     * but mock_i2c strips nothing — it reads at index 0xA8+i.
     * Place data at 0xA8..0xAD for auto-increment reads. */
    uint8_t base = 0xA8u;  /* 0x28 | 0x80 */
    mock_regs[base + 0] = (uint8_t)((uint16_t)x & 0xFFu);
    mock_regs[base + 1] = (uint8_t)((uint16_t)x >> 8);
    mock_regs[base + 2] = (uint8_t)((uint16_t)y & 0xFFu);
    mock_regs[base + 3] = (uint8_t)((uint16_t)y >> 8);
    mock_regs[base + 4] = (uint8_t)((uint16_t)z & 0xFFu);
    mock_regs[base + 5] = (uint8_t)((uint16_t)z >> 8);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(lis3mdl_init(&dev, &mock_i2c, &default_cfg), LIS3MDL_OK);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(lis3mdl_init(NULL, &mock_i2c, &default_cfg), LIS3MDL_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    ASSERT_EQ(lis3mdl_init(&dev, NULL, &default_cfg), LIS3MDL_ERR_PARAM);
}

TEST(test_init_null_cfg)
{
    ASSERT_EQ(lis3mdl_init(&dev, &mock_i2c, NULL), LIS3MDL_ERR_PARAM);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[MOCK_WHO_AM_I_REG] = 0x00;
    ASSERT_EQ(lis3mdl_init(&dev, &mock_i2c, &default_cfg), LIS3MDL_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(lis3mdl_init(&dev, &mock_i2c, &default_cfg), LIS3MDL_ERR_I2C);
}

TEST(test_init_write_ctrl1_fail)
{
    mock_reset();
    mock_fail_at = 1;  /* second op: write CTRL_REG1 */
    ASSERT_EQ(lis3mdl_init(&dev, &mock_i2c, &default_cfg), LIS3MDL_ERR_I2C);
}

TEST(test_init_write_ctrl2_fail)
{
    mock_reset();
    mock_fail_at = 2;  /* third op: write CTRL_REG2 */
    ASSERT_EQ(lis3mdl_init(&dev, &mock_i2c, &default_cfg), LIS3MDL_ERR_I2C);
}

TEST(test_init_write_ctrl3_fail)
{
    mock_reset();
    mock_fail_at = 3;  /* fourth op: write CTRL_REG3 */
    ASSERT_EQ(lis3mdl_init(&dev, &mock_i2c, &default_cfg), LIS3MDL_ERR_I2C);
}

TEST(test_zero_init_safety)
{
    lis3mdl_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    lis3mdl_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    lis3mdl_cfg_t zero_cfg;
    memset(&zero_cfg, 0, sizeof(zero_cfg));
    ASSERT_TRUE(lis3mdl_init(&zero_dev, &zero_hal, &zero_cfg) < 0);
}

TEST(test_invalid_range)
{
    mock_reset();
    lis3mdl_cfg_t bad_cfg = { .range = (lis3mdl_range_t)4, .addr = LIS3MDL_DEV_ADDR };
    ASSERT_EQ(lis3mdl_init(&dev, &mock_i2c, &bad_cfg), LIS3MDL_ERR_PARAM);
}

/* ---------- read tests ---------- */

TEST(test_read_known_values_4g)
{
    mock_reset();
    lis3mdl_init(&dev, &mock_i2c, &default_cfg);

    /* Inject raw=6842 -> should read ~100 uT (1 gauss) */
    set_mock_data(6842, -6842, 0);

    lis3mdl_data_t data;
    ASSERT_EQ(lis3mdl_read(&dev, &data), LIS3MDL_OK);
    ASSERT_NEAR(data.x_ut, 100.0f, 0.1f);
    ASSERT_NEAR(data.y_ut, -100.0f, 0.1f);
    ASSERT_NEAR(data.z_ut, 0.0f, 0.01f);
}

TEST(test_read_known_values_8g)
{
    mock_reset();
    lis3mdl_cfg_t cfg8 = { .range = LIS3MDL_RANGE_8G, .addr = LIS3MDL_DEV_ADDR };
    lis3mdl_init(&dev, &mock_i2c, &cfg8);

    /* raw=3421 at 8g -> 1 gauss = 100 uT */
    set_mock_data(3421, 0, -3421);

    lis3mdl_data_t data;
    ASSERT_EQ(lis3mdl_read(&dev, &data), LIS3MDL_OK);
    ASSERT_NEAR(data.x_ut, 100.0f, 0.1f);
    ASSERT_NEAR(data.y_ut, 0.0f, 0.01f);
    ASSERT_NEAR(data.z_ut, -100.0f, 0.1f);
}

TEST(test_read_known_values_12g)
{
    mock_reset();
    lis3mdl_cfg_t cfg12 = { .range = LIS3MDL_RANGE_12G, .addr = LIS3MDL_DEV_ADDR };
    lis3mdl_init(&dev, &mock_i2c, &cfg12);

    set_mock_data(2281, -2281, 2281);

    lis3mdl_data_t data;
    ASSERT_EQ(lis3mdl_read(&dev, &data), LIS3MDL_OK);
    ASSERT_NEAR(data.x_ut, 100.0f, 0.1f);
    ASSERT_NEAR(data.y_ut, -100.0f, 0.1f);
    ASSERT_NEAR(data.z_ut, 100.0f, 0.1f);
}

TEST(test_read_known_values_16g)
{
    mock_reset();
    lis3mdl_cfg_t cfg16 = { .range = LIS3MDL_RANGE_16G, .addr = LIS3MDL_DEV_ADDR };
    lis3mdl_init(&dev, &mock_i2c, &cfg16);

    set_mock_data(1711, 0, 0);

    lis3mdl_data_t data;
    ASSERT_EQ(lis3mdl_read(&dev, &data), LIS3MDL_OK);
    ASSERT_NEAR(data.x_ut, 100.0f, 0.1f);
}

TEST(test_read_raw)
{
    mock_reset();
    lis3mdl_init(&dev, &mock_i2c, &default_cfg);

    set_mock_data(1234, -5678, 9012);

    lis3mdl_raw_t raw;
    ASSERT_EQ(lis3mdl_read_raw(&dev, &raw), LIS3MDL_OK);
    ASSERT_EQ(raw.x, 1234);
    ASSERT_EQ(raw.y, -5678);
    ASSERT_EQ(raw.z, 9012);
}

TEST(test_read_null_dev)
{
    lis3mdl_data_t data;
    ASSERT_EQ(lis3mdl_read(NULL, &data), LIS3MDL_ERR_PARAM);
}

TEST(test_read_null_out)
{
    mock_reset();
    lis3mdl_init(&dev, &mock_i2c, &default_cfg);
    ASSERT_EQ(lis3mdl_read(&dev, NULL), LIS3MDL_ERR_PARAM);
}

TEST(test_read_raw_null)
{
    ASSERT_EQ(lis3mdl_read_raw(NULL, NULL), LIS3MDL_ERR_PARAM);
}

TEST(test_read_i2c_fail)
{
    mock_reset();
    lis3mdl_init(&dev, &mock_i2c, &default_cfg);
    mock_fail_next = 1;

    lis3mdl_data_t data;
    ASSERT_EQ(lis3mdl_read(&dev, &data), LIS3MDL_ERR_I2C);
}

/* ---------- error code tests ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ(0, (int)LIS3MDL_OK);
    ASSERT_EQ(-1, (int)LIS3MDL_ERR_I2C);
    ASSERT_EQ(-2, (int)LIS3MDL_ERR_ID);
    ASSERT_EQ(-3, (int)LIS3MDL_ERR_PARAM);
}

/* ---------- address configuration ---------- */

TEST(test_alternate_address)
{
    mock_reset();
    lis3mdl_cfg_t cfg_alt = { .range = LIS3MDL_RANGE_4G, .addr = LIS3MDL_ADDR_HIGH };
    ASSERT_EQ(lis3mdl_init(&dev, &mock_i2c, &cfg_alt), LIS3MDL_OK);
    ASSERT_EQ(dev.cfg.addr, LIS3MDL_ADDR_HIGH);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(lis3mdl_t *d)
{
    lis3mdl_data_t data;
    lis3mdl_raw_t raw;
    ASSERT_EQ(lis3mdl_read(d, &data), LIS3MDL_ERR_PARAM);
    ASSERT_EQ(lis3mdl_read_raw(d, &raw), LIS3MDL_ERR_PARAM);
}

TEST(test_device_refused_after_wrong_id)
{
    lis3mdl_t d;
    mock_reset();
    mock_regs[MOCK_WHO_AM_I_REG] = 0x00;
    ASSERT_EQ(lis3mdl_init(&d, &mock_i2c, &default_cfg), LIS3MDL_ERR_ID);
    assert_device_refused(&d);
}

TEST(test_device_refused_after_failed_init)
{
    lis3mdl_t d;
    for (int step = 0; step < 4; step++) {
        mock_reset();
        mock_fail_at = step;  /* the ID read, then the three CTRL writes */
        ASSERT_EQ(lis3mdl_init(&d, &mock_i2c, &default_cfg), LIS3MDL_ERR_I2C);
        assert_device_refused(&d);
    }
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    lis3mdl_t d;
    lis3mdl_i2c_t no_read = mock_i2c;
    lis3mdl_cfg_t bad = default_cfg;
    no_read.read_regs = NULL;
    bad.range = (lis3mdl_range_t)4;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(lis3mdl_init(&d, NULL, &default_cfg), LIS3MDL_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(lis3mdl_init(&d, &no_read, &default_cfg), LIS3MDL_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(lis3mdl_init(&d, &mock_i2c, &bad), LIS3MDL_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("lis3mdl")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_null_cfg);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_init_write_ctrl1_fail);
    RUN(test_init_write_ctrl2_fail);
    RUN(test_init_write_ctrl3_fail);
    RUN(test_zero_init_safety);
    RUN(test_invalid_range);
    RUN(test_read_known_values_4g);
    RUN(test_read_known_values_8g);
    RUN(test_read_known_values_12g);
    RUN(test_read_known_values_16g);
    RUN(test_read_raw);
    RUN(test_read_null_dev);
    RUN(test_read_null_out);
    RUN(test_read_raw_null);
    RUN(test_read_i2c_fail);
    RUN(test_error_codes);
    RUN(test_alternate_address);
    RUN(test_device_refused_after_wrong_id);
    RUN(test_device_refused_after_failed_init);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
