// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_max6675.h"
#include "max6675.h"

static const max6675_spi_t hw_spi = {
    .cs_low  = hw_model_cs_low,
    .cs_high = hw_model_cs_high,
    .xfer    = hw_model_xfer,
    .ctx     = NULL,
};

static max6675_t dev;

TEST(test_hw_init)
{
    hw_model_reset();
    ASSERT_EQ(max6675_init(&dev, &hw_spi), MAX6675_OK);
}

TEST(test_hw_roundtrip_25c)
{
    hw_model_reset();
    max6675_init(&dev, &hw_spi);
    hw_model_inject_temperature(25.0f);
    float temp = 0.0f;
    max6675_read(&dev, &temp);
    ASSERT_NEAR(temp, 25.0f, 0.3f);
}

TEST(test_hw_roundtrip_1000c)
{
    hw_model_reset();
    max6675_init(&dev, &hw_spi);
    hw_model_inject_temperature(1000.0f);
    float temp = 0.0f;
    max6675_read(&dev, &temp);
    ASSERT_NEAR(temp, 1000.0f, 0.3f);
}

TEST(test_hw_open_detection)
{
    hw_model_reset();
    max6675_init(&dev, &hw_spi);
    hw_model_inject_temperature(100.0f);
    hw_model_set_open(true);
    float temp = 0.0f;
    ASSERT_EQ(max6675_read(&dev, &temp), MAX6675_ERR_OPEN);
}

TEST_MAIN("max6675_hw_model")
    RUN(test_hw_init);
    RUN(test_hw_roundtrip_25c);
    RUN(test_hw_roundtrip_1000c);
    RUN(test_hw_open_detection);
TEST_MAIN_END()
