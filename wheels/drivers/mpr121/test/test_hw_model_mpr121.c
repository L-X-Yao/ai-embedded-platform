// SPDX-License-Identifier: MIT
/*
 * test_hw_model_mpr121.c — tests using hardware model
 *
 * Validates MPR121 driver against a datasheet-accurate model.
 * The model enforces register layout, touch status encoding,
 * electrode data, threshold write-back, and soft-reset behavior.
 */

#include "test_harness.h"
#include "hw_model_mpr121.h"
#include "../mpr121.h"

static mpr121_model_t model;

static const mpr121_i2c_t model_i2c = {
    .write_reg = mpr121_model_write_reg,
    .read_regs = mpr121_model_read_regs,
    .delay_ms  = mpr121_model_delay_ms,
    .ctx       = &model,
};

static const mpr121_cfg_t default_cfg = {
    .touch_threshold   = 12,
    .release_threshold = 6,
    .ecr               = 0x8Fu,
};

static mpr121_t dev;

static void setup(void)
{
    mpr121_model_reset(&model);
    mpr121_init(&dev, &model_i2c, &default_cfg);
}

/* --- init --- */

TEST(test_init_soft_reset_and_ecr)
{
    mpr121_model_reset(&model);
    mpr121_t d;
    mpr121_err_t err = mpr121_init(&d, &model_i2c, &default_cfg);
    ASSERT_EQ(err, MPR121_OK);
    /* After init, ECR should hold the configured value */
    ASSERT_EQ(model.ecr, 0x8Fu);
}

TEST(test_init_thresholds_roundtrip)
{
    mpr121_model_reset(&model);
    mpr121_t d;
    mpr121_cfg_t cfg = { .touch_threshold = 20, .release_threshold = 10, .ecr = 0x8Fu };
    mpr121_init(&d, &model_i2c, &cfg);

    for (int ch = 0; ch < 12; ch++) {
        ASSERT_EQ(model.touch_threshold[ch], 20);
        ASSERT_EQ(model.release_threshold[ch], 10);
    }
}

TEST(test_init_baseline_filters_written)
{
    mpr121_model_reset(&model);
    mpr121_t d;
    mpr121_init(&d, &model_i2c, &default_cfg);

    /* Verify baseline filtering control was configured */
    ASSERT_EQ(model.mhdr, 0x01u);
    ASSERT_EQ(model.nhdr, 0x01u);
    ASSERT_EQ(model.nclr, 0x0Eu);
    ASSERT_EQ(model.fdlr, 0x00u);
    ASSERT_EQ(model.mhdf, 0x01u);
    ASSERT_EQ(model.nhdf, 0x05u);
    ASSERT_EQ(model.nclf, 0x01u);
    ASSERT_EQ(model.fdlf, 0x00u);
}

TEST(test_init_delay_after_reset)
{
    mpr121_model_reset(&model);
    mpr121_t d;
    mpr121_init(&d, &model_i2c, &default_cfg);
    ASSERT_TRUE(model.delay_count >= 1);
}

/* --- touch status round-trip --- */

TEST(test_touch_roundtrip_single)
{
    setup();
    mpr121_model_inject_touch(&model, 0x0004u);  /* electrode 2 */

    uint16_t status = 0;
    mpr121_read_touched(&dev, &status);
    ASSERT_EQ_HEX(status, 0x0004u);
}

TEST(test_touch_roundtrip_multiple)
{
    setup();
    mpr121_model_inject_touch(&model, 0x0A05u);  /* electrodes 0, 2, 9, 11 */

    uint16_t status = 0;
    mpr121_read_touched(&dev, &status);
    ASSERT_EQ_HEX(status, 0x0A05u);
}

TEST(test_touch_roundtrip_none)
{
    setup();
    mpr121_model_inject_touch(&model, 0x0000u);

    uint16_t status = 0xFFFFu;
    mpr121_read_touched(&dev, &status);
    ASSERT_EQ(status, 0u);
}

TEST(test_touch_roundtrip_all)
{
    setup();
    mpr121_model_inject_touch(&model, 0x0FFFu);

    uint16_t status = 0;
    mpr121_read_touched(&dev, &status);
    ASSERT_EQ_HEX(status, 0x0FFFu);
}

/* --- electrode data round-trip --- */

TEST(test_electrode_roundtrip_ch0)
{
    setup();
    mpr121_model_inject_electrode(&model, 0, 0x0123u);

    uint16_t val = 0;
    mpr121_read_electrode(&dev, 0, &val);
    ASSERT_EQ(val, 0x0123u);
}

TEST(test_electrode_roundtrip_ch11)
{
    setup();
    mpr121_model_inject_electrode(&model, 11, 0x03FFu);

    uint16_t val = 0;
    mpr121_read_electrode(&dev, 11, &val);
    ASSERT_EQ(val, 0x03FFu);
}

TEST(test_electrode_roundtrip_zero)
{
    setup();
    mpr121_model_inject_electrode(&model, 5, 0x0000u);

    uint16_t val = 0xFFFFu;
    mpr121_read_electrode(&dev, 5, &val);
    ASSERT_EQ(val, 0u);
}

TEST(test_electrode_all_channels)
{
    setup();
    /* Inject increasing values */
    for (uint8_t ch = 0; ch < 12; ch++) {
        mpr121_model_inject_electrode(&model, ch, (uint16_t)(ch * 80u));
    }

    for (uint8_t ch = 0; ch < 12; ch++) {
        uint16_t val = 0;
        mpr121_read_electrode(&dev, ch, &val);
        ASSERT_EQ(val, (uint16_t)(ch * 80u));
    }
}

/* --- sequential operations --- */

TEST(test_touch_then_clear)
{
    setup();
    mpr121_model_inject_touch(&model, 0x0001u);

    uint16_t status = 0;
    mpr121_read_touched(&dev, &status);
    ASSERT_EQ_HEX(status, 0x0001u);

    mpr121_model_inject_touch(&model, 0x0000u);
    mpr121_read_touched(&dev, &status);
    ASSERT_EQ(status, 0u);
}

TEST(test_electrode_updates)
{
    setup();
    mpr121_model_inject_electrode(&model, 3, 100);

    uint16_t val = 0;
    mpr121_read_electrode(&dev, 3, &val);
    ASSERT_EQ(val, 100);

    mpr121_model_inject_electrode(&model, 3, 900);
    mpr121_read_electrode(&dev, 3, &val);
    ASSERT_EQ(val, 900);
}

TEST_MAIN("mpr121 hardware model")
    RUN(test_init_soft_reset_and_ecr);
    RUN(test_init_thresholds_roundtrip);
    RUN(test_init_baseline_filters_written);
    RUN(test_init_delay_after_reset);
    RUN(test_touch_roundtrip_single);
    RUN(test_touch_roundtrip_multiple);
    RUN(test_touch_roundtrip_none);
    RUN(test_touch_roundtrip_all);
    RUN(test_electrode_roundtrip_ch0);
    RUN(test_electrode_roundtrip_ch11);
    RUN(test_electrode_roundtrip_zero);
    RUN(test_electrode_all_channels);
    RUN(test_touch_then_clear);
    RUN(test_electrode_updates);
TEST_MAIN_END()
