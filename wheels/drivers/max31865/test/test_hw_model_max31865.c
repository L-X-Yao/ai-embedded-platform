// SPDX-License-Identifier: MIT
/*
 * test_hw_model_max31865.c — tests using hardware register model
 *
 * Validates MAX31865 driver against a SPI register model.
 * The model enforces read-only RTD/fault registers, config write
 * behavior, fault detection, and Callendar-Van Dusen conversion.
 */

#include "test_harness.h"
#include "hw_model_max31865.h"
#include "../max31865.h"
#include <math.h>

static max31865_model_t model;

static const max31865_spi_t model_spi = {
    .cs_low  = max31865_model_cs_low,
    .cs_high = max31865_model_cs_high,
    .xfer    = max31865_model_xfer,
    .ctx     = &model,
};

static const max31865_cfg_t default_cfg = {
    .r_ref      = 430.0f,
    .r_nom      = 100.0f,
    .wire       = MAX31865_WIRE_2,
    .filter_50hz = false,
};

static void setup(void)
{
    max31865_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    max31865_t dev;
    max31865_err_t err = max31865_init(&dev, &model_spi, &default_cfg);
    ASSERT_EQ(err, MAX31865_OK);
}

TEST(test_init_null_param)
{
    setup();
    max31865_t dev;
    max31865_err_t err = max31865_init(&dev, NULL, &default_cfg);
    ASSERT_EQ(err, MAX31865_ERR_PARAM);
}

TEST(test_init_bad_rref)
{
    setup();
    max31865_t dev;
    max31865_cfg_t bad = default_cfg;
    bad.r_ref = 0.0f;
    max31865_err_t err = max31865_init(&dev, &model_spi, &bad);
    ASSERT_EQ(err, MAX31865_ERR_PARAM);
}

TEST(test_init_writes_config)
{
    setup();
    max31865_t dev;
    max31865_init(&dev, &model_spi, &default_cfg);
    /* Config should have BIAS (0x80) and AUTO (0x40) set */
    uint8_t cfg = model.hw.regs[MAX_REG_CONFIG];
    ASSERT_TRUE(cfg & MAX_CFG_BIAS);
    ASSERT_TRUE(cfg & MAX_CFG_AUTO);
}

TEST(test_init_3wire_config)
{
    setup();
    max31865_t dev;
    max31865_cfg_t cfg3 = default_cfg;
    cfg3.wire = MAX31865_WIRE_3;
    max31865_init(&dev, &model_spi, &cfg3);
    ASSERT_TRUE(model.hw.regs[MAX_REG_CONFIG] & MAX_CFG_3WIRE);
}

TEST(test_init_50hz_filter)
{
    setup();
    max31865_t dev;
    max31865_cfg_t cfg50 = default_cfg;
    cfg50.filter_50hz = true;
    max31865_init(&dev, &model_spi, &cfg50);
    ASSERT_TRUE(model.hw.regs[MAX_REG_CONFIG] & MAX_CFG_50HZ);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_rtd_raw)
{
    setup();
    /* Set RTD to 8360 (PT100 at ~25C) */
    max31865_model_set_rtd(&model, 8360);
    max31865_t dev;
    max31865_init(&dev, &model_spi, &default_cfg);

    uint16_t rtd_raw;
    max31865_err_t err = max31865_read_rtd_raw(&dev, &rtd_raw);
    ASSERT_EQ(err, MAX31865_OK);
    ASSERT_EQ(rtd_raw, 8360);
}

TEST(test_read_temperature_25c)
{
    setup();
    /* PT100 at 25C: R=109.73, rtd_raw = 109.73/430 * 32768 = 8360 */
    max31865_model_set_rtd(&model, 8360);
    max31865_t dev;
    max31865_init(&dev, &model_spi, &default_cfg);

    float temp;
    max31865_err_t err = max31865_read_temperature(&dev, &temp);
    ASSERT_EQ(err, MAX31865_OK);
    ASSERT_NEAR(temp, 25.0f, 2.0f);
}

TEST(test_read_temperature_0c)
{
    setup();
    /* PT100 at 0C: R=100.0, rtd_raw = 100/430 * 32768 = 7620 */
    max31865_model_set_rtd(&model, 7620);
    max31865_t dev;
    max31865_init(&dev, &model_spi, &default_cfg);

    float temp;
    max31865_read_temperature(&dev, &temp);
    ASSERT_NEAR(temp, 0.0f, 2.0f);
}

TEST(test_read_temperature_100c)
{
    setup();
    /* PT100 at 100C: R=138.51, rtd_raw = 138.51/430 * 32768 = 10554 */
    max31865_model_set_rtd(&model, 10554);
    max31865_t dev;
    max31865_init(&dev, &model_spi, &default_cfg);

    float temp;
    max31865_read_temperature(&dev, &temp);
    ASSERT_NEAR(temp, 100.0f, 3.0f);
}

/* --- fault tests ------------------------------------------------------- */

TEST(test_fault_flag_in_rtd_lsb)
{
    setup();
    max31865_model_set_fault(&model, MAX31865_FAULT_OVUV);
    max31865_t dev;
    max31865_init(&dev, &model_spi, &default_cfg);

    uint16_t rtd_raw;
    max31865_err_t err = max31865_read_rtd_raw(&dev, &rtd_raw);
    ASSERT_EQ(err, MAX31865_ERR_FAULT);
}

TEST(test_read_fault_register)
{
    setup();
    max31865_model_set_fault(&model, MAX31865_FAULT_HIGHTHRESH);
    max31865_t dev;
    max31865_init(&dev, &model_spi, &default_cfg);

    uint8_t fault;
    max31865_err_t err = max31865_read_fault(&dev, &fault);
    ASSERT_EQ(err, MAX31865_OK);
    ASSERT_EQ(fault, MAX31865_FAULT_HIGHTHRESH);
}

TEST(test_clear_fault)
{
    setup();
    max31865_model_set_fault(&model, MAX31865_FAULT_OVUV);
    max31865_t dev;
    max31865_init(&dev, &model_spi, &default_cfg);

    max31865_err_t err = max31865_clear_fault(&dev);
    ASSERT_EQ(err, MAX31865_OK);

    uint8_t fault;
    max31865_read_fault(&dev, &fault);
    ASSERT_EQ(fault, 0);
}

/* --- datasheet behavior tests ----------------------------------------- */

TEST(test_rtd_registers_readonly)
{
    setup();
    max31865_model_set_rtd(&model, 8360);
    uint8_t original_msb = model.hw.regs[MAX_REG_RTD_MSB];
    hw_model__write_reg(&model.hw, MAX_REG_RTD_MSB, 0xFF);
    ASSERT_EQ(model.hw.regs[MAX_REG_RTD_MSB], original_msb);
}

TEST(test_fault_register_readonly)
{
    setup();
    max31865_model_set_fault(&model, 0x80);
    hw_model__write_reg(&model.hw, MAX_REG_FAULT, 0x00);
    ASSERT_EQ(model.hw.regs[MAX_REG_FAULT], 0x80);
}

TEST(test_temperature_increases_with_rtd)
{
    setup();
    max31865_t dev;
    max31865_init(&dev, &model_spi, &default_cfg);

    max31865_model_set_rtd(&model, 7620);  /* ~0C */
    float t1;
    max31865_read_temperature(&dev, &t1);

    max31865_model_set_rtd(&model, 10554); /* ~100C */
    float t2;
    max31865_read_temperature(&dev, &t2);

    ASSERT_TRUE(t2 > t1);
}

TEST_MAIN("max31865 hardware model")
    RUN(test_init_ok);
    RUN(test_init_null_param);
    RUN(test_init_bad_rref);
    RUN(test_init_writes_config);
    RUN(test_init_3wire_config);
    RUN(test_init_50hz_filter);
    RUN(test_read_rtd_raw);
    RUN(test_read_temperature_25c);
    RUN(test_read_temperature_0c);
    RUN(test_read_temperature_100c);
    RUN(test_fault_flag_in_rtd_lsb);
    RUN(test_read_fault_register);
    RUN(test_clear_fault);
    RUN(test_rtd_registers_readonly);
    RUN(test_fault_register_readonly);
    RUN(test_temperature_increases_with_rtd);
TEST_MAIN_END()
