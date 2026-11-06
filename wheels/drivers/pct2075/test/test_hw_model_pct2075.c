// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_pct2075.h"
#include "pct2075.h"

static const pct2075_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = hw_model_delay_ms,
    .ctx       = NULL,
};

static const pct2075_cfg_t default_cfg = {
    .addr         = PCT2075_DEV_ADDR,
    .write_config = false,
    .conf_val     = 0x00,
};

static pct2075_t dev;

static pct2075_err_t setup(void)
{
    hw_model_reset();
    return pct2075_init(&dev, &hw_i2c, &default_cfg);
}

static uint8_t read8(uint8_t reg)
{
    uint8_t b = 0;
    hw_model_read_regs(NULL, PCT2075_DEV_ADDR, reg, &b, 1);
    return b;
}

static uint16_t read16(uint8_t reg)
{
    uint8_t b[2] = {0, 0};
    hw_model_read_regs(NULL, PCT2075_DEV_ADDR, reg, b, 2);
    return (uint16_t)((uint16_t)b[0] << 8 | b[1]);
}

/* Table 7: POR state of every register */
TEST(test_hw_por_state)
{
    hw_model_reset();
    ASSERT_EQ_HEX(read16(0x00), 0x0000);  /* Temp */
    ASSERT_EQ_HEX(read8(0x01), 0x00);     /* Conf */
    ASSERT_EQ_HEX(read16(0x02), 0x4B00);  /* Thyst */
    ASSERT_EQ_HEX(read16(0x03), 0x5000);  /* Tos */
    ASSERT_EQ_HEX(read8(0x04), 0x00);     /* Tidle */
}

/* Table 7: Conf "contains a single 8-bit data byte" of its own, so it
 * shares nothing with the two bytes of Temp. 25.125 C is 0C9h, in bits
 * 15:5. */
TEST(test_hw_conf_write_leaves_temp_alone)
{
    hw_model_reset();
    hw_model_delay_ms(NULL, 28);
    hw_model_inject_temp(25.125f);
    hw_model_write_reg(NULL, PCT2075_DEV_ADDR, 0x01, 0x18);
    ASSERT_EQ_HEX(read16(0x00), 0x1920);
    ASSERT_EQ_HEX(read8(0x01), 0x18);
}

/* Table 7: Temp is "read only" */
TEST(test_hw_temp_is_read_only)
{
    hw_model_reset();
    hw_model_write_reg(NULL, PCT2075_DEV_ADDR, 0x00, 0x7F);
    ASSERT_EQ_HEX(read16(0x00), 0x0000);
}

/* p.7 "The temp reading data is 0 C and not updated until the first
 * conversion is completed in about 28 ms." */
TEST(test_hw_temp_is_zero_until_first_conversion)
{
    hw_model_reset();
    hw_model_inject_temp(25.0f);
    ASSERT_EQ_HEX(read16(0x00), 0x0000);
    hw_model_delay_ms(NULL, 27);
    ASSERT_EQ_HEX(read16(0x00), 0x0000);
    hw_model_delay_ms(NULL, 1);
    ASSERT_EQ_HEX(read16(0x00), 0x1900);
}

/* p.6 "In shutdown mode, the device becomes idle, data conversion is
 * disabled and the Temp register holds the latest result". "The
 * temperature conversion is initiated when the device is powered-up or
 * put back into normal mode from shutdown." */
TEST(test_hw_shutdown_holds_temp)
{
    hw_model_reset();
    hw_model_delay_ms(NULL, 28);
    hw_model_inject_temp(25.0f);

    hw_model_write_reg(NULL, PCT2075_DEV_ADDR, 0x01, 0x01);
    hw_model_inject_temp(30.0f);
    hw_model_delay_ms(NULL, 100);
    ASSERT_EQ_HEX(read16(0x00), 0x1900);

    hw_model_write_reg(NULL, PCT2075_DEV_ADDR, 0x01, 0x00);
    ASSERT_EQ_HEX(read16(0x00), 0x1900);
    hw_model_delay_ms(NULL, 28);
    ASSERT_EQ_HEX(read16(0x00), 0x1E00);
}

/* p.7: the first read after init has to be a conversion, not the 0 C of
 * the POR state. */
TEST(test_hw_first_read_after_init_is_a_conversion)
{
    hw_model_reset();
    hw_model_inject_temp(25.0f);
    pct2075_t d;
    ASSERT_EQ(pct2075_init(&d, &hw_i2c, &default_cfg), PCT2075_OK);

    float temp = 0.0f;
    ASSERT_EQ(pct2075_read(&d, &temp), PCT2075_OK);
    ASSERT_NEAR(temp, 25.0f, 0.001f);
}

TEST(test_hw_init_por_defaults)
{
    pct2075_err_t e = setup();
    ASSERT_EQ(e, PCT2075_OK);
    /* POR temperature = 0 C */
    float temp = 99.0f;
    e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 0.0f, 0.01f);
}

TEST(test_hw_read_25c_round_trip)
{
    setup();
    hw_model_inject_temp(25.0f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 25.0f, 0.125f);
}

TEST(test_hw_read_neg25c_round_trip)
{
    setup();
    hw_model_inject_temp(-25.0f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, -25.0f, 0.125f);
}

TEST(test_hw_read_125c_max)
{
    setup();
    hw_model_inject_temp(125.0f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 125.0f, 0.125f);
}

TEST(test_hw_read_neg55c_min)
{
    setup();
    hw_model_inject_temp(-55.0f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, -55.0f, 0.125f);
}

TEST(test_hw_resolution_0_125)
{
    setup();
    hw_model_inject_temp(25.125f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 25.125f, 0.125f);
}

TEST(test_hw_init_reads_temp_reg)
{
    hw_model_reset();
    pct2075_t d;
    (void)pct2075_init(&d, &hw_i2c, &default_cfg);
    /* Init must read the temperature register to verify I2C comms */
    ASSERT_TRUE(hw_read_count >= 1);
}

TEST(test_hw_config_write)
{
    hw_model_reset();
    pct2075_t d;
    pct2075_cfg_t cfg = {
        .addr         = PCT2075_DEV_ADDR,
        .write_config = true,
        .conf_val     = 0x18,  /* Table 10: B[4:3] = 11, queue value = 6 */
    };
    pct2075_err_t e = pct2075_init(&d, &hw_i2c, &cfg);
    ASSERT_EQ(e, PCT2075_OK);
    /* Config register at address 0x01 should hold 0x18 */
    ASSERT_EQ_HEX(read8(0x01), 0x18);
    ASSERT_TRUE(hw_write_count >= 1);
}

TEST(test_hw_zero_roundtrip)
{
    setup();
    hw_model_inject_temp(0.0f);
    float temp = 99.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 0.0f, 0.01f);
}

TEST(test_hw_neg0_125_roundtrip)
{
    setup();
    hw_model_inject_temp(-0.125f);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, -0.125f, 0.001f);
}

TEST_MAIN("pct2075_hw_model")
    RUN(test_hw_por_state);
    RUN(test_hw_conf_write_leaves_temp_alone);
    RUN(test_hw_temp_is_read_only);
    RUN(test_hw_temp_is_zero_until_first_conversion);
    RUN(test_hw_shutdown_holds_temp);
    RUN(test_hw_first_read_after_init_is_a_conversion);
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_25c_round_trip);
    RUN(test_hw_read_neg25c_round_trip);
    RUN(test_hw_read_125c_max);
    RUN(test_hw_read_neg55c_min);
    RUN(test_hw_resolution_0_125);
    RUN(test_hw_init_reads_temp_reg);
    RUN(test_hw_config_write);
    RUN(test_hw_zero_roundtrip);
    RUN(test_hw_neg0_125_roundtrip);
TEST_MAIN_END()
