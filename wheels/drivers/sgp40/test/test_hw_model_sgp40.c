// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_sgp40.h"
#include "../sgp40.h"

static sgp40_model_t model;

static const sgp40_i2c_t model_i2c = {
    .write_reg = sgp40_model_write,
    .read_regs = sgp40_model_read,
    .delay_ms  = sgp40_model_delay,
    .ctx       = &model,
};

static void setup(void)
{
    sgp40_model_init(&model);
}

TEST(test_init_runs_self_test)
{
    setup();
    sgp40_t dev;
    sgp40_err_t err = sgp40_init(&dev, &model_i2c);
    ASSERT_EQ(err, SGP40_OK);
    ASSERT_EQ(model.self_test_count, 1);
}

TEST(test_init_self_test_fail)
{
    setup();
    model.self_test_pass = false;
    sgp40_t dev;
    sgp40_err_t err = sgp40_init(&dev, &model_i2c);
    ASSERT_EQ(err, SGP40_ERR_SELF_TEST);
}

TEST(test_measure_returns_sraw)
{
    setup();
    sgp40_t dev;
    sgp40_init(&dev, &model_i2c);

    sgp40_data_t data;
    sgp40_err_t err = sgp40_measure_raw(&dev, 0x8000, 0x6666, &data);
    ASSERT_EQ(err, SGP40_OK);
    ASSERT_EQ(data.sraw, 26000);
}

TEST(test_measure_different_sraw)
{
    setup();
    sgp40_t dev;
    sgp40_init(&dev, &model_i2c);

    model.sraw_value = 50000;
    sgp40_data_t data;
    sgp40_measure_raw(&dev, 0x8000, 0x6666, &data);
    ASSERT_EQ(data.sraw, 50000);
}

TEST(test_measure_count)
{
    setup();
    sgp40_t dev;
    sgp40_init(&dev, &model_i2c);

    int before = model.measure_count;
    sgp40_data_t data;
    sgp40_measure_raw(&dev, 0x8000, 0x6666, &data);
    ASSERT_EQ(model.measure_count, before + 1);
}

TEST(test_crc_verified)
{
    setup();
    sgp40_t dev;
    sgp40_init(&dev, &model_i2c);

    /* Model always generates correct CRC, so reads succeed */
    sgp40_data_t data;
    sgp40_err_t err = sgp40_measure_raw(&dev, 0x8000, 0x6666, &data);
    ASSERT_EQ(err, SGP40_OK);
}

TEST(test_multiple_measures)
{
    setup();
    sgp40_t dev;
    sgp40_init(&dev, &model_i2c);

    sgp40_data_t d1, d2;
    sgp40_measure_raw(&dev, 0x8000, 0x6666, &d1);
    sgp40_measure_raw(&dev, 0x8000, 0x6666, &d2);
    ASSERT_EQ(d1.sraw, d2.sraw);
}

TEST_MAIN("sgp40 hardware model")
    RUN(test_init_runs_self_test);
    RUN(test_init_self_test_fail);
    RUN(test_measure_returns_sraw);
    RUN(test_measure_different_sraw);
    RUN(test_measure_count);
    RUN(test_crc_verified);
    RUN(test_multiple_measures);
TEST_MAIN_END()
