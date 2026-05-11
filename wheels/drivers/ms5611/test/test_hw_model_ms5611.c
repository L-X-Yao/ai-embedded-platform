// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_ms5611.h"
#include "../ms5611.h"

static ms5611_model_t model;

static const ms5611_i2c_t model_i2c = {
    .write_reg = ms5611_model_write,
    .read_regs = ms5611_model_read,
    .delay_ms  = ms5611_model_delay,
    .ctx       = &model,
};

static const ms5611_cfg_t default_cfg = {
    .addr = MS5611_DEV_ADDR,
    .osr  = MS5611_OSR_4096,
};

static void setup(void)
{
    ms5611_model_init(&model);
}

TEST(test_init_sends_reset)
{
    setup();
    ms5611_t dev;
    ms5611_init(&dev, &model_i2c, &default_cfg);
    ASSERT_TRUE(model.reset_count > 0);
}

TEST(test_init_reads_prom)
{
    setup();
    ms5611_t dev;
    ms5611_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(dev.prom[1], 40127);
    ASSERT_EQ(dev.prom[5], 33464);
}

TEST(test_read_temperature_reasonable)
{
    setup();
    ms5611_t dev;
    ms5611_init(&dev, &model_i2c, &default_cfg);

    ms5611_data_t data;
    ms5611_err_t err = ms5611_read(&dev, &data);
    ASSERT_EQ(err, MS5611_OK);
    ASSERT_TRUE(data.temperature_c > 15.0f && data.temperature_c < 25.0f);
}

TEST(test_read_pressure_reasonable)
{
    setup();
    ms5611_t dev;
    ms5611_init(&dev, &model_i2c, &default_cfg);

    ms5611_data_t data;
    ms5611_read(&dev, &data);
    ASSERT_TRUE(data.pressure_pa > 90000.0f && data.pressure_pa < 110000.0f);
}

TEST(test_conversion_triggered)
{
    setup();
    ms5611_t dev;
    ms5611_init(&dev, &model_i2c, &default_cfg);

    int before = model.convert_count;
    ms5611_data_t data;
    ms5611_read(&dev, &data);
    /* Should trigger D1 + D2 conversions */
    ASSERT_EQ(model.convert_count, before + 2);
}

TEST(test_different_temperature)
{
    setup();
    ms5611_t dev;
    ms5611_init(&dev, &model_i2c, &default_cfg);

    ms5611_data_t d1;
    ms5611_read(&dev, &d1);

    /* Increase D2 for higher temperature */
    model.d2_raw = 9000000;
    ms5611_data_t d2;
    ms5611_read(&dev, &d2);

    ASSERT_TRUE(d2.temperature_c > d1.temperature_c);
}

TEST(test_multiple_reads)
{
    setup();
    ms5611_t dev;
    ms5611_init(&dev, &model_i2c, &default_cfg);

    ms5611_data_t d1, d2;
    ms5611_read(&dev, &d1);
    ms5611_read(&dev, &d2);
    ASSERT_NEAR(d1.temperature_c, d2.temperature_c, 0.01f);
    ASSERT_NEAR(d1.pressure_pa, d2.pressure_pa, 1.0f);
}

TEST_MAIN("ms5611 hardware model")
    RUN(test_init_sends_reset);
    RUN(test_init_reads_prom);
    RUN(test_read_temperature_reasonable);
    RUN(test_read_pressure_reasonable);
    RUN(test_conversion_triggered);
    RUN(test_different_temperature);
    RUN(test_multiple_reads);
TEST_MAIN_END()
