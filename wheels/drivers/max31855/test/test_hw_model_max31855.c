// SPDX-License-Identifier: MIT
/*
 * test_hw_model_max31855.c -- tests using MAX31855 hardware model
 *
 * Validates MAX31855 driver against a datasheet-accurate model.
 * Tests thermocouple/internal temperature readout, fault detection
 * (open circuit, short to GND/VCC), and negative temperature encoding.
 */

#include "test_harness.h"
#include "hw_model_max31855.h"
#include "../max31855.h"
#include <math.h>

static max31855_model_t model;
static max31855_spi_ctx_t spi_ctx;

static const max31855_spi_t model_spi = {
    .cs_low  = max31855_model_cs_low,
    .cs_high = max31855_model_cs_high,
    .xfer    = max31855_model_xfer,
    .ctx     = &spi_ctx,
};

static void setup(void)
{
    max31855_model_init(&model);
    spi_ctx.model = &model;
    spi_ctx.byte_idx = 0;
    spi_ctx.selected = false;
}

/* --- init tests --- */

TEST(test_init_success)
{
    setup();
    max31855_t dev;
    max31855_err_t err = max31855_init(&dev, &model_spi);
    ASSERT_EQ(err, MAX31855_OK);
}

/* --- read temperature tests --- */

TEST(test_read_room_temperature)
{
    setup();
    max31855_model_set_temps(&model, 25.0f, 23.0f);

    max31855_t dev;
    max31855_init(&dev, &model_spi);

    float tc = 0.0f, internal = 0.0f;
    max31855_err_t err = max31855_read(&dev, &tc, &internal);
    ASSERT_EQ(err, MAX31855_OK);
    ASSERT_NEAR(tc, 25.0f, 0.5f);
    ASSERT_NEAR(internal, 23.0f, 0.1f);
}

TEST(test_read_high_temperature)
{
    setup();
    max31855_model_set_temps(&model, 200.0f, 30.0f);

    max31855_t dev;
    max31855_init(&dev, &model_spi);

    float tc = 0.0f, internal = 0.0f;
    max31855_read(&dev, &tc, &internal);
    ASSERT_NEAR(tc, 200.0f, 0.5f);
    ASSERT_NEAR(internal, 30.0f, 0.1f);
}

TEST(test_read_negative_temperature)
{
    setup();
    max31855_model_set_temps(&model, -40.0f, 10.0f);

    max31855_t dev;
    max31855_init(&dev, &model_spi);

    float tc = 0.0f, internal = 0.0f;
    max31855_read(&dev, &tc, &internal);
    ASSERT_NEAR(tc, -40.0f, 0.5f);
}

TEST(test_read_zero_temperature)
{
    setup();
    max31855_model_set_temps(&model, 0.0f, 0.0f);

    max31855_t dev;
    max31855_init(&dev, &model_spi);

    float tc = 0.0f, internal = 0.0f;
    max31855_read(&dev, &tc, &internal);
    ASSERT_NEAR(tc, 0.0f, 0.5f);
    ASSERT_NEAR(internal, 0.0f, 0.1f);
}

/* --- fault tests --- */

TEST(test_fault_open_circuit)
{
    setup();
    max31855_model_set_fault(&model, true, false, false);

    max31855_t dev;
    max31855_init(&dev, &model_spi);

    float tc = 0.0f;
    max31855_err_t err = max31855_read(&dev, &tc, NULL);
    ASSERT_EQ(err, MAX31855_ERR_OPEN);
}

TEST(test_fault_short_gnd)
{
    setup();
    max31855_model_set_fault(&model, false, true, false);

    max31855_t dev;
    max31855_init(&dev, &model_spi);

    float tc = 0.0f;
    max31855_err_t err = max31855_read(&dev, &tc, NULL);
    ASSERT_EQ(err, MAX31855_ERR_SHORT_GND);
}

TEST(test_fault_short_vcc)
{
    setup();
    max31855_model_set_fault(&model, false, false, true);

    max31855_t dev;
    max31855_init(&dev, &model_spi);

    float tc = 0.0f;
    max31855_err_t err = max31855_read(&dev, &tc, NULL);
    ASSERT_EQ(err, MAX31855_ERR_SHORT_VCC);
}

/* --- raw read test --- */

TEST(test_read_raw)
{
    setup();
    max31855_model_set_temps(&model, 25.0f, 23.0f);

    max31855_t dev;
    max31855_init(&dev, &model_spi);

    uint32_t raw = 0;
    max31855_err_t err = max31855_read_raw(&dev, &raw);
    ASSERT_EQ(err, MAX31855_OK);
    /* Raw should be nonzero for non-zero temps */
    ASSERT_TRUE(raw != 0);
    /* No fault flag */
    ASSERT_FALSE(raw & (1u << 16));
}

/* --- multiple reads --- */

TEST(test_multiple_reads_consistent)
{
    setup();
    max31855_model_set_temps(&model, 100.0f, 25.0f);

    max31855_t dev;
    max31855_init(&dev, &model_spi);

    float tc1 = 0.0f, tc2 = 0.0f;
    max31855_read(&dev, &tc1, NULL);
    max31855_read(&dev, &tc2, NULL);
    ASSERT_NEAR(tc1, tc2, 0.01f);
    ASSERT_EQ(model.read_count, 2);
}

/* --- the frame, from 19-5793 Rev 5 --- */

static uint32_t read_frame(void)
{
    max31855_t dev;
    uint32_t raw = 0;

    (void)max31855_init(&dev, &model_spi);
    (void)max31855_read_raw(&dev, &raw);
    return raw;
}

/* p.9: "If T+ and T- are unconnected, the thermocouple temperature sign bit
 * (D31) is 0, and the remainder of the thermocouple temperature value
 * (D[30:18]) is 1." Table 3: D17 and D3 always read 0, D16 is the fault bit,
 * D0 the open circuit. The internal temperature is still measured. */
TEST(test_open_circuit_frame)
{
    setup();
    max31855_model_set_temps(&model, 25.0f, 23.0f);
    max31855_model_set_fault(&model, true, false, false);

    uint32_t raw = read_frame();
    ASSERT_EQ_HEX(raw >> 18, 0x1FFF);
    ASSERT_EQ_HEX(raw & 0x30000u, 0x10000);
    ASSERT_EQ_HEX(raw & 0x0000Fu, 0x1);
    ASSERT_EQ_HEX((raw >> 4) & 0xFFFu, 0x170);  /* 23 C at 0.0625 C/LSB */
}

/* The open-circuit value must not reach the caller as a temperature */
TEST(test_open_circuit_writes_no_temperature)
{
    setup();
    max31855_model_set_fault(&model, true, false, false);

    max31855_t dev;
    max31855_init(&dev, &model_spi);

    float tc = -1.0f, internal = -1.0f;
    ASSERT_EQ(max31855_read(&dev, &tc, &internal), MAX31855_ERR_OPEN);
    ASSERT_NEAR(tc, -1.0f, 0.001f);
    ASSERT_NEAR(internal, -1.0f, 0.001f);
}

TEST(test_temperature_returns_when_the_circuit_closes)
{
    setup();
    max31855_model_set_temps(&model, 25.0f, 23.0f);
    max31855_model_set_fault(&model, true, false, false);
    max31855_model_set_fault(&model, false, false, false);

    uint32_t raw = read_frame();
    ASSERT_EQ_HEX(raw >> 18, 0x64);             /* 25 C at 0.25 C/LSB */
    ASSERT_EQ_HEX(raw & 0x1000Fu, 0x0);
}

/* p.9: "The fault and temperature data are only be updated when CS is high." */
TEST(test_frame_holds_while_cs_is_low)
{
    setup();
    max31855_model_set_temps(&model, 25.0f, 23.0f);
    uint32_t before = model.frame;

    max31855_model_cs_low(&spi_ctx);
    uint8_t d31_24 = max31855_model_xfer(&spi_ctx, 0);
    uint8_t d23_16 = max31855_model_xfer(&spi_ctx, 0);
    max31855_model_set_temps(&model, 100.0f, 50.0f);
    uint8_t d15_8 = max31855_model_xfer(&spi_ctx, 0);
    uint8_t d7_0  = max31855_model_xfer(&spi_ctx, 0);
    max31855_model_cs_high(&spi_ctx);

    ASSERT_EQ_HEX(((uint32_t)d31_24 << 24) | ((uint32_t)d23_16 << 16) |
                  ((uint32_t)d15_8 << 8) | d7_0, before);
    ASSERT_EQ_HEX(read_frame(), model.frame);
}

/* p.9: "CS can be taken high at any point while clocking out conversion
 * data", and "Drive CS low to output the first bit on the SO pin." */
TEST(test_cs_restarts_the_frame)
{
    setup();
    max31855_model_set_temps(&model, 25.0f, 23.0f);

    max31855_model_cs_low(&spi_ctx);
    uint8_t first = max31855_model_xfer(&spi_ctx, 0);
    (void)max31855_model_xfer(&spi_ctx, 0);
    max31855_model_cs_high(&spi_ctx);

    max31855_model_cs_low(&spi_ctx);
    ASSERT_EQ_HEX(max31855_model_xfer(&spi_ctx, 0), first);
    max31855_model_cs_high(&spi_ctx);
    ASSERT_EQ(model.read_count, 0);
}

/* The electrical table has "CS Rise to Output Disable": with CS high nothing
 * drives SO, so the model reads the bus as idle high. */
TEST(test_so_is_not_driven_with_cs_high)
{
    setup();
    max31855_model_set_temps(&model, 25.0f, 23.0f);
    for (int i = 0; i < 6; i++)
        ASSERT_EQ_HEX(max31855_model_xfer(&spi_ctx, 0), 0xFF);
}

/* Clocks past D0 must not run off the frame */
TEST(test_clocks_past_d0_read_0)
{
    setup();
    max31855_model_cs_low(&spi_ctx);
    for (int i = 0; i < 4; i++)
        (void)max31855_model_xfer(&spi_ctx, 0);
    ASSERT_EQ_HEX(max31855_model_xfer(&spi_ctx, 0), 0x00);
    ASSERT_EQ_HEX(max31855_model_xfer(&spi_ctx, 0), 0x00);
    max31855_model_cs_high(&spi_ctx);
}

TEST_MAIN("max31855 hardware model")
    RUN(test_init_success);
    RUN(test_read_room_temperature);
    RUN(test_read_high_temperature);
    RUN(test_read_negative_temperature);
    RUN(test_read_zero_temperature);
    RUN(test_fault_open_circuit);
    RUN(test_fault_short_gnd);
    RUN(test_fault_short_vcc);
    RUN(test_read_raw);
    RUN(test_multiple_reads_consistent);
    RUN(test_open_circuit_frame);
    RUN(test_open_circuit_writes_no_temperature);
    RUN(test_temperature_returns_when_the_circuit_closes);
    RUN(test_frame_holds_while_cs_is_low);
    RUN(test_cs_restarts_the_frame);
    RUN(test_so_is_not_driven_with_cs_high);
    RUN(test_clocks_past_d0_read_0);
TEST_MAIN_END()
