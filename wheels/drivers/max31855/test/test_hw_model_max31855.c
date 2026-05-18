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
TEST_MAIN_END()
