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

/* p.3 and p.4 note 5: the first conversion takes the single conversion time,
 * then they come at the continuous rate. Maximum figures, 60 Hz notch. */
#define FIRST_60HZ_US 55000u
#define NEXT_60HZ_US  17600u

static void wait_first_conversion(void)
{
    max31865_model_advance_us(&model, FIRST_60HZ_US);
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
    wait_first_conversion();

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
    wait_first_conversion();

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
    wait_first_conversion();

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
    wait_first_conversion();

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
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0xC0);
    wait_first_conversion();
    uint8_t original_msb = model.hw.regs[MAX_REG_RTD_MSB];
    ASSERT_EQ(original_msb, (8360 << 1) >> 8);
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
    wait_first_conversion();
    float t1;
    max31865_read_temperature(&dev, &t1);

    max31865_model_set_rtd(&model, 10554); /* ~100C */
    max31865_model_advance_us(&model, NEXT_60HZ_US);
    float t2;
    max31865_read_temperature(&dev, &t2);

    ASSERT_TRUE(t2 > t1);
}

/* --- the model, from the manual ---------------------------------------- */

static uint16_t rtd_word(void)
{
    return (uint16_t)((model.hw.regs[MAX_REG_RTD_MSB] << 8) |
                      model.hw.regs[MAX_REG_RTD_LSB]);
}

/* p.13, Table 1: POR values */
TEST(test_por_values)
{
    setup();
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_CONFIG], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_RTD_MSB], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_RTD_LSB], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_HFAULT_H], 0xFF);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_HFAULT_L], 0xFF);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_LFAULT_H], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_LFAULT_L], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_FAULT], 0x00);
}

/* p.13: automatic conversions need D6, and VBIAS needs D7 */
TEST(test_no_conversion_without_bias_and_auto)
{
    static const uint8_t off[] = {0x00, 0x80, 0x40};
    for (size_t i = 0; i < sizeof(off); i++) {
        setup();
        hw_model__write_reg(&model.hw, MAX_REG_CONFIG, off[i]);
        max31865_model_advance_us(&model, 200000);
        ASSERT_EQ_HEX(rtd_word(), 0x0000);
    }
}

/* p.4 note 5: "The first conversion after enabling continuous conversion mode
 * takes a time equal to the single conversion time" */
TEST(test_first_conversion_takes_the_single_time)
{
    setup();
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0xC0);
    max31865_model_advance_us(&model, FIRST_60HZ_US - 1);
    ASSERT_EQ_HEX(rtd_word(), 0x0000);
    max31865_model_advance_us(&model, 1);
    ASSERT_EQ(rtd_word() >> 1, 8360);

    setup();
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0xC1);  /* 50 Hz notch */
    max31865_model_advance_us(&model, 65999);
    ASSERT_EQ_HEX(rtd_word(), 0x0000);
    max31865_model_advance_us(&model, 1);
    ASSERT_EQ(rtd_word() >> 1, 8360);
}

/* p.3: tCONV continuous, 17.6 ms at 60 Hz and 21 ms at 50 Hz, at most */
TEST(test_later_conversions_come_at_the_continuous_rate)
{
    setup();
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0xC0);
    wait_first_conversion();
    max31865_model_set_rtd(&model, 9000);
    max31865_model_advance_us(&model, NEXT_60HZ_US - 1);
    ASSERT_EQ(rtd_word() >> 1, 8360);
    max31865_model_advance_us(&model, 1);
    ASSERT_EQ(rtd_word() >> 1, 9000);

    setup();
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0xC1);
    max31865_model_advance_us(&model, 66000);
    max31865_model_set_rtd(&model, 9000);
    max31865_model_advance_us(&model, 20999);
    ASSERT_EQ(rtd_word() >> 1, 8360);
    max31865_model_advance_us(&model, 1);
    ASSERT_EQ(rtd_word() >> 1, 9000);
}

/* p.13: "Write 0 to this bit to exit automatic conversion mode" */
TEST(test_leaving_auto_stops_the_conversions)
{
    setup();
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0xC0);
    wait_first_conversion();
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0x80);
    max31865_model_set_rtd(&model, 9000);
    max31865_model_advance_us(&model, 200000);
    ASSERT_EQ(rtd_word() >> 1, 8360);
}

/* p.16: "The RTD High bit in the Fault Status Register is set if the RTD
 * resistance register value is greater than or equal to the value in the High
 * Fault Threshold register", and p.21: an open RTD reads full scale */
TEST(test_full_scale_trips_the_por_high_threshold)
{
    setup();
    max31865_model_set_rtd(&model, 0x7FFF);
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0xC0);
    wait_first_conversion();
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_FAULT], 0x80);
    ASSERT_EQ_HEX(rtd_word(), 0xFFFF);
}

/* p.16: "The RTD Low bit in the Fault Status Register is set if the RTD
 * resistance value is less than or equal to the value in the Low Fault
 * Threshold register. The POR value of the Low Fault Threshold register is
 * 0000h." So a finished conversion at code 0 has its fault flag set. */
TEST(test_code_zero_trips_the_por_low_threshold)
{
    setup();
    max31865_model_set_rtd(&model, 0);
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0xC0);
    wait_first_conversion();
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_FAULT], 0x40);
    ASSERT_EQ_HEX(rtd_word(), 0x0001);
}

/* p.16: the thresholds have the format of the RTD registers */
TEST(test_written_thresholds_apply)
{
    setup();
    hw_model__write_reg(&model.hw, MAX_REG_HFAULT_H, 0x20);
    hw_model__write_reg(&model.hw, MAX_REG_HFAULT_L, 0x00);  /* code 0x1000 */
    max31865_model_set_rtd(&model, 0x1000);
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0xC0);
    wait_first_conversion();
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_FAULT], 0x80);

    setup();
    hw_model__write_reg(&model.hw, MAX_REG_LFAULT_H, 0x20);
    hw_model__write_reg(&model.hw, MAX_REG_LFAULT_L, 0x00);
    max31865_model_set_rtd(&model, 0x1000);
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0xC0);
    wait_first_conversion();
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_FAULT], 0x40);
}

/* p.16: "The Fault Status register latches any detected fault bits" */
TEST(test_fault_bits_latch)
{
    setup();
    max31865_model_set_rtd(&model, 0x7FFF);
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0xC0);
    wait_first_conversion();
    max31865_model_set_rtd(&model, 8360);
    max31865_model_advance_us(&model, NEXT_60HZ_US);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_FAULT], 0x80);
    ASSERT_EQ_HEX(rtd_word() & 0x0001, 0x0001);
}

/* p.14: "Write a 1 to this bit while writing 0 to bits D5, D3, and D2 to return
 * all fault status bits", and "The fault status clear bit D1, self-clears" */
TEST(test_fault_clear_rules)
{
    setup();
    max31865_model_set_fault(&model, 0x80);
    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0x82 | 0x08);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_FAULT], 0x80);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_CONFIG] & 0x02, 0x00);

    hw_model__write_reg(&model.hw, MAX_REG_CONFIG, 0x82);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_FAULT], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[MAX_REG_CONFIG], 0x80);
    ASSERT_EQ_HEX(rtd_word() & 0x0001, 0x0000);
}

/* --- the driver on it --------------------------------------------------- */

/* p.13: the RTD registers reset to 00h. No result exists before the first
 * conversion ends, and that word must not pass for 0 ohm. */
TEST(test_read_before_the_first_conversion)
{
    setup();
    max31865_t dev;
    ASSERT_EQ(max31865_init(&dev, &model_spi, &default_cfg), MAX31865_OK);

    uint16_t raw = 77;
    float temp = 77.0f;
    ASSERT_EQ(max31865_read_rtd_raw(&dev, &raw), MAX31865_ERR_NOT_READY);
    ASSERT_EQ(max31865_read_temperature(&dev, &temp), MAX31865_ERR_NOT_READY);
    ASSERT_EQ(raw, 77);
    ASSERT_NEAR(temp, 77.0f, 0.001f);

    wait_first_conversion();
    ASSERT_EQ(max31865_read_rtd_raw(&dev, &raw), MAX31865_OK);
    ASSERT_EQ(raw, 8360);
}

/* p.16: a shorted RTD at code 0 is a fault, not a conversion still to come */
TEST(test_a_dead_short_reads_as_a_fault)
{
    setup();
    max31865_model_set_rtd(&model, 0);
    max31865_t dev;
    ASSERT_EQ(max31865_init(&dev, &model_spi, &default_cfg), MAX31865_OK);
    wait_first_conversion();

    uint16_t raw;
    uint8_t fault = 0;
    ASSERT_EQ(max31865_read_rtd_raw(&dev, &raw), MAX31865_ERR_FAULT);
    ASSERT_EQ(max31865_read_fault(&dev, &fault), MAX31865_OK);
    ASSERT_EQ_HEX(fault, MAX31865_FAULT_LOWTHRESH);
}

/* p.21: "An open RTD element results in a full-scale reading" */
TEST(test_an_open_rtd_reads_as_a_fault)
{
    setup();
    max31865_model_set_rtd(&model, 0x7FFF);
    max31865_t dev;
    ASSERT_EQ(max31865_init(&dev, &model_spi, &default_cfg), MAX31865_OK);
    wait_first_conversion();

    float temp;
    ASSERT_EQ(max31865_read_temperature(&dev, &temp), MAX31865_ERR_FAULT);
}

/* p.14: the clear takes D0 of the RTD LSB with it, and p.4 note 5 applies again
 * because the driver leaves and re-enters auto mode */
TEST(test_clear_fault_lets_reads_work_again)
{
    setup();
    max31865_model_set_rtd(&model, 0x7FFF);
    max31865_t dev;
    ASSERT_EQ(max31865_init(&dev, &model_spi, &default_cfg), MAX31865_OK);
    wait_first_conversion();
    max31865_model_set_rtd(&model, 8360);

    ASSERT_EQ(max31865_clear_fault(&dev), MAX31865_OK);
    wait_first_conversion();

    uint16_t raw;
    ASSERT_EQ(max31865_read_rtd_raw(&dev, &raw), MAX31865_OK);
    ASSERT_EQ(raw, 8360);
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
    RUN(test_por_values);
    RUN(test_no_conversion_without_bias_and_auto);
    RUN(test_first_conversion_takes_the_single_time);
    RUN(test_later_conversions_come_at_the_continuous_rate);
    RUN(test_leaving_auto_stops_the_conversions);
    RUN(test_full_scale_trips_the_por_high_threshold);
    RUN(test_code_zero_trips_the_por_low_threshold);
    RUN(test_written_thresholds_apply);
    RUN(test_fault_bits_latch);
    RUN(test_fault_clear_rules);
    RUN(test_read_before_the_first_conversion);
    RUN(test_a_dead_short_reads_as_a_fault);
    RUN(test_an_open_rtd_reads_as_a_fault);
    RUN(test_clear_fault_lets_reads_work_again);
TEST_MAIN_END()
