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

/* p.8 example */
#define D1_EXAMPLE 9085466u
#define D2_EXAMPLE 8569150u

static void send(uint8_t cmd)
{
    ASSERT_TRUE(ms5611_model_write_raw(&model, MS5611_ADDR_CSB_LOW, &cmd, 1));
}

static uint32_t adc_read(void)
{
    uint8_t b[3] = { 0xFF, 0xFF, 0xFF };
    ms5611_model_read(&model, MS5611_ADDR_CSB_LOW, 0x00, b, 3);
    return ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];
}

static uint16_t prom_read(uint8_t cmd)
{
    uint8_t b[2] = { 0xFF, 0xFF };
    ms5611_model_read(&model, MS5611_ADDR_CSB_LOW, cmd, b, 2);
    return (uint16_t)((uint16_t)b[0] << 8 | b[1]);
}

/* p.11 "If the conversion is not executed before the ADC read command ... it
 * will give 0 as the output result." */
TEST(test_adc_read_without_a_conversion_gives_0)
{
    ms5611_model_init(&model);
    ASSERT_EQ(adc_read(), 0u);
}

/* p.11 "or the ADC read command is repeated, it will give 0" */
TEST(test_repeated_adc_read_gives_0)
{
    ms5611_model_init(&model);
    send(0x48);
    ms5611_model_delay(&model, 10);
    ASSERT_EQ(adc_read(), D1_EXAMPLE);
    ASSERT_EQ(adc_read(), 0u);
}

/* p.3 conversion time, maximum: 0.60, 1.17, 2.28, 4.54 and 9.04 ms. p.11 "If
 * the ADC read command is sent during conversion the result will be 0". */
TEST(test_adc_read_needs_the_conversion_time)
{
    static const struct { uint8_t d1; uint8_t d2; uint32_t early_ms; } rows[] = {
        { 0x40, 0x50, 0 }, { 0x42, 0x52, 1 }, { 0x44, 0x54, 2 },
        { 0x46, 0x56, 4 }, { 0x48, 0x58, 9 },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        ms5611_model_init(&model);
        send(rows[i].d1);
        ms5611_model_delay(&model, rows[i].early_ms);
        ASSERT_EQ(adc_read(), 0u);

        ms5611_model_init(&model);
        send(rows[i].d1);
        ms5611_model_delay(&model, rows[i].early_ms + 1u);
        ASSERT_EQ(adc_read(), D1_EXAMPLE);

        ms5611_model_init(&model);
        send(rows[i].d2);
        ms5611_model_delay(&model, rows[i].early_ms + 1u);
        ASSERT_EQ(adc_read(), D2_EXAMPLE);
    }
}

/* p.11 "the conversion will not stop and the final result will be wrong" */
TEST(test_adc_read_during_a_conversion_spoils_it)
{
    ms5611_model_init(&model);
    send(0x48);
    ms5611_model_delay(&model, 1);
    ASSERT_EQ(adc_read(), 0u);
    ms5611_model_delay(&model, 10);
    ASSERT_TRUE(adc_read() != D1_EXAMPLE);
}

/* p.11 "Conversion sequence sent during the already started conversion
 * process will yield incorrect result as well." */
TEST(test_convert_during_a_conversion_spoils_it)
{
    ms5611_model_init(&model);
    send(0x48);
    send(0x58);
    ms5611_model_delay(&model, 20);

    uint32_t adc = adc_read();
    ASSERT_TRUE(adc != D1_EXAMPLE);
    ASSERT_TRUE(adc != D2_EXAMPLE);
}

/* p.10 "The Reset sequence shall be sent once after power-on to make sure that
 * the calibration PROM gets loaded into the internal register", and its figure
 * gives "2.8ms RELOAD". */
TEST(test_prom_is_loaded_by_the_reset)
{
    ms5611_model_init(&model);
    ASSERT_EQ_HEX(prom_read(0xA2), 0x0000);

    send(0x1E);
    ms5611_model_delay(&model, 2);
    ASSERT_EQ_HEX(prom_read(0xA2), 0x0000);

    ms5611_model_delay(&model, 1);
    ASSERT_EQ(prom_read(0xA2), 40127);
}

/* A HAL whose wait comes up short */
static void short_delay(void *ctx, uint32_t ms)
{
    (void)ms;
    ms5611_model_delay(ctx, 0);
}

static const ms5611_i2c_t short_delay_i2c = {
    .read_regs = ms5611_model_read,
    .delay_ms  = short_delay,
    .ctx       = &model,
    .write_raw = ms5611_model_write_raw,
};

/* p.11: 0 is the only way the part reports a missed conversion, so it must not
 * come back as a measurement. */
TEST(test_read_refuses_an_adc_result_of_0)
{
    ms5611_data_t data;
    ms5611_t d;

    ms5611_model_init(&model);
    ASSERT_EQ(ms5611_init(&d, &model_i2c, &cfg), MS5611_OK);

    d.i2c = &short_delay_i2c;
    ASSERT_EQ(ms5611_read(&d, &data), MS5611_ERR_ADC);
}

TEST_MAIN("hw_model_ms5611")
    RUN(test_read_gives_the_manual_example);
    RUN(test_a_byte_after_convert_spoils_the_result);
    RUN(test_a_lone_convert_gives_the_conversion);
    RUN(test_adc_read_without_a_conversion_gives_0);
    RUN(test_repeated_adc_read_gives_0);
    RUN(test_adc_read_needs_the_conversion_time);
    RUN(test_adc_read_during_a_conversion_spoils_it);
    RUN(test_convert_during_a_conversion_spoils_it);
    RUN(test_prom_is_loaded_by_the_reset);
    RUN(test_read_refuses_an_adc_result_of_0);
TEST_MAIN_END()
