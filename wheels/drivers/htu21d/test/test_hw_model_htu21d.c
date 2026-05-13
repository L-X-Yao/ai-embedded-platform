/*
 * test_hw_model_htu21d.c — tests using command-based hardware model
 *
 * Validates HTU21D driver against a command-response model.
 * The model maps single-byte commands to 3-byte responses with CRC-8.
 * Exercises measurement, reset, CRC validation, and varying environment.
 */

#include "test_harness.h"
#include "hw_model_htu21d.h"
#include "../htu21d.h"
#include <math.h>

static htu21d_model_t model;

static const htu21d_i2c_t model_i2c = {
    .write_cmd = htu21d_model_write_cmd,
    .read_data = htu21d_model_read_data,
    .delay_ms  = htu21d_model_delay,
    .ctx       = &model,
};

static void setup(void)
{
    htu21d_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    htu21d_t dev;
    htu21d_err_t err = htu21d_init(&dev, &model_i2c);
    ASSERT_EQ(err, HTU21D_OK);
    /* Init sends soft reset */
    ASSERT_EQ(model.reset_count, 1);
}

TEST(test_init_null_param)
{
    setup();
    htu21d_t dev;
    htu21d_err_t err = htu21d_init(&dev, NULL);
    ASSERT_EQ(err, HTU21D_ERR_PARAM);
}

/* --- temperature read tests -------------------------------------------- */

TEST(test_read_temp_25c)
{
    setup();
    htu21d_model_set_temperature(&model, 25.0f);
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);

    float temp;
    htu21d_err_t err = htu21d_read_temperature(&dev, &temp);
    ASSERT_EQ(err, HTU21D_OK);
    ASSERT_NEAR(temp, 25.0f, 0.5f);
}

TEST(test_read_temp_0c)
{
    setup();
    htu21d_model_set_temperature(&model, 0.0f);
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);

    float temp;
    htu21d_err_t err = htu21d_read_temperature(&dev, &temp);
    ASSERT_EQ(err, HTU21D_OK);
    ASSERT_NEAR(temp, 0.0f, 0.5f);
}

TEST(test_read_temp_hot)
{
    setup();
    htu21d_model_set_temperature(&model, 80.0f);
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);

    float temp;
    htu21d_read_temperature(&dev, &temp);
    ASSERT_NEAR(temp, 80.0f, 1.0f);
}

TEST(test_read_temp_cold)
{
    setup();
    htu21d_model_set_temperature(&model, -20.0f);
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);

    float temp;
    htu21d_read_temperature(&dev, &temp);
    ASSERT_NEAR(temp, -20.0f, 1.0f);
}

/* --- humidity read tests ----------------------------------------------- */

TEST(test_read_hum_50pct)
{
    setup();
    htu21d_model_set_humidity(&model, 50.0f);
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);

    float rh;
    htu21d_err_t err = htu21d_read_humidity(&dev, &rh);
    ASSERT_EQ(err, HTU21D_OK);
    ASSERT_NEAR(rh, 50.0f, 0.5f);
}

TEST(test_read_hum_dry)
{
    setup();
    htu21d_model_set_humidity(&model, 10.0f);
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);

    float rh;
    htu21d_read_humidity(&dev, &rh);
    ASSERT_NEAR(rh, 10.0f, 0.5f);
}

TEST(test_read_hum_saturated)
{
    setup();
    htu21d_model_set_humidity(&model, 95.0f);
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);

    float rh;
    htu21d_read_humidity(&dev, &rh);
    ASSERT_NEAR(rh, 95.0f, 0.5f);
}

/* --- CRC model vs driver ---------------------------------------------- */

TEST(test_crc_matches_driver)
{
    uint8_t data1[2] = { 0xBE, 0xEF };
    uint8_t model_crc = htu21d_model_crc8(data1, 2);
    uint8_t driver_crc = htu21d_crc8(data1, 2);
    ASSERT_EQ(model_crc, driver_crc);
}

TEST(test_crc_various_values)
{
    uint8_t data2[2] = { 0x00, 0x00 };
    ASSERT_EQ(htu21d_model_crc8(data2, 2), htu21d_crc8(data2, 2));

    uint8_t data3[2] = { 0xFF, 0xFF };
    ASSERT_EQ(htu21d_model_crc8(data3, 2), htu21d_crc8(data3, 2));
}

/* --- reset tests ------------------------------------------------------- */

TEST(test_reset)
{
    setup();
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);

    htu21d_err_t err = htu21d_reset(&dev);
    ASSERT_EQ(err, HTU21D_OK);
    ASSERT_EQ(model.reset_count, 2); /* 1 from init + 1 explicit */
}

/* --- command tracking -------------------------------------------------- */

TEST(test_temp_triggers_command)
{
    setup();
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);

    float temp;
    htu21d_read_temperature(&dev, &temp);
    ASSERT_EQ(model.temp_count, 1);

    htu21d_read_temperature(&dev, &temp);
    ASSERT_EQ(model.temp_count, 2);
}

TEST(test_hum_triggers_command)
{
    setup();
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);

    float rh;
    htu21d_read_humidity(&dev, &rh);
    ASSERT_EQ(model.hum_count, 1);

    htu21d_read_humidity(&dev, &rh);
    ASSERT_EQ(model.hum_count, 2);
}

/* --- round-trip: varying environment ----------------------------------- */

TEST(test_temperature_varies)
{
    setup();
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);
    float t1, t2;

    htu21d_model_set_temperature(&model, 10.0f);
    htu21d_read_temperature(&dev, &t1);

    htu21d_model_set_temperature(&model, 40.0f);
    htu21d_read_temperature(&dev, &t2);

    ASSERT_TRUE(t2 > t1);
}

TEST(test_humidity_varies)
{
    setup();
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);
    float h1, h2;

    htu21d_model_set_humidity(&model, 20.0f);
    htu21d_read_humidity(&dev, &h1);

    htu21d_model_set_humidity(&model, 80.0f);
    htu21d_read_humidity(&dev, &h2);

    ASSERT_TRUE(h2 > h1);
}

TEST(test_multiple_reads_consistent)
{
    setup();
    htu21d_model_set_temperature(&model, 25.0f);
    htu21d_model_set_humidity(&model, 50.0f);
    htu21d_t dev;
    htu21d_init(&dev, &model_i2c);

    float t1, t2, h1, h2;
    htu21d_read_temperature(&dev, &t1);
    htu21d_read_temperature(&dev, &t2);
    ASSERT_NEAR(t1, t2, 0.01f);

    htu21d_read_humidity(&dev, &h1);
    htu21d_read_humidity(&dev, &h2);
    ASSERT_NEAR(h1, h2, 0.01f);
}

TEST_MAIN("htu21d hardware model")
    RUN(test_init_ok);
    RUN(test_init_null_param);
    RUN(test_read_temp_25c);
    RUN(test_read_temp_0c);
    RUN(test_read_temp_hot);
    RUN(test_read_temp_cold);
    RUN(test_read_hum_50pct);
    RUN(test_read_hum_dry);
    RUN(test_read_hum_saturated);
    RUN(test_crc_matches_driver);
    RUN(test_crc_various_values);
    RUN(test_reset);
    RUN(test_temp_triggers_command);
    RUN(test_hum_triggers_command);
    RUN(test_temperature_varies);
    RUN(test_humidity_varies);
    RUN(test_multiple_reads_consistent);
TEST_MAIN_END()
