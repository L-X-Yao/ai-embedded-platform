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

static uint8_t bus_read(uint8_t reg)
{
    uint8_t val = 0xA5u;
    (void)mpr121_model_read_regs(&model, MPR121_DEV_ADDR, reg, &val, 1);
    return val;
}

static void bus_write(uint8_t reg, uint8_t val)
{
    ASSERT_TRUE(mpr121_model_write_reg(&model, MPR121_DEV_ADDR, reg, val));
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

TEST(test_touch_proximity_not_in_bitmap)
{
    setup();
    mpr121_model_inject_touch(&model, 0x1001u);  /* E12TS and electrode 0 */

    uint16_t status = 0;
    ASSERT_EQ(mpr121_read_touched(&dev, &status), MPR121_OK);
    ASSERT_EQ_HEX(status, 0x0001u);
}

/* --- over current --- */

TEST(test_overcurrent_reported)
{
    setup();
    mpr121_model_inject_touch(&model, 0x0005u);
    mpr121_model_inject_electrode(&model, 3, 0x0123u);
    mpr121_model_inject_overcurrent(&model);

    uint16_t status = 0xFFFFu;
    ASSERT_EQ(mpr121_read_touched(&dev, &status), MPR121_ERR_OVERCURRENT);
    ASSERT_EQ_HEX(status, 0x0000u);

    /* Rev 4 p.11: Stop Mode, ECR D5-D0 and output registers cleared */
    ASSERT_EQ_HEX(model.ecr, 0x80u);
    uint16_t val = 0xFFFFu;
    ASSERT_EQ(mpr121_read_electrode(&dev, 3, &val), MPR121_OK);
    ASSERT_EQ(val, 0u);
}

/* Rev 4 p.11: ECR writes into Run Mode are discarded while OVCF is 1, and
 * writing 1 to OVCF clears it. This is the recovery the header describes. */
TEST(test_overcurrent_recovery)
{
    setup();
    mpr121_model_inject_overcurrent(&model);

    ASSERT_TRUE(mpr121_model_write_reg(&model, MPR121_DEV_ADDR, 0x5Eu, 0x8Fu));
    ASSERT_EQ_HEX(model.ecr, 0x80u);

    ASSERT_TRUE(mpr121_model_write_reg(&model, MPR121_DEV_ADDR,
                                       MPR121_REG_TOUCH_STATUS_H, 0x80u));
    ASSERT_EQ(mpr121_init(&dev, &model_i2c, &default_cfg), MPR121_OK);
    ASSERT_EQ_HEX(model.ecr, 0x8Fu);

    uint16_t status = 0xFFFFu;
    ASSERT_EQ(mpr121_read_touched(&dev, &status), MPR121_OK);
    ASSERT_EQ_HEX(status, 0x0000u);
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

/* --- what the model itself must do (MPR121 Rev 4) --- */

/* p10: "All the registers are cleared, except AFE Configuration Registers 0x5C
 * (Default 0x10) and 0x5D (Default 0x24)." */
TEST(test_afe_reset_values)
{
    mpr121_model_reset(&model);
    ASSERT_EQ_HEX(bus_read(0x5Cu), 0x10u);
    ASSERT_EQ_HEX(bus_read(0x5Du), 0x24u);
    ASSERT_EQ_HEX(bus_read(MPR121_REG_ECR), 0x00u);
}

/* p10: the same note covers "soft reset by command", output registers too */
TEST(test_soft_reset_clears_everything_else)
{
    mpr121_model_reset(&model);
    bus_write(0x5Cu, 0x3Fu);
    bus_write(0x5Du, 0x00u);
    bus_write(MPR121_REG_MHDR, 0x3Fu);
    bus_write(MPR121_REG_ECR, 0x8Fu);
    mpr121_model_inject_touch(&model, 0x0FFFu);
    mpr121_model_inject_electrode(&model, 0, 0x0123u);

    bus_write(MPR121_REG_SOFT_RESET, 0x63u);
    ASSERT_EQ_HEX(bus_read(0x00u), 0x00u);
    ASSERT_EQ_HEX(bus_read(0x01u), 0x00u);
    ASSERT_EQ_HEX(bus_read(0x04u), 0x00u);
    ASSERT_EQ_HEX(bus_read(MPR121_REG_MHDR), 0x00u);
    ASSERT_EQ_HEX(bus_read(MPR121_REG_ECR), 0x00u);
    ASSERT_EQ_HEX(bus_read(0x5Cu), 0x10u);
    ASSERT_EQ_HEX(bus_read(0x5Du), 0x24u);
}

/* p10: "Registers write operation can only be done after MPR121 is set into
 * Stop Mode, except the ECR and GPIO/LED related registers, which means
 * changing the value of these registers while MPR121 is in Run Mode will not
 * be effective." */
TEST(test_control_writes_are_lost_in_run_mode)
{
    mpr121_model_reset(&model);
    bus_write(MPR121_REG_ECR, 0x8Fu);

    bus_write(MPR121_REG_MHDR, 0x3Fu);
    bus_write(MPR121_REG_FDLF, 0x3Fu);
    bus_write(MPR121_REG_E0TTH, 0x55u);
    bus_write(MPR121_REG_E0RTH, 0x55u);
    bus_write(0x5Cu, 0x3Fu);
    bus_write(0x33u, 0x3Fu);  /* touched MHD */
    bus_write(0x5Fu, 0x3Fu);  /* CDC0 */
    bus_write(0x7Bu, 0x3Fu);  /* auto-config control */

    ASSERT_EQ_HEX(bus_read(0x33u), 0x00u);
    ASSERT_EQ_HEX(bus_read(MPR121_REG_MHDR), 0x00u);
    ASSERT_EQ_HEX(bus_read(MPR121_REG_FDLF), 0x00u);
    ASSERT_EQ_HEX(bus_read(MPR121_REG_E0TTH), 0x00u);
    ASSERT_EQ_HEX(bus_read(MPR121_REG_E0RTH), 0x00u);
    ASSERT_EQ_HEX(bus_read(0x5Cu), 0x10u);
    ASSERT_EQ_HEX(bus_read(0x5Fu), 0x00u);
    ASSERT_EQ_HEX(bus_read(0x7Bu), 0x00u);
}

TEST(test_control_writes_land_in_stop_mode)
{
    mpr121_model_reset(&model);
    bus_write(MPR121_REG_MHDR, 0x3Fu);
    bus_write(MPR121_REG_E0TTH, 0x55u);
    bus_write(0x5Cu, 0x3Fu);
    bus_write(0x33u, 0x3Fu);
    bus_write(0x5Fu, 0x3Fu);
    bus_write(0x7Bu, 0x3Fu);

    ASSERT_EQ_HEX(bus_read(0x33u), 0x3Fu);
    ASSERT_EQ_HEX(bus_read(MPR121_REG_MHDR), 0x3Fu);
    ASSERT_EQ_HEX(bus_read(MPR121_REG_E0TTH), 0x55u);
    ASSERT_EQ_HEX(bus_read(0x5Cu), 0x3Fu);
    ASSERT_EQ_HEX(bus_read(0x5Fu), 0x3Fu);
    ASSERT_EQ_HEX(bus_read(0x7Bu), 0x3Fu);
}

/* p10: "Set MPR121 into Stop Mode by writing 2bXX000000 to ECR register at any
 * time", and that opens the control registers again */
TEST(test_ecr_stop_mode_reopens_control_registers)
{
    mpr121_model_reset(&model);
    bus_write(MPR121_REG_ECR, 0x8Fu);
    bus_write(MPR121_REG_ECR, 0xC0u);
    ASSERT_EQ_HEX(bus_read(MPR121_REG_ECR), 0xC0u);
    bus_write(MPR121_REG_MHDR, 0x3Fu);
    ASSERT_EQ_HEX(bus_read(MPR121_REG_MHDR), 0x3Fu);
}

TEST(test_gpio_writes_work_in_run_mode)
{
    mpr121_model_reset(&model);
    bus_write(MPR121_REG_ECR, 0x8Fu);
    for (uint8_t reg = 0x73u; reg <= 0x7Au; reg++) {
        bus_write(reg, 0x5Au);
        ASSERT_EQ_HEX(bus_read(reg), 0x5Au);
    }
}

/* p7: "MPR121 is default in Stop Mode without any capacitance measurement",
 * so nothing reaches the output registers until Run Mode */
TEST(test_stop_mode_measures_nothing)
{
    mpr121_model_reset(&model);
    mpr121_model_inject_touch(&model, 0x0FFFu);
    mpr121_model_inject_electrode(&model, 0, 0x0123u);
    ASSERT_EQ_HEX(bus_read(0x00u), 0x00u);
    ASSERT_EQ_HEX(bus_read(0x04u), 0x00u);
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
    RUN(test_touch_proximity_not_in_bitmap);
    RUN(test_overcurrent_reported);
    RUN(test_overcurrent_recovery);
    RUN(test_electrode_roundtrip_ch0);
    RUN(test_electrode_roundtrip_ch11);
    RUN(test_electrode_roundtrip_zero);
    RUN(test_electrode_all_channels);
    RUN(test_touch_then_clear);
    RUN(test_electrode_updates);
    RUN(test_afe_reset_values);
    RUN(test_soft_reset_clears_everything_else);
    RUN(test_control_writes_are_lost_in_run_mode);
    RUN(test_control_writes_land_in_stop_mode);
    RUN(test_ecr_stop_mode_reopens_control_registers);
    RUN(test_gpio_writes_work_in_run_mode);
    RUN(test_stop_mode_measures_nothing);
TEST_MAIN_END()
