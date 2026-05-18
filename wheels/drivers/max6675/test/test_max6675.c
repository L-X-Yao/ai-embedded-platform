// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_max6675.h"
#include "max6675.h"
#include <string.h>
#include <math.h>

static const max6675_spi_t mock_spi = {
    .cs_low  = hw_model_cs_low,
    .cs_high = hw_model_cs_high,
    .xfer    = hw_model_xfer,
    .ctx     = NULL,
};

static max6675_t dev;

/* ---------- init ---------- */

TEST(test_init_ok)
{
    hw_model_reset();
    ASSERT_EQ(max6675_init(&dev, &mock_spi), MAX6675_OK);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(max6675_init(NULL, &mock_spi), MAX6675_ERR_PARAM);
}

TEST(test_init_null_spi)
{
    ASSERT_EQ(max6675_init(&dev, NULL), MAX6675_ERR_PARAM);
}

TEST(test_init_zero_hal)
{
    max6675_spi_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ASSERT_TRUE(max6675_init(&dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ((int)MAX6675_OK, 0);
    ASSERT_EQ((int)MAX6675_ERR_PARAM, -1);
    ASSERT_EQ((int)MAX6675_ERR_SPI, -2);
    ASSERT_EQ((int)MAX6675_ERR_OPEN, -3);
}

/* ---------- read temperature ---------- */

TEST(test_read_0c)
{
    hw_model_reset();
    max6675_init(&dev, &mock_spi);
    hw_model_inject_temperature(0.0f);
    float temp = -1.0f;
    ASSERT_EQ(max6675_read(&dev, &temp), MAX6675_OK);
    ASSERT_NEAR(temp, 0.0f, 0.3f);
}

TEST(test_read_100c)
{
    hw_model_reset();
    max6675_init(&dev, &mock_spi);
    hw_model_inject_temperature(100.0f);
    float temp = 0.0f;
    ASSERT_EQ(max6675_read(&dev, &temp), MAX6675_OK);
    ASSERT_NEAR(temp, 100.0f, 0.3f);
}

TEST(test_read_500c)
{
    hw_model_reset();
    max6675_init(&dev, &mock_spi);
    hw_model_inject_temperature(500.0f);
    float temp = 0.0f;
    ASSERT_EQ(max6675_read(&dev, &temp), MAX6675_OK);
    ASSERT_NEAR(temp, 500.0f, 0.3f);
}

TEST(test_read_max_1023_75)
{
    hw_model_reset();
    max6675_init(&dev, &mock_spi);
    hw_model_inject_temperature(1023.75f);
    float temp = 0.0f;
    ASSERT_EQ(max6675_read(&dev, &temp), MAX6675_OK);
    ASSERT_NEAR(temp, 1023.75f, 0.3f);
}

TEST(test_read_resolution)
{
    hw_model_reset();
    max6675_init(&dev, &mock_spi);
    hw_frame = (1u << 3);
    float temp = 0.0f;
    ASSERT_EQ(max6675_read(&dev, &temp), MAX6675_OK);
    ASSERT_NEAR(temp, 0.25f, 1e-6f);
}

/* ---------- open thermocouple ---------- */

TEST(test_open_thermocouple)
{
    hw_model_reset();
    max6675_init(&dev, &mock_spi);
    hw_model_inject_temperature(25.0f);
    hw_model_set_open(true);
    float temp = 0.0f;
    ASSERT_EQ(max6675_read(&dev, &temp), MAX6675_ERR_OPEN);
}

/* ---------- read_raw ---------- */

TEST(test_read_raw)
{
    hw_model_reset();
    max6675_init(&dev, &mock_spi);
    hw_frame = 0x1234;
    uint16_t raw = 0;
    ASSERT_EQ(max6675_read_raw(&dev, &raw), MAX6675_OK);
    ASSERT_EQ(raw, 0x1234);
}

/* ---------- null safety ---------- */

TEST(test_read_null_dev)
{
    ASSERT_EQ(max6675_read(NULL, &(float){0}), MAX6675_ERR_PARAM);
}

TEST(test_read_null_out)
{
    hw_model_reset();
    max6675_init(&dev, &mock_spi);
    ASSERT_EQ(max6675_read(&dev, NULL), MAX6675_ERR_PARAM);
}

TEST(test_read_raw_null)
{
    ASSERT_EQ(max6675_read_raw(NULL, &(uint16_t){0}), MAX6675_ERR_PARAM);
    hw_model_reset();
    max6675_init(&dev, &mock_spi);
    ASSERT_EQ(max6675_read_raw(&dev, NULL), MAX6675_ERR_PARAM);
}

TEST_MAIN("max6675")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_spi);
    RUN(test_init_zero_hal);
    RUN(test_error_codes);
    RUN(test_read_0c);
    RUN(test_read_100c);
    RUN(test_read_500c);
    RUN(test_read_max_1023_75);
    RUN(test_read_resolution);
    RUN(test_open_thermocouple);
    RUN(test_read_raw);
    RUN(test_read_null_dev);
    RUN(test_read_null_out);
    RUN(test_read_raw_null);
TEST_MAIN_END()
