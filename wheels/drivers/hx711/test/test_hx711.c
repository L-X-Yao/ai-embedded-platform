// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hx711.h"

/* ---------- mock HX711 serial protocol ---------- */

static bool     mock_dout_ready;      /* true = data ready, DOUT low */
static uint32_t mock_raw_value;       /* 24-bit value to serve */
static int      mock_bit_index;       /* SCK rising edges of this read */
static int      mock_sck_rises;       /* total SCK rising edges */
static int      mock_sck_falls;       /* total SCK falling edges */
static bool     mock_sck_level;       /* current SCK state */
static int      mock_delay_calls;
static uint32_t mock_last_delay_us;
static uint32_t mock_now_us;          /* advanced by delay_us */
static uint32_t mock_conv_us;         /* one conversion period */
static uint32_t mock_done_at_us;      /* when the running conversion ends */
static bool     mock_converting;      /* a read started the next conversion */

#define MOCK_CONV_10SPS_US  100000u
#define MOCK_CONV_NEVER_US  0xF0000000u

static void mock_reset(void)
{
    mock_dout_ready   = true;   /* ready by default */
    mock_raw_value    = 0;
    mock_bit_index    = 0;
    mock_sck_rises    = 0;
    mock_sck_falls    = 0;
    mock_sck_level    = false;
    mock_delay_calls  = 0;
    mock_last_delay_us = 0;
    mock_now_us       = 0;
    mock_conv_us      = MOCK_CONV_10SPS_US;
    mock_done_at_us   = 0;
    mock_converting   = false;
}

static bool mock_read_dout(void *ctx)
{
    (void)ctx;

    /* SCK low: DOUT says whether data is ready */
    if (!mock_sck_level)
        return !mock_dout_ready;

    /* during data clocking (bits 0..23): serve MSB first on SCK high */
    if (mock_bit_index >= 1 && mock_bit_index <= 24) {
        int bit_pos = 23 - (mock_bit_index - 1);
        return (mock_raw_value >> (unsigned)bit_pos) & 1U ? true : false;
    }

    /* the 25th pulse pulled DOUT high */
    return true;
}

static void mock_set_sck(void *ctx, bool level)
{
    (void)ctx;

    if (level && !mock_sck_level) {
        mock_sck_rises++;
        /* the first pulse after a finished read starts a new one */
        if (mock_bit_index >= 25 && mock_dout_ready)
            mock_bit_index = 0;
        mock_bit_index++;
        /* p.4: "The 25th pulse at PD_SCK input will pull DOUT pin back to
         * high". It stays high until the next conversion ends. */
        if (mock_bit_index == 25) {
            mock_dout_ready = false;
            mock_converting = true;
            mock_done_at_us = mock_now_us + mock_conv_us;
        }
    }
    if (!level && mock_sck_level)
        mock_sck_falls++;
    mock_sck_level = level;
}

static void mock_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    mock_delay_calls++;
    mock_last_delay_us = us;
    mock_now_us += us;
    if (mock_converting && mock_now_us >= mock_done_at_us) {
        mock_converting = false;
        mock_dout_ready = true;
    }
}

static const hx711_hal_t mock_hal = {
    .read_dout = mock_read_dout,
    .set_sck   = mock_set_sck,
    .delay_us  = mock_delay_us,
    .ctx       = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    hx711_t dev;
    mock_reset();
    ASSERT_EQ(hx711_init(&dev, &mock_hal, HX711_GAIN_128), HX711_OK);
    ASSERT_EQ(dev.gain, HX711_GAIN_128);
    ASSERT_EQ(dev.offset, 0);
    ASSERT_NEAR(dev.scale, 1.0f, 0.001f);
}

TEST(test_init_null_params)
{
    hx711_t dev;
    mock_reset();
    ASSERT_EQ(hx711_init(NULL, &mock_hal, HX711_GAIN_128), HX711_ERR_PARAM);
    ASSERT_EQ(hx711_init(&dev, NULL, HX711_GAIN_128), HX711_ERR_PARAM);
}

TEST(test_init_invalid_gain)
{
    hx711_t dev;
    mock_reset();
    ASSERT_EQ(hx711_init(&dev, &mock_hal, (hx711_gain_t)0), HX711_ERR_PARAM);
    ASSERT_EQ(hx711_init(&dev, &mock_hal, (hx711_gain_t)4), HX711_ERR_PARAM);
}

TEST(test_is_ready_true)
{
    hx711_t dev;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);
    mock_dout_ready = true;
    ASSERT_TRUE(hx711_is_ready(&dev));
}

TEST(test_is_ready_false)
{
    hx711_t dev;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);
    mock_dout_ready = false;
    ASSERT_FALSE(hx711_is_ready(&dev));
}

TEST(test_read_raw_positive)
{
    hx711_t dev;
    int32_t val = 0;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    /* 0x3A5C00 = 3,825,664 (positive 24-bit) */
    mock_raw_value  = 0x3A5C00;
    mock_dout_ready = true;

    ASSERT_EQ(hx711_read_raw(&dev, &val), HX711_OK);
    ASSERT_EQ(val, 0x3A5C00);
}

TEST(test_read_raw_negative)
{
    hx711_t dev;
    int32_t val = 0;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    /* 0xFFF000 = -4096 after 24-bit sign extension */
    mock_raw_value  = 0xFFF000;
    mock_dout_ready = true;

    ASSERT_EQ(hx711_read_raw(&dev, &val), HX711_OK);
    ASSERT_EQ(val, -4096);
}

TEST(test_read_raw_zero)
{
    hx711_t dev;
    int32_t val = 999;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    mock_raw_value  = 0x000000;
    mock_dout_ready = true;

    ASSERT_EQ(hx711_read_raw(&dev, &val), HX711_OK);
    ASSERT_EQ(val, 0);
}

TEST(test_read_raw_not_ready)
{
    hx711_t dev;
    int32_t val = 0;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    mock_dout_ready = false;
    ASSERT_EQ(hx711_read_raw(&dev, &val), HX711_ERR_NOT_READY);
}

TEST(test_gain_128_pulse_count)
{
    hx711_t dev;
    int32_t val;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    mock_raw_value  = 0x000001;
    mock_dout_ready = true;

    hx711_read_raw(&dev, &val);
    /* 24 data + 1 gain = 25 pulses */
    ASSERT_EQ(mock_sck_rises, 25);
}

TEST(test_gain_32_pulse_count)
{
    hx711_t dev;
    int32_t val;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_32);

    mock_raw_value  = 0x000001;
    mock_dout_ready = true;

    hx711_read_raw(&dev, &val);
    /* 24 data + 2 gain = 26 pulses */
    ASSERT_EQ(mock_sck_rises, 26);
}

TEST(test_gain_64_pulse_count)
{
    hx711_t dev;
    int32_t val;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_64);

    mock_raw_value  = 0x000001;
    mock_dout_ready = true;

    hx711_read_raw(&dev, &val);
    /* 24 data + 3 gain = 27 pulses */
    ASSERT_EQ(mock_sck_rises, 27);
}

TEST(test_tare)
{
    hx711_t dev;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    mock_raw_value  = 0x001000;  /* 4096 */
    mock_dout_ready = true;

    ASSERT_EQ(hx711_tare(&dev, 3), HX711_OK);
    /* each call reads 4096 → avg = 4096 */
    ASSERT_EQ(dev.offset, 4096);
    /* three reads, 25 pulses each, one conversion period between them */
    ASSERT_EQ(mock_sck_rises, 75);
    ASSERT_GE(mock_now_us, 2u * MOCK_CONV_10SPS_US);
}

/* 80 SPS: one conversion takes 12.5 ms */
TEST(test_tare_at_80sps)
{
    hx711_t dev;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);
    mock_conv_us    = 12500;
    mock_raw_value  = 0x001000;
    mock_dout_ready = true;

    ASSERT_EQ(hx711_tare(&dev, 10), HX711_OK);
    ASSERT_EQ(dev.offset, 4096);
}

/* The slowest rate in Table 2: RATE = 0 and a 1 MHz clock give fclk/1,105,920,
 * which is 1.106 s per conversion. */
TEST(test_tare_at_slowest_clock)
{
    hx711_t dev;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);
    mock_conv_us    = 1105920;
    mock_raw_value  = 0x001000;
    mock_dout_ready = true;

    ASSERT_EQ(hx711_tare(&dev, 2), HX711_OK);
    ASSERT_EQ(dev.offset, 4096);
}

/* A read just before tare leaves DOUT high, so the first sample waits too */
TEST(test_tare_after_a_read)
{
    hx711_t dev;
    int32_t val;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);
    mock_raw_value  = 0x001000;
    mock_dout_ready = true;

    ASSERT_EQ(hx711_read_raw(&dev, &val), HX711_OK);
    ASSERT_FALSE(hx711_is_ready(&dev));
    ASSERT_EQ(hx711_read_raw(&dev, &val), HX711_ERR_NOT_READY);
    ASSERT_EQ(hx711_tare(&dev, 2), HX711_OK);
    ASSERT_EQ(dev.offset, 4096);
}

TEST(test_tare_conversion_never_ends)
{
    hx711_t dev;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);
    hx711_set_offset(&dev, 7);
    mock_conv_us    = MOCK_CONV_NEVER_US;
    mock_raw_value  = 0x001000;
    mock_dout_ready = true;

    ASSERT_EQ(hx711_tare(&dev, 3), HX711_ERR_TIMEOUT);
    ASSERT_EQ(dev.offset, 7);
    /* the wait covers a whole conversion at the slowest clock, and ends */
    ASSERT_GE(mock_now_us, 1105920u);
    ASSERT_LE(mock_now_us, 2000000u);
}

TEST(test_read_with_scale)
{
    hx711_t dev;
    float fval = 0.0f;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    mock_raw_value  = 0x002000;  /* 8192 */
    mock_dout_ready = true;

    hx711_set_offset(&dev, 4096);
    hx711_set_scale(&dev, 2.0f);

    ASSERT_EQ(hx711_read(&dev, &fval), HX711_OK);
    /* (8192 - 4096) / 2.0 = 2048.0 */
    ASSERT_NEAR(fval, 2048.0f, 0.1f);
}

TEST(test_set_scale_and_offset)
{
    hx711_t dev;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    hx711_set_scale(&dev, 42.5f);
    ASSERT_NEAR(dev.scale, 42.5f, 0.001f);

    hx711_set_offset(&dev, -100);
    ASSERT_EQ(dev.offset, -100);
}

TEST(test_power_down_up)
{
    hx711_t dev;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    hx711_power_down(&dev);
    /* SCK should be high */
    ASSERT_TRUE(mock_sck_level);
    /* delay should have been called with >=60us */
    ASSERT_TRUE(mock_last_delay_us >= 60);

    hx711_power_up(&dev);
    /* SCK should be low */
    ASSERT_FALSE(mock_sck_level);
}

TEST(test_read_raw_null_params)
{
    hx711_t dev;
    int32_t val;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    ASSERT_EQ(hx711_read_raw(NULL, &val), HX711_ERR_PARAM);
    ASSERT_EQ(hx711_read_raw(&dev, NULL), HX711_ERR_PARAM);
}

TEST(test_tare_zero_samples)
{
    hx711_t dev;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);
    ASSERT_EQ(hx711_tare(&dev, 0), HX711_ERR_PARAM);
}

/* ---------- not ready: tare waits, single reads leave it to the caller ---------- */

TEST(test_tare_never_ready_times_out)
{
    hx711_t dev;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    mock_dout_ready = false;  /* DOUT high = not ready */
    ASSERT_EQ(hx711_tare(&dev, 5), HX711_ERR_TIMEOUT);
}

TEST(test_read_not_ready_propagates_through_read)
{
    hx711_t dev;
    float fval = 0.0f;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);
    hx711_set_scale(&dev, 2.0f);

    mock_dout_ready = false;
    hx711_err_t e = hx711_read(&dev, &fval);
    /* hx711_read delegates to read_raw → ERR_NOT_READY propagates */
    ASSERT_EQ(e, HX711_ERR_NOT_READY);
}

/* ABI stability: verify the timeout enum value exists at the expected slot */
TEST(test_err_timeout_value)
{
    ASSERT_EQ((int)HX711_ERR_TIMEOUT, -2);
}

TEST_MAIN("hx711")
    RUN(test_init_ok);
    RUN(test_init_null_params);
    RUN(test_init_invalid_gain);
    RUN(test_is_ready_true);
    RUN(test_is_ready_false);
    RUN(test_read_raw_positive);
    RUN(test_read_raw_negative);
    RUN(test_read_raw_zero);
    RUN(test_read_raw_not_ready);
    RUN(test_gain_128_pulse_count);
    RUN(test_gain_32_pulse_count);
    RUN(test_gain_64_pulse_count);
    RUN(test_tare);
    RUN(test_tare_at_80sps);
    RUN(test_tare_at_slowest_clock);
    RUN(test_tare_after_a_read);
    RUN(test_tare_conversion_never_ends);
    RUN(test_read_with_scale);
    RUN(test_set_scale_and_offset);
    RUN(test_power_down_up);
    RUN(test_read_raw_null_params);
    RUN(test_tare_zero_samples);
    RUN(test_tare_never_ready_times_out);
    RUN(test_read_not_ready_propagates_through_read);
    RUN(test_err_timeout_value);
TEST_MAIN_END()
