// SPDX-License-Identifier: MIT
/*
 * test_hw_model_si7021.c — tests using command-based hardware model
 *
 * Validates Si7021 driver against a command-response model.
 * The model maps single-byte commands to responses with CRC-8.
 * Exercises humidity, temperature, both-read, resolution, reset,
 * serial number, and varying environment.
 */

#include "test_harness.h"
#include "hw_model_si7021.h"
#include "../si7021.h"
#include <math.h>

static si7021_model_t model;

static const si7021_i2c_t model_i2c = {
    .write    = si7021_model_write,
    .read     = si7021_model_read,
    .delay_ms = si7021_model_delay,
    .ctx      = &model,
};

static void setup(void)
{
    si7021_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    si7021_t dev;
    si7021_err_t err = si7021_init(&dev, &model_i2c);
    ASSERT_EQ(err, SI7021_OK);
}

TEST(test_init_null_param)
{
    setup();
    si7021_t dev;
    si7021_err_t err = si7021_init(&dev, NULL);
    ASSERT_EQ(err, SI7021_ERR_PARAM);
}

/* --- humidity read tests ---------------------------------------------- */

TEST(test_read_humidity_50)
{
    setup();
    si7021_model_set_humidity(&model, 50.0f);
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    float rh;
    si7021_err_t err = si7021_read_humidity(&dev, &rh);
    ASSERT_EQ(err, SI7021_OK);
    ASSERT_NEAR(rh, 50.0f, 1.0f);
}

TEST(test_read_humidity_low)
{
    setup();
    si7021_model_set_humidity(&model, 10.0f);
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    float rh;
    si7021_read_humidity(&dev, &rh);
    ASSERT_NEAR(rh, 10.0f, 1.0f);
}

TEST(test_read_humidity_high)
{
    setup();
    si7021_model_set_humidity(&model, 90.0f);
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    float rh;
    si7021_read_humidity(&dev, &rh);
    ASSERT_NEAR(rh, 90.0f, 1.0f);
}

/* --- temperature read tests ------------------------------------------- */

TEST(test_read_temperature_25c)
{
    setup();
    si7021_model_set_temperature(&model, 25.0f);
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    float temp;
    si7021_err_t err = si7021_read_temperature(&dev, &temp);
    ASSERT_EQ(err, SI7021_OK);
    ASSERT_NEAR(temp, 25.0f, 1.0f);
}

TEST(test_read_temperature_cold)
{
    setup();
    si7021_model_set_temperature(&model, -10.0f);
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    float temp;
    si7021_read_temperature(&dev, &temp);
    ASSERT_NEAR(temp, -10.0f, 1.0f);
}

/* --- read both -------------------------------------------------------- */

TEST(test_read_both)
{
    setup();
    si7021_model_set_temperature(&model, 25.0f);
    si7021_model_set_humidity(&model, 50.0f);
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    float rh, temp;
    si7021_err_t err = si7021_read_both(&dev, &rh, &temp);
    ASSERT_EQ(err, SI7021_OK);
    ASSERT_NEAR(rh, 50.0f, 1.0f);
    ASSERT_NEAR(temp, 25.0f, 1.0f);
}

/* --- CRC validation --------------------------------------------------- */

TEST(test_crc_matches_driver)
{
    uint8_t data1[2] = { 0xBE, 0xEF };
    uint8_t model_crc = si7021_model_crc8(data1, 2);
    uint8_t driver_crc = si7021_crc8(data1, 2);
    ASSERT_EQ(model_crc, driver_crc);
}

TEST(test_crc_zero)
{
    uint8_t data2[2] = { 0x00, 0x00 };
    ASSERT_EQ(si7021_model_crc8(data2, 2), si7021_crc8(data2, 2));
}

/* --- resolution ------------------------------------------------------- */

TEST(test_set_resolution)
{
    setup();
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    si7021_err_t err = si7021_set_resolution(&dev, SI7021_RES_12BIT_8BIT);
    ASSERT_EQ(err, SI7021_OK);
    /* User register should have resolution bits updated */
    ASSERT_TRUE((model.user_reg & 0x81) == 0x01);
}

/* --- conversion timing ------------------------------------------------ */

/* p.20: "Device will NACK the slave address byte until conversion is
 * complete." Table 2 (p.5): 12 ms for RH plus 10.8 ms for the temperature
 * measurement that comes with it. */
TEST(test_model_nacks_read_during_rh_conversion)
{
    setup();
    uint8_t cmd = SI_CMD_MEASURE_RH_NOHOLD;
    uint8_t buf[3];

    ASSERT_TRUE(si7021_model_write(&model, SI7021_DEV_ADDR, &cmd, 1));
    ASSERT_FALSE(si7021_model_read(&model, SI7021_DEV_ADDR, buf, 3));
    si7021_model_delay(&model, 22);
    ASSERT_FALSE(si7021_model_read(&model, SI7021_DEV_ADDR, buf, 3));
    si7021_model_delay(&model, 1);
    ASSERT_TRUE(si7021_model_read(&model, SI7021_DEV_ADDR, buf, 3));
}

TEST(test_model_nacks_read_during_temp_conversion)
{
    setup();
    uint8_t cmd = SI_CMD_MEASURE_TEMP_NOHOLD;
    uint8_t buf[3];

    ASSERT_TRUE(si7021_model_write(&model, SI7021_DEV_ADDR, &cmd, 1));
    si7021_model_delay(&model, 10);
    ASSERT_FALSE(si7021_model_read(&model, SI7021_DEV_ADDR, buf, 3));
    si7021_model_delay(&model, 1);
    ASSERT_TRUE(si7021_model_read(&model, SI7021_DEV_ADDR, buf, 3));
}

/* --- reset ------------------------------------------------------------ */

TEST(test_reset)
{
    setup();
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    si7021_err_t err = si7021_reset(&dev);
    ASSERT_EQ(err, SI7021_OK);
    ASSERT_EQ(model.reset_count, 1);
    /* User register back to default */
    ASSERT_EQ(model.user_reg, SI_USER_REG_DEFAULT);
}

/* Table 2 (p.5): ready within 15 ms "after issuing a software reset command",
 * so the next command must not arrive before that. */
TEST(test_read_straight_after_reset)
{
    setup();
    si7021_t dev;
    si7021_init(&dev, &model_i2c);
    ASSERT_EQ(si7021_reset(&dev), SI7021_OK);

    float rh = 0.0f;
    ASSERT_EQ(si7021_read_humidity(&dev, &rh), SI7021_OK);
    ASSERT_NEAR(rh, 50.0f, 0.5f);
}

/* --- serial number ---------------------------------------------------- */

TEST(test_read_serial)
{
    setup();
    si7021_model_set_serial(&model, 0x12345678, 0x15CDEF01);
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    uint32_t a, b;
    si7021_err_t err = si7021_read_serial(&dev, &a, &b);
    ASSERT_EQ(err, SI7021_OK);
    ASSERT_EQ_HEX(a, 0x12345678);
    ASSERT_EQ_HEX(b, 0x15CDEF01);
}

/* p.24: SNB_3 "contains the device identification", "0x15=21=Si7021" */
TEST(test_default_serial_names_a_si7021)
{
    setup();
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    uint32_t a, b;
    ASSERT_EQ(si7021_read_serial(&dev, &a, &b), SI7021_OK);
    ASSERT_EQ_HEX(b >> 24, 0x15);
}

/* --- codes the manual fixes ------------------------------------------- */

/* A refused transfer returns 0xFFFF: no measurement ends in the bits 11. */
static uint16_t measure_code(uint8_t cmd)
{
    uint8_t buf[3] = { 0, 0, 0 };

    if (!si7021_model_write(&model, SI7021_DEV_ADDR, &cmd, 1))
        return 0xFFFF;
    si7021_model_delay(&model, 25);
    if (!si7021_model_read(&model, SI7021_DEV_ADDR, buf, 3))
        return 0xFFFF;
    return (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
}

/* p.21: "A humidity measurement will always return XXXXXX10 in the LSB
 * field." */
TEST(test_rh_code_always_ends_in_10)
{
    setup();
    for (int rh = -6; rh <= 119; rh++) {
        si7021_model_set_humidity(&model, (float)rh + 0.37f);
        ASSERT_EQ_HEX(measure_code(SI_CMD_MEASURE_RH_NOHOLD) & 0x03u, 0x02);
    }
}

/* p.22: "A temperature measurement will always return XXXXXX00 in the LSB
 * field." */
TEST(test_temp_code_always_ends_in_00)
{
    setup();
    for (int t = -46; t <= 128; t++) {
        si7021_model_set_temperature(&model, (float)t + 0.37f);
        ASSERT_EQ_HEX(measure_code(SI_CMD_MEASURE_TEMP_NOHOLD) & 0x03u, 0x00);
    }
}

/* p.21: 0xE0 "returns the temperature value measured during the relative
 * humidity measurement", so it is a temperature code too. */
TEST(test_temp_from_rh_code_ends_in_00)
{
    setup();
    si7021_model_set_temperature(&model, 21.37f);
    (void)measure_code(SI_CMD_MEASURE_RH_NOHOLD);

    uint8_t cmd = SI_CMD_READ_TEMP_FROM_RH;
    uint8_t buf[2] = { 0xFF, 0xFF };
    ASSERT_TRUE(si7021_model_write(&model, SI7021_DEV_ADDR, &cmd, 1));
    ASSERT_TRUE(si7021_model_read(&model, SI7021_DEV_ADDR, buf, 2));
    ASSERT_EQ_HEX(buf[1] & 0x03u, 0x00);
}

/* Top of the range: the code stays a 16-bit word, it does not wrap to 0. */
TEST(test_codes_saturate_at_the_top_of_the_range)
{
    setup();
    si7021_model_set_humidity(&model, 500.0f);
    ASSERT_EQ_HEX(measure_code(SI_CMD_MEASURE_RH_NOHOLD), 0xFFFE);
    si7021_model_set_temperature(&model, 500.0f);
    ASSERT_EQ_HEX(measure_code(SI_CMD_MEASURE_TEMP_NOHOLD), 0xFFFC);
}

/* --- user register ---------------------------------------------------- */

/* A refused transfer returns 0: the reserved bits never read back as 0. */
static uint8_t read_user_reg(void)
{
    uint8_t cmd = SI_CMD_READ_USER_REG;
    uint8_t val = 0;

    if (!si7021_model_write(&model, SI7021_DEV_ADDR, &cmd, 1))
        return 0;
    if (!si7021_model_read(&model, SI7021_DEV_ADDR, &val, 1))
        return 0;
    return val;
}

static void write_user_reg(uint8_t val)
{
    uint8_t buf[2] = { SI_CMD_WRITE_USER_REG, val };

    ASSERT_TRUE(si7021_model_write(&model, SI7021_DEV_ADDR, buf, 2));
}

/* p.26: "Reset Settings = 0011_1010" */
TEST(test_user_reg_reset_value)
{
    setup();
    ASSERT_EQ_HEX(read_user_reg(), 0x3A);
}

/* p.26: "reserved register bits will always read back as "1," and are not
 * affected by write operations", and VDDS has type R. Only RES1, HTRE and
 * RES0 take the written value. */
TEST(test_user_reg_keeps_vdds_and_reserved_bits)
{
    setup();
    write_user_reg(0x00);
    ASSERT_EQ_HEX(read_user_reg(), 0x3A);
    write_user_reg(0xFF);
    ASSERT_EQ_HEX(read_user_reg(), 0xBF);
    write_user_reg(0x40);
    ASSERT_EQ_HEX(read_user_reg(), 0x3A);
}

/* VDDS is set by the part: 1 = VDD Low. A write cannot clear it. */
TEST(test_user_reg_write_does_not_clear_vdd_low)
{
    setup();
    model.user_reg |= 0x40u;
    write_user_reg(0x3A);
    ASSERT_EQ_HEX(read_user_reg(), 0x7A);
}

/* p.26 table: 00 = RH 12 / T 14, 01 = 8 / 12, 10 = 10 / 13, 11 = 11 / 11,
 * with RES1 in D7 and RES0 in D0. */
TEST(test_set_resolution_sets_the_manual_codes)
{
    static const struct {
        si7021_resolution_t res;
        uint8_t reg;
    } cases[] = {
        { SI7021_RES_14BIT_12BIT, 0x3A },
        { SI7021_RES_12BIT_8BIT,  0x3B },
        { SI7021_RES_13BIT_10BIT, 0xBA },
        { SI7021_RES_11BIT_11BIT, 0xBB },
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        setup();
        si7021_t dev;
        si7021_init(&dev, &model_i2c);
        ASSERT_EQ(si7021_set_resolution(&dev, cases[i].res), SI7021_OK);
        ASSERT_EQ_HEX(read_user_reg(), cases[i].reg);
    }
}

/* p.26: "prior to a write operation, registers should be read", so the
 * heater bit must survive a change of resolution. */
TEST(test_set_resolution_keeps_the_heater_bit)
{
    setup();
    write_user_reg(0x3E);   /* HTRE = 1 */
    si7021_t dev;
    si7021_init(&dev, &model_i2c);
    ASSERT_EQ(si7021_set_resolution(&dev, SI7021_RES_11BIT_11BIT), SI7021_OK);
    ASSERT_EQ_HEX(read_user_reg(), 0xBF);
}

/* --- varying environment ---------------------------------------------- */

TEST(test_temperature_varies)
{
    setup();
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    si7021_model_set_temperature(&model, 10.0f);
    float t1;
    si7021_read_temperature(&dev, &t1);

    si7021_model_set_temperature(&model, 40.0f);
    float t2;
    si7021_read_temperature(&dev, &t2);

    ASSERT_TRUE(t2 > t1);
}

TEST(test_humidity_varies)
{
    setup();
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    si7021_model_set_humidity(&model, 20.0f);
    float rh1;
    si7021_read_humidity(&dev, &rh1);

    si7021_model_set_humidity(&model, 80.0f);
    float rh2;
    si7021_read_humidity(&dev, &rh2);

    ASSERT_TRUE(rh2 > rh1);
}

TEST(test_multiple_reads_consistent)
{
    setup();
    si7021_model_set_temperature(&model, 25.0f);
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    float t1, t2;
    si7021_read_temperature(&dev, &t1);
    si7021_read_temperature(&dev, &t2);
    ASSERT_NEAR(t1, t2, 0.01f);
}

TEST_MAIN("si7021 hardware model")
    RUN(test_init_ok);
    RUN(test_init_null_param);
    RUN(test_read_humidity_50);
    RUN(test_read_humidity_low);
    RUN(test_read_humidity_high);
    RUN(test_read_temperature_25c);
    RUN(test_read_temperature_cold);
    RUN(test_read_both);
    RUN(test_crc_matches_driver);
    RUN(test_crc_zero);
    RUN(test_set_resolution);
    RUN(test_model_nacks_read_during_rh_conversion);
    RUN(test_model_nacks_read_during_temp_conversion);
    RUN(test_reset);
    RUN(test_read_straight_after_reset);
    RUN(test_read_serial);
    RUN(test_default_serial_names_a_si7021);
    RUN(test_rh_code_always_ends_in_10);
    RUN(test_temp_code_always_ends_in_00);
    RUN(test_temp_from_rh_code_ends_in_00);
    RUN(test_codes_saturate_at_the_top_of_the_range);
    RUN(test_user_reg_reset_value);
    RUN(test_user_reg_keeps_vdds_and_reserved_bits);
    RUN(test_user_reg_write_does_not_clear_vdd_low);
    RUN(test_set_resolution_sets_the_manual_codes);
    RUN(test_set_resolution_keeps_the_heater_bit);
    RUN(test_temperature_varies);
    RUN(test_humidity_varies);
    RUN(test_multiple_reads_consistent);
TEST_MAIN_END()
