// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_ltr390.h"
#include "ltr390.h"

static const ltr390_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = hw_model_delay_ms,
    .ctx       = NULL,
};

static ltr390_t dev;

static const ltr390_cfg_t default_cfg = {
    .mode       = LTR390_MODE_ALS,
    .gain       = LTR390_GAIN_3,
    .resolution = LTR390_RES_18BIT,
};

static ltr390_err_t setup(void)
{
    hw_model_reset();
    return ltr390_init(&dev, &hw_i2c, &default_cfg);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    ASSERT_EQ(setup(), LTR390_OK);
}

TEST(test_init_wrong_id)
{
    hw_model_reset();
    ltr390_mdl.regs[0x06] = 0x00;
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &hw_i2c, &default_cfg), LTR390_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    hw_model_reset();
    ltr390_mdl.fail_next_read = true;
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &hw_i2c, &default_cfg), LTR390_ERR_I2C);
}

/* ---------- NULL safety ---------- */

TEST(test_null_safety)
{
    ASSERT_EQ(ltr390_init(NULL, &hw_i2c, &default_cfg), LTR390_ERR_PARAM);
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, NULL, &default_cfg), LTR390_ERR_PARAM);
    ASSERT_EQ(ltr390_init(&d, &hw_i2c, NULL), LTR390_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    ltr390_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ltr390_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(ltr390_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_null_output_ptr)
{
    setup();
    ASSERT_EQ(ltr390_read_als(&dev, NULL), LTR390_ERR_PARAM);
    ASSERT_EQ(ltr390_read_uvs(&dev, NULL), LTR390_ERR_PARAM);
}

/* ---------- read ALS ---------- */

TEST(test_read_als)
{
    setup();
    hw_model_inject_als(1000);
    uint32_t raw = 0;
    ASSERT_EQ(ltr390_read_als(&dev, &raw), LTR390_OK);
    ASSERT_EQ(raw, 1000u);
}

/* ---------- read UVS ---------- */

TEST(test_read_uvs)
{
    setup();
    hw_model_inject_uvs(2000);
    uint32_t raw = 0;
    ASSERT_EQ(ltr390_read_uvs(&dev, &raw), LTR390_OK);
    ASSERT_EQ(raw, 2000u);
}

/* ---------- mode switch ---------- */

TEST(test_mode_switch)
{
    setup();
    ASSERT_EQ(ltr390_set_mode(&dev, LTR390_MODE_UVS), LTR390_OK);
    /* MAIN_CTRL should have enable + UVS bits */
    ASSERT_EQ(ltr390_mdl.regs[0x00], 0x0A);

    ASSERT_EQ(ltr390_set_mode(&dev, LTR390_MODE_ALS), LTR390_OK);
    /* MAIN_CTRL should have enable only */
    ASSERT_EQ(ltr390_mdl.regs[0x00], 0x02);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)LTR390_OK,         0);
    ASSERT_EQ((int)LTR390_ERR_I2C,   -1);
    ASSERT_EQ((int)LTR390_ERR_ID,    -2);
    ASSERT_EQ((int)LTR390_ERR_PARAM, -3);
}

TEST_MAIN("ltr390")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_null_output_ptr);
    RUN(test_read_als);
    RUN(test_read_uvs);
    RUN(test_mode_switch);
    RUN(test_error_codes);
TEST_MAIN_END()
