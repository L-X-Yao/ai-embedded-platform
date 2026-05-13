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

/* ---- init ---- */

TEST(test_hw_init_ok)
{
    ltr390_err_t e = setup();
    ASSERT_EQ(e, LTR390_OK);
}

TEST(test_hw_init_reads_part_id)
{
    hw_model_reset();
    ltr390_t d;
    (void)ltr390_init(&d, &hw_i2c, &default_cfg);
    /* init must have performed at least one I2C read (part ID) */
    ASSERT_TRUE(ltr390_mdl.read_count >= 1);
}

TEST(test_hw_init_sets_gain)
{
    setup();
    /* Gain register (0x05) should be set to cfg->gain = 1 (GAIN_3) */
    ASSERT_EQ(ltr390_mdl.regs[0x05], 1);
}

TEST(test_hw_init_sets_resolution)
{
    setup();
    /* MEAS_RATE register (0x04): resolution in bits [6:4], default_cfg.resolution=2 */
    /* (2 << 4) = 0x20, rate bits [2:0] = 0 */
    ASSERT_EQ(ltr390_mdl.regs[0x04], 0x20);
}

TEST(test_hw_init_enables_sensor)
{
    setup();
    /* MAIN_CTRL (0x00): enable bit [1] set, ALS mode bit [3] clear */
    ASSERT_EQ(ltr390_mdl.regs[0x00], 0x02);
}

TEST(test_hw_init_uvs_mode)
{
    hw_model_reset();
    ltr390_cfg_t cfg = { .mode = LTR390_MODE_UVS, .gain = LTR390_GAIN_1,
                         .resolution = LTR390_RES_20BIT };
    ltr390_t d;
    ltr390_err_t e = ltr390_init(&d, &hw_i2c, &cfg);
    ASSERT_EQ(e, LTR390_OK);
    /* MAIN_CTRL: enable [1] + UVS mode [3] = 0x0A */
    ASSERT_EQ(ltr390_mdl.regs[0x00], 0x0A);
}

/* ---- wrong ID ---- */

TEST(test_hw_init_wrong_id)
{
    hw_model_reset();
    ltr390_mdl.regs[0x06] = 0x55;  /* wrong part ID */
    ltr390_t d;
    ltr390_err_t e = ltr390_init(&d, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, LTR390_ERR_ID);
}

TEST(test_hw_init_id_revision_ignored)
{
    /* Part number bits [7:4] must match 0xB, but revision [3:0] can vary */
    hw_model_reset();
    ltr390_mdl.regs[0x06] = 0xB5;  /* part=0xB, rev=0x5 */
    ltr390_t d;
    ltr390_err_t e = ltr390_init(&d, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, LTR390_OK);
}

/* ---- NULL safety ---- */

TEST(test_null_dev)
{
    ASSERT_EQ(ltr390_init(NULL, &hw_i2c, &default_cfg), LTR390_ERR_PARAM);
}

TEST(test_null_i2c)
{
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, NULL, &default_cfg), LTR390_ERR_PARAM);
}

TEST(test_null_cfg)
{
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &hw_i2c, NULL), LTR390_ERR_PARAM);
}

TEST(test_null_hal_fptrs)
{
    ltr390_i2c_t bad = { .write_reg = NULL, .read_regs = NULL, .ctx = NULL };
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &bad, &default_cfg), LTR390_ERR_PARAM);
}

TEST(test_null_read_als)
{
    setup();
    ASSERT_EQ(ltr390_read_als(NULL, NULL), LTR390_ERR_PARAM);
    ASSERT_EQ(ltr390_read_als(&dev, NULL), LTR390_ERR_PARAM);
}

TEST(test_null_read_uvs)
{
    setup();
    ASSERT_EQ(ltr390_read_uvs(NULL, NULL), LTR390_ERR_PARAM);
    ASSERT_EQ(ltr390_read_uvs(&dev, NULL), LTR390_ERR_PARAM);
}

TEST(test_null_set_mode)
{
    ASSERT_EQ(ltr390_set_mode(NULL, LTR390_MODE_ALS), LTR390_ERR_PARAM);
}

/* ---- read ALS ---- */

TEST(test_hw_read_als_zero)
{
    setup();
    hw_model_inject_als(0);
    uint32_t raw = 0xFFFFFFFF;
    ltr390_err_t e = ltr390_read_als(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 0u);
}

TEST(test_hw_read_als_known_value)
{
    setup();
    hw_model_inject_als(12345);
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_als(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 12345u);
}

TEST(test_hw_read_als_max_20bit)
{
    setup();
    hw_model_inject_als(0x000FFFFF);
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_als(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 0x000FFFFFu);
}

TEST(test_hw_read_als_byte_order)
{
    /* Inject value 0x0A0B0C: byte0=0x0C, byte1=0x0B, byte2=0x0A */
    setup();
    hw_model_inject_als(0x000A0B0Cu);
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_als(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ_HEX(raw, 0x000A0B0Cu);
}

/* ---- read UVS ---- */

TEST(test_hw_read_uvs_zero)
{
    setup();
    hw_model_inject_uvs(0);
    uint32_t raw = 0xFFFFFFFF;
    ltr390_err_t e = ltr390_read_uvs(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 0u);
}

TEST(test_hw_read_uvs_known_value)
{
    setup();
    hw_model_inject_uvs(54321);
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_uvs(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 54321u);
}

TEST(test_hw_read_uvs_max_20bit)
{
    setup();
    hw_model_inject_uvs(0x000FFFFF);
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_uvs(&dev, &raw);
    ASSERT_EQ(e, LTR390_OK);
    ASSERT_EQ(raw, 0x000FFFFFu);
}

/* ---- mode switch ---- */

TEST(test_hw_set_mode_als_to_uvs)
{
    setup();  /* starts in ALS mode */
    ltr390_err_t e = ltr390_set_mode(&dev, LTR390_MODE_UVS);
    ASSERT_EQ(e, LTR390_OK);
    /* MAIN_CTRL: enable [1] + UVS mode [3] = 0x0A */
    ASSERT_EQ(ltr390_mdl.regs[0x00], 0x0A);
}

TEST(test_hw_set_mode_uvs_to_als)
{
    hw_model_reset();
    ltr390_cfg_t cfg = { .mode = LTR390_MODE_UVS, .gain = LTR390_GAIN_3,
                         .resolution = LTR390_RES_18BIT };
    ltr390_t d;
    ltr390_init(&d, &hw_i2c, &cfg);

    ltr390_err_t e = ltr390_set_mode(&d, LTR390_MODE_ALS);
    ASSERT_EQ(e, LTR390_OK);
    /* MAIN_CTRL: enable [1] only = 0x02 */
    ASSERT_EQ(ltr390_mdl.regs[0x00], 0x02);
}

TEST(test_hw_set_mode_invalid)
{
    setup();
    ltr390_err_t e = ltr390_set_mode(&dev, (ltr390_mode_t)99);
    ASSERT_EQ(e, LTR390_ERR_PARAM);
}

/* ---- I2C error paths ---- */

TEST(test_hw_init_i2c_read_fail)
{
    hw_model_reset();
    ltr390_mdl.fail_next_read = true;
    ltr390_t d;
    ltr390_err_t e = ltr390_init(&d, &hw_i2c, &default_cfg);
    ASSERT_EQ(e, LTR390_ERR_I2C);
}

TEST(test_hw_read_als_i2c_fail)
{
    setup();
    ltr390_mdl.fail_next_read = true;
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_als(&dev, &raw);
    ASSERT_EQ(e, LTR390_ERR_I2C);
}

TEST(test_hw_read_uvs_i2c_fail)
{
    setup();
    ltr390_mdl.fail_next_read = true;
    uint32_t raw = 0;
    ltr390_err_t e = ltr390_read_uvs(&dev, &raw);
    ASSERT_EQ(e, LTR390_ERR_I2C);
}

TEST(test_hw_set_mode_i2c_fail)
{
    setup();
    ltr390_mdl.fail_next_write = true;
    ltr390_err_t e = ltr390_set_mode(&dev, LTR390_MODE_UVS);
    ASSERT_EQ(e, LTR390_ERR_I2C);
}

/* ---- error code values ---- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)LTR390_OK,        0);
    ASSERT_EQ((int)LTR390_ERR_I2C,  -1);
    ASSERT_EQ((int)LTR390_ERR_ID,   -2);
    ASSERT_EQ((int)LTR390_ERR_PARAM,-3);
}

/* ---- invalid config ---- */

TEST(test_init_invalid_gain)
{
    hw_model_reset();
    ltr390_cfg_t cfg = { .mode = LTR390_MODE_ALS, .gain = (ltr390_gain_t)99,
                         .resolution = LTR390_RES_18BIT };
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &hw_i2c, &cfg), LTR390_ERR_PARAM);
}

TEST(test_init_invalid_resolution)
{
    hw_model_reset();
    ltr390_cfg_t cfg = { .mode = LTR390_MODE_ALS, .gain = LTR390_GAIN_1,
                         .resolution = (ltr390_res_t)99 };
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &hw_i2c, &cfg), LTR390_ERR_PARAM);
}

TEST(test_init_invalid_mode)
{
    hw_model_reset();
    ltr390_cfg_t cfg = { .mode = (ltr390_mode_t)99, .gain = LTR390_GAIN_1,
                         .resolution = LTR390_RES_18BIT };
    ltr390_t d;
    ASSERT_EQ(ltr390_init(&d, &hw_i2c, &cfg), LTR390_ERR_PARAM);
}

/* ---- register isolation ---- */

TEST(test_hw_als_uvs_data_isolated)
{
    setup();
    hw_model_inject_als(0x00012345u);
    hw_model_inject_uvs(0x000ABCDEu);

    uint32_t als = 0, uvs = 0;
    ASSERT_EQ(ltr390_read_als(&dev, &als), LTR390_OK);
    ASSERT_EQ(ltr390_read_uvs(&dev, &uvs), LTR390_OK);
    ASSERT_EQ_HEX(als, 0x00012345u);
    ASSERT_EQ_HEX(uvs, 0x000ABCDEu);
}

/* ---- zero-init safety ---- */

TEST(test_zero_init_safety)
{
    ltr390_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ltr390_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(ltr390_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

TEST_MAIN("ltr390_hw_model")
    RUN(test_hw_init_ok);
    RUN(test_hw_init_reads_part_id);
    RUN(test_hw_init_sets_gain);
    RUN(test_hw_init_sets_resolution);
    RUN(test_hw_init_enables_sensor);
    RUN(test_hw_init_uvs_mode);
    RUN(test_hw_init_wrong_id);
    RUN(test_hw_init_id_revision_ignored);
    RUN(test_null_dev);
    RUN(test_null_i2c);
    RUN(test_null_cfg);
    RUN(test_null_hal_fptrs);
    RUN(test_null_read_als);
    RUN(test_null_read_uvs);
    RUN(test_null_set_mode);
    RUN(test_hw_read_als_zero);
    RUN(test_hw_read_als_known_value);
    RUN(test_hw_read_als_max_20bit);
    RUN(test_hw_read_als_byte_order);
    RUN(test_hw_read_uvs_zero);
    RUN(test_hw_read_uvs_known_value);
    RUN(test_hw_read_uvs_max_20bit);
    RUN(test_hw_set_mode_als_to_uvs);
    RUN(test_hw_set_mode_uvs_to_als);
    RUN(test_hw_set_mode_invalid);
    RUN(test_hw_init_i2c_read_fail);
    RUN(test_hw_read_als_i2c_fail);
    RUN(test_hw_read_uvs_i2c_fail);
    RUN(test_hw_set_mode_i2c_fail);
    RUN(test_error_codes);
    RUN(test_init_invalid_gain);
    RUN(test_init_invalid_resolution);
    RUN(test_init_invalid_mode);
    RUN(test_hw_als_uvs_data_isolated);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
