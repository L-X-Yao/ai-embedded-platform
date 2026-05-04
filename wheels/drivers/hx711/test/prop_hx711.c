// SPDX-License-Identifier: MIT
/*
 * prop_hx711.c — property-based tests for HX711 load cell ADC
 */

#include "test_harness.h"
#include "property_test.h"
#include "hx711.h"

/* --- GPIO mock --- */

static bool mock_dout;
static int  mock_pulse_count;

static bool mock_read_dout(void *ctx) { (void)ctx; return mock_dout; }
static void mock_set_sck(void *ctx, bool level)
{
    (void)ctx;
    if (level) {
        mock_pulse_count++;
        /* Toggle DOUT on each SCK pulse to simulate data */
        if (mock_pulse_count % 2 == 0) mock_dout = !mock_dout;
    }
}
static void mock_delay_us(void *ctx, uint32_t us) { (void)ctx; (void)us; }

static const hx711_hal_t prop_hal = {
    .read_dout = mock_read_dout,
    .set_sck   = mock_set_sck,
    .delay_us  = mock_delay_us,
    .ctx       = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_dout = false; /* data ready */
    mock_pulse_count = 0;
    hx711_t dev;
    hx711_gain_t gain = (hx711_gain_t)(1 + ARBI_UINT8(seed) % 3);
    hx711_init(&dev, &prop_hal, gain);
}

PROPERTY(read_no_crash, 5000)
{
    mock_dout = false;
    mock_pulse_count = 0;
    hx711_t dev;
    hx711_init(&dev, &prop_hal, HX711_GAIN_128);
    hx711_set_scale(&dev, 1.0f);

    int32_t raw;
    hx711_read_raw(&dev, &raw);

    float value;
    mock_dout = false;
    mock_pulse_count = 0;
    hx711_read(&dev, &value);
}

PROPERTY(set_scale_offset_no_crash, 5000)
{
    mock_dout = false;
    mock_pulse_count = 0;
    hx711_t dev;
    hx711_init(&dev, &prop_hal, HX711_GAIN_128);

    float scale = (float)(int8_t)ARBI_UINT8(seed);
    if (scale == 0.0f) scale = 1.0f;
    hx711_set_scale(&dev, scale);
    hx711_set_offset(&dev, (int32_t)ARBI_UINT32(seed));
}

TEST_MAIN("prop_hx711")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(set_scale_offset_no_crash);
TEST_MAIN_END()
