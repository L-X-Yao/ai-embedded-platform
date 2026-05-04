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

/* --- serial number ---------------------------------------------------- */

TEST(test_read_serial)
{
    setup();
    si7021_model_set_serial(&model, 0x12345678, 0xABCDEF01);
    si7021_t dev;
    si7021_init(&dev, &model_i2c);

    uint32_t a, b;
    si7021_err_t err = si7021_read_serial(&dev, &a, &b);
    ASSERT_EQ(err, SI7021_OK);
    ASSERT_EQ_HEX(a, 0x12345678);
    ASSERT_EQ_HEX(b, 0xABCDEF01);
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
    RUN(test_reset);
    RUN(test_read_serial);
    RUN(test_temperature_varies);
    RUN(test_humidity_varies);
    RUN(test_multiple_reads_consistent);
TEST_MAIN_END()
