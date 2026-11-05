// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_tmp117.h"
#include "tmp117.h"
#include <string.h>
#include <math.h>

static const tmp117_i2c_t mock_i2c = {
    .write_regs = hw_model_write_regs,
    .read_regs  = hw_model_read_regs,
    .delay_ms   = NULL,
    .ctx        = NULL,
};

static const tmp117_cfg_t default_cfg = {
    .addr = TMP117_ADDR_GND,
    .avg  = TMP117_AVG_NONE,
};

static tmp117_t dev;

/* ---------- init ---------- */

TEST(test_init_ok)
{
    hw_model_reset();
    ASSERT_EQ(tmp117_init(&dev, &mock_i2c, &default_cfg), TMP117_OK);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(tmp117_init(NULL, &mock_i2c, &default_cfg), TMP117_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    ASSERT_EQ(tmp117_init(&dev, NULL, &default_cfg), TMP117_ERR_PARAM);
}

TEST(test_init_null_cfg)
{
    ASSERT_EQ(tmp117_init(&dev, &mock_i2c, NULL), TMP117_ERR_PARAM);
}

TEST(test_init_zero_hal)
{
    tmp117_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ASSERT_TRUE(tmp117_init(&dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_init_wrong_id)
{
    hw_model_reset();
    hw_regs16[0x0F] = 0x0000;
    ASSERT_EQ(tmp117_init(&dev, &mock_i2c, &default_cfg), TMP117_ERR_ID);
}

/* TI SNOSD82D Table 7-15 (p.32): bits 15:12 are Rev[3:0], and only bits 11:0
 * hold the device ID, so a later revision must pass and a wrong ID must not. */
TEST(test_init_id_ignores_revision)
{
    static const struct {
        uint16_t      id;
        tmp117_err_t  want;
    } rows[] = {
        { 0x0117, TMP117_OK },     { 0x1117, TMP117_OK },
        { 0xF117, TMP117_OK },     { 0x0118, TMP117_ERR_ID },
        { 0x1118, TMP117_ERR_ID }, { 0x1000, TMP117_ERR_ID },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        hw_model_reset();
        hw_regs16[0x0F] = rows[i].id;
        ASSERT_EQ(tmp117_init(&dev, &mock_i2c, &default_cfg), rows[i].want);
    }
}

TEST(test_init_avg_modes)
{
    hw_model_reset();
    tmp117_cfg_t cfg = default_cfg;
    cfg.avg = TMP117_AVG_64;
    ASSERT_EQ(tmp117_init(&dev, &mock_i2c, &cfg), TMP117_OK);
    uint16_t config = hw_regs16[0x01];
    ASSERT_EQ((config >> 5) & 0x03, 3);
}

TEST(test_init_avg_invalid)
{
    tmp117_cfg_t cfg = default_cfg;
    cfg.avg = (tmp117_avg_t)4;
    ASSERT_EQ(tmp117_init(&dev, &mock_i2c, &cfg), TMP117_ERR_PARAM);
}

TEST(test_error_codes)
{
    ASSERT_EQ((int)TMP117_OK, 0);
    ASSERT_EQ((int)TMP117_ERR_I2C, -1);
    ASSERT_EQ((int)TMP117_ERR_ID, -2);
    ASSERT_EQ((int)TMP117_ERR_PARAM, -3);
    ASSERT_EQ((int)TMP117_ERR_NOT_READY, -4);
}

/* ---------- read ---------- */

/* TI SNOSD82D p.26: the register resets to 8000h and "reads -256 C until the
 * first conversion, including averaging, is complete". */
TEST(test_read_before_first_conversion)
{
    hw_model_reset();
    tmp117_init(&dev, &mock_i2c, &default_cfg);

    float temp = 99.0f;
    ASSERT_EQ(tmp117_read(&dev, &temp), TMP117_ERR_NOT_READY);
    ASSERT_NEAR(temp, 99.0f, 1e-6f);

    hw_model_inject_temperature(25.0f);
    ASSERT_EQ(tmp117_read(&dev, &temp), TMP117_OK);
    ASSERT_NEAR(temp, 25.0f, 0.01f);
}

/* Only the reset value itself means "no result yet" */
TEST(test_read_next_code_is_data)
{
    hw_model_reset();
    tmp117_init(&dev, &mock_i2c, &default_cfg);
    hw_regs16[0x00] = 0x8001;

    float temp = 0.0f;
    ASSERT_EQ(tmp117_read(&dev, &temp), TMP117_OK);
    ASSERT_NEAR(temp, -255.9921875f, 1e-4f);
}

TEST(test_read_25c)
{
    hw_model_reset();
    tmp117_init(&dev, &mock_i2c, &default_cfg);
    hw_model_inject_temperature(25.0f);
    float temp = 0.0f;
    ASSERT_EQ(tmp117_read(&dev, &temp), TMP117_OK);
    ASSERT_NEAR(temp, 25.0f, 0.01f);
}

TEST(test_read_zero)
{
    hw_model_reset();
    tmp117_init(&dev, &mock_i2c, &default_cfg);
    hw_model_inject_temperature(0.0f);
    float temp = 99.0f;
    ASSERT_EQ(tmp117_read(&dev, &temp), TMP117_OK);
    ASSERT_NEAR(temp, 0.0f, 0.01f);
}

TEST(test_read_negative)
{
    hw_model_reset();
    tmp117_init(&dev, &mock_i2c, &default_cfg);
    hw_model_inject_temperature(-40.0f);
    float temp = 0.0f;
    ASSERT_EQ(tmp117_read(&dev, &temp), TMP117_OK);
    ASSERT_NEAR(temp, -40.0f, 0.01f);
}

TEST(test_read_max)
{
    hw_model_reset();
    tmp117_init(&dev, &mock_i2c, &default_cfg);
    hw_model_inject_temperature(150.0f);
    float temp = 0.0f;
    ASSERT_EQ(tmp117_read(&dev, &temp), TMP117_OK);
    ASSERT_NEAR(temp, 150.0f, 0.02f);
}

TEST(test_read_resolution)
{
    hw_model_reset();
    tmp117_init(&dev, &mock_i2c, &default_cfg);
    hw_regs16[0x00] = 0x0001;
    float temp = 0.0f;
    ASSERT_EQ(tmp117_read(&dev, &temp), TMP117_OK);
    ASSERT_NEAR(temp, 0.0078125f, 1e-6f);
}

TEST(test_read_null_dev)
{
    ASSERT_EQ(tmp117_read(NULL, &(float){0}), TMP117_ERR_PARAM);
}

TEST(test_read_null_out)
{
    hw_model_reset();
    tmp117_init(&dev, &mock_i2c, &default_cfg);
    ASSERT_EQ(tmp117_read(&dev, NULL), TMP117_ERR_PARAM);
}

/* ---------- reset ---------- */

TEST(test_reset_ok)
{
    hw_model_reset();
    tmp117_init(&dev, &mock_i2c, &default_cfg);
    ASSERT_EQ(tmp117_reset(&dev), TMP117_OK);
    ASSERT_EQ(hw_regs16[0x01] & 0x0002, 0x0002);
}

TEST(test_reset_null)
{
    ASSERT_EQ(tmp117_reset(NULL), TMP117_ERR_PARAM);
}

TEST_MAIN("tmp117")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_null_cfg);
    RUN(test_init_zero_hal);
    RUN(test_init_wrong_id);
    RUN(test_init_id_ignores_revision);
    RUN(test_init_avg_modes);
    RUN(test_init_avg_invalid);
    RUN(test_error_codes);
    RUN(test_read_before_first_conversion);
    RUN(test_read_next_code_is_data);
    RUN(test_read_25c);
    RUN(test_read_zero);
    RUN(test_read_negative);
    RUN(test_read_max);
    RUN(test_read_resolution);
    RUN(test_read_null_dev);
    RUN(test_read_null_out);
    RUN(test_reset_ok);
    RUN(test_reset_null);
TEST_MAIN_END()
