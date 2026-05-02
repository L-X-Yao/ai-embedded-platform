// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hx711.h"

/* ---------- mock HX711 serial protocol ---------- */

static bool     mock_dout_ready;      /* false = data ready (active low) */
static uint32_t mock_raw_value;       /* 24-bit value to serve */
static int      mock_bit_index;       /* counts SCK rising edges */
static int      mock_sck_rises;       /* total SCK rising edges */
static int      mock_sck_falls;       /* total SCK falling edges */
static bool     mock_sck_level;       /* current SCK state */
static int      mock_delay_calls;
static uint32_t mock_last_delay_us;

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
}

static bool mock_read_dout(void *ctx)
{
    (void)ctx;

    /* before any clocking: report ready/not-ready, reset bit index for new read */
    if (mock_bit_index == 0 && !mock_sck_level)
        return !mock_dout_ready;

    /* during data clocking (bits 0..23): serve MSB first on SCK high */
    if (mock_bit_index <= 24 && mock_sck_level) {
        int bit_pos = 23 - (mock_bit_index - 1);
        if (bit_pos >= 0)
            return (mock_raw_value >> (unsigned)bit_pos) & 1U ? true : false;
    }

    return false;
}

static void mock_set_sck(void *ctx, bool level)
{
    (void)ctx;

    /* detect end of read cycle: SCK falling after all pulses done,
     * reset bit_index so next read_dout sees ready state again */
    if (!level && mock_sck_level && mock_bit_index >= 25)
        mock_bit_index = 0;

    if (level && !mock_sck_level) {
        mock_sck_rises++;
        mock_bit_index++;
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

/* ---------- HX711_ERR_TIMEOUT: tare propagates not-ready from read_raw --
 * The driver does not use a polling timeout loop (caller handles retry
 * policy), so HX711_ERR_TIMEOUT is defined but never returned.  However,
 * calling tare while the device is not ready exercises the closest
 * error-propagation path. */

TEST(test_tare_not_ready_propagates)
{
    hx711_t dev;
    mock_reset();
    hx711_init(&dev, &mock_hal, HX711_GAIN_128);

    mock_dout_ready = false;  /* DOUT high = not ready */
    hx711_err_t e = hx711_tare(&dev, 5);
    /* tare calls read_raw which returns ERR_NOT_READY, tare propagates it */
    ASSERT_EQ(e, HX711_ERR_NOT_READY);
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
    RUN(test_read_with_scale);
    RUN(test_set_scale_and_offset);
    RUN(test_power_down_up);
    RUN(test_read_raw_null_params);
    RUN(test_tare_zero_samples);
    RUN(test_tare_not_ready_propagates);
    RUN(test_read_not_ready_propagates_through_read);
    RUN(test_err_timeout_value);
TEST_MAIN_END()
