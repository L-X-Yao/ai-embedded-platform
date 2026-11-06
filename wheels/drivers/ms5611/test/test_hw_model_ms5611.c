// SPDX-License-Identifier: MIT
/*
 * test_hw_model_ms5611.c — driver against the hardware model
 *
 * Checks what hw_model_ms5611.h takes from MS5611-01BA03 06/2017: commands are
 * one byte, and a stray byte during a conversion spoils the result. The values
 * expected from a read are the worked example on p8, not the driver's.
 */

#include "test_harness.h"
#include "hw_model_ms5611.h"
#include "../ms5611.h"

static ms5611_model_t model;

static const ms5611_i2c_t model_i2c = {
    .write_reg = ms5611_model_write,
    .read_regs = ms5611_model_read,
    .delay_ms  = ms5611_model_delay,
    .ctx       = &model,
    .write_raw = ms5611_model_write_raw,
};

static const ms5611_cfg_t cfg = {
    .addr = MS5611_ADDR_CSB_LOW,
    .osr  = MS5611_OSR_4096,
};

TEST(test_read_gives_the_manual_example)
{
    ms5611_data_t data;
    ms5611_t d;

    ms5611_model_init(&model);
    ASSERT_EQ(ms5611_init(&d, &model_i2c, &cfg), MS5611_OK);
    ASSERT_EQ(model.reset_count, 1);

    /* p8: D1 = 9085466 and D2 = 8569150 give 20.07 C and 1000.09 mbar */
    ASSERT_EQ(ms5611_read(&d, &data), MS5611_OK);
    ASSERT_EQ(model.convert_count, 2);
    ASSERT_NEAR(data.temperature_c, 20.07f, 0.001f);
    ASSERT_NEAR(data.pressure_pa, 100009.0f, 0.5f);
}

TEST(test_a_byte_after_convert_spoils_the_result)
{
    /* p10: 0x00 is ADC Read. p11: sent during a conversion, the result is 0 */
    static const uint8_t with_data[] = { 0x48, 0x00 };
    static const uint8_t zero[3] = { 0, 0, 0 };
    uint8_t adc[3] = { 0xFF, 0xFF, 0xFF };

    ms5611_model_init(&model);
    ASSERT_TRUE(ms5611_model_write_raw(&model, MS5611_ADDR_CSB_LOW,
                                       with_data, sizeof(with_data)));
    ms5611_model_delay(&model, 10);
    ASSERT_TRUE(ms5611_model_read(&model, MS5611_ADDR_CSB_LOW, 0x00, adc, 3));
    ASSERT_MEMEQ_N(adc, zero, 3);
}

TEST(test_a_lone_convert_gives_the_conversion)
{
    static const uint8_t convert[] = { 0x48 };
    uint8_t adc[3] = { 0, 0, 0 };

    ms5611_model_init(&model);
    ASSERT_TRUE(ms5611_model_write_raw(&model, MS5611_ADDR_CSB_LOW,
                                       convert, sizeof(convert)));
    ms5611_model_delay(&model, 10);
    ASSERT_TRUE(ms5611_model_read(&model, MS5611_ADDR_CSB_LOW, 0x00, adc, 3));
    /* p8: D1 = 9085466 = 0x8AA21A */
    ASSERT_EQ_HEX(adc[0], 0x8A);
    ASSERT_EQ_HEX(adc[1], 0xA2);
    ASSERT_EQ_HEX(adc[2], 0x1A);
}

TEST_MAIN("hw_model_ms5611")
    RUN(test_read_gives_the_manual_example);
    RUN(test_a_byte_after_convert_spoils_the_result);
    RUN(test_a_lone_convert_gives_the_conversion);
TEST_MAIN_END()
