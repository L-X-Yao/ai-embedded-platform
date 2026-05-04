// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_ds18b20.h"

/* ---------- tests ---------- */

TEST(test_read_25c)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, 25.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    ds18b20_start_conversion(&dev);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 25.0f, 0.1f);
}

TEST(test_read_negative_10c)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, -10.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    ds18b20_start_conversion(&dev);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, -10.0f, 0.1f);
}

TEST(test_read_85c_power_on_default)
{
    /* DS18B20 powers on with 85.0 C in scratchpad */
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    /* model defaults to 85.0 C, don't call set_temperature */
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 85.0f, 0.1f);
}

TEST(test_crc_matches_scratchpad)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, 25.0f);

    /* verify model-generated CRC matches recomputed CRC */
    uint8_t stored_crc = ds18b20_model_get_scratchpad_crc(&model);
    uint8_t computed_crc = ds18b20_model_verify_crc(&model);
    ASSERT_EQ(stored_crc, computed_crc);

    /* test with a different temperature too */
    ds18b20_model_set_temperature(&model, -40.0f);
    stored_crc = ds18b20_model_get_scratchpad_crc(&model);
    computed_crc = ds18b20_model_verify_crc(&model);
    ASSERT_EQ(stored_crc, computed_crc);
}

TEST(test_reset_presence_pulse)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_present(&model, 1);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    /* start_conversion does a reset + skip_rom + convert_t */
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_OK);
    ASSERT_EQ(ds18b20_model_get_reset_count(&model), 1);
    ASSERT_EQ(ds18b20_model_get_convert_count(&model), 1);
}

TEST(test_no_device_on_bus)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_present(&model, 0);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_ERR_NO_DEVICE);
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_ERR_NO_DEVICE);
}

TEST(test_read_zero_celsius)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, 0.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 0.0f, 0.1f);
}

TEST(test_read_max_125c)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, 125.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    ds18b20_start_conversion(&dev);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 125.0f, 0.1f);
}

TEST_MAIN("ds18b20_hw_model")
    RUN(test_read_25c);
    RUN(test_read_negative_10c);
    RUN(test_read_85c_power_on_default);
    RUN(test_crc_matches_scratchpad);
    RUN(test_reset_presence_pulse);
    RUN(test_no_device_on_bus);
    RUN(test_read_zero_celsius);
    RUN(test_read_max_125c);
TEST_MAIN_END()
