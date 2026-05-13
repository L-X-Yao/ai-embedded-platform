// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "ht16k33.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
}

static const ht16k33_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static const ht16k33_cfg_t default_cfg = {
    .addr       = 0x70,
    .brightness = 8,
};

static ht16k33_t dev;

static void setup(void)
{
    mock_reset();
    (void)ht16k33_init(&dev, &mock_i2c, &default_cfg);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ht16k33_t d;
    ht16k33_err_t e = ht16k33_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, HT16K33_OK);
    ASSERT_EQ(d.addr, 0x70);
}

TEST(test_init_sends_oscillator_on)
{
    mock_reset();
    ht16k33_t d;
    (void)ht16k33_init(&d, &mock_i2c, &default_cfg);
    /* Oscillator on = 0x21 is written as reg address.
     * mock_regs[0x21] should have been written. */
    ASSERT_TRUE(mock_write_count >= 3); /* osc + display + brightness */
}

TEST(test_init_custom_addr)
{
    mock_reset();
    ht16k33_t d;
    ht16k33_cfg_t cfg = { .addr = 0x77, .brightness = 0 };
    ht16k33_err_t e = ht16k33_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, HT16K33_OK);
    ASSERT_EQ(d.addr, 0x77);
}

TEST(test_init_invalid_addr_low)
{
    mock_reset();
    ht16k33_t d;
    ht16k33_cfg_t cfg = { .addr = 0x6F, .brightness = 0 };
    ASSERT_EQ(ht16k33_init(&d, &mock_i2c, &cfg), HT16K33_ERR_PARAM);
}

TEST(test_init_invalid_addr_high)
{
    mock_reset();
    ht16k33_t d;
    ht16k33_cfg_t cfg = { .addr = 0x78, .brightness = 0 };
    ASSERT_EQ(ht16k33_init(&d, &mock_i2c, &cfg), HT16K33_ERR_PARAM);
}

TEST(test_init_invalid_brightness)
{
    mock_reset();
    ht16k33_t d;
    ht16k33_cfg_t cfg = { .addr = 0x70, .brightness = 16 };
    ASSERT_EQ(ht16k33_init(&d, &mock_i2c, &cfg), HT16K33_ERR_PARAM);
}

TEST(test_init_i2c_fail_first_cmd)
{
    mock_reset();
    mock_fail_at = 0; /* fail on first write (oscillator on) */
    ht16k33_t d;
    ASSERT_EQ(ht16k33_init(&d, &mock_i2c, &default_cfg), HT16K33_ERR_I2C);
}

TEST(test_init_i2c_fail_second_cmd)
{
    mock_reset();
    mock_fail_at = 1; /* fail on second write (display on) */
    ht16k33_t d;
    ASSERT_EQ(ht16k33_init(&d, &mock_i2c, &default_cfg), HT16K33_ERR_I2C);
}

TEST(test_init_i2c_fail_third_cmd)
{
    mock_reset();
    mock_fail_at = 2; /* fail on third write (brightness) */
    ht16k33_t d;
    ASSERT_EQ(ht16k33_init(&d, &mock_i2c, &default_cfg), HT16K33_ERR_I2C);
}

/* ---------- brightness tests ---------- */

TEST(test_brightness_zero)
{
    setup();
    ASSERT_EQ(ht16k33_set_brightness(&dev, 0), HT16K33_OK);
}

TEST(test_brightness_max)
{
    setup();
    ASSERT_EQ(ht16k33_set_brightness(&dev, 15), HT16K33_OK);
}

TEST(test_brightness_over_max)
{
    setup();
    ASSERT_EQ(ht16k33_set_brightness(&dev, 16), HT16K33_ERR_PARAM);
}

TEST(test_brightness_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(ht16k33_set_brightness(&dev, 5), HT16K33_ERR_I2C);
}

/* ---------- write_raw tests ---------- */

TEST(test_write_raw_single_byte)
{
    setup();
    uint8_t data = 0xAB;
    ASSERT_EQ(ht16k33_write_raw(&dev, &data, 1), HT16K33_OK);
    /* Byte written to mock_regs[0x00] */
    ASSERT_EQ(mock_regs[0x00], 0xAB);
}

TEST(test_write_raw_full)
{
    setup();
    uint8_t data[16];
    for (int i = 0; i < 16; i++) {
        data[i] = (uint8_t)(i * 0x11);
    }
    ASSERT_EQ(ht16k33_write_raw(&dev, data, 16), HT16K33_OK);
    for (int i = 0; i < 16; i++) {
        ASSERT_EQ(mock_regs[i], (uint8_t)(i * 0x11));
    }
}

TEST(test_write_raw_too_long)
{
    setup();
    uint8_t data[17];
    memset(data, 0xFF, sizeof(data));
    ASSERT_EQ(ht16k33_write_raw(&dev, data, 17), HT16K33_ERR_PARAM);
}

TEST(test_write_raw_zero_len)
{
    setup();
    uint8_t data = 0x00;
    ASSERT_EQ(ht16k33_write_raw(&dev, &data, 0), HT16K33_ERR_PARAM);
}

TEST(test_write_raw_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    uint8_t data = 0xFF;
    ASSERT_EQ(ht16k33_write_raw(&dev, &data, 1), HT16K33_ERR_I2C);
}

/* ---------- clear tests ---------- */

TEST(test_clear)
{
    setup();
    /* Write some data first */
    uint8_t data[16];
    memset(data, 0xFF, sizeof(data));
    (void)ht16k33_write_raw(&dev, data, 16);

    /* Clear should zero all display RAM */
    ASSERT_EQ(ht16k33_clear(&dev), HT16K33_OK);
    for (int i = 0; i < 16; i++) {
        ASSERT_EQ(mock_regs[i], 0x00);
    }
}

TEST(test_clear_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(ht16k33_clear(&dev), HT16K33_ERR_I2C);
}

/* ---------- NULL safety ---------- */

TEST(test_null_dev_init)
{
    ASSERT_EQ(ht16k33_init(NULL, &mock_i2c, &default_cfg), HT16K33_ERR_PARAM);
}

TEST(test_null_i2c_init)
{
    ht16k33_t d;
    ASSERT_EQ(ht16k33_init(&d, NULL, &default_cfg), HT16K33_ERR_PARAM);
}

TEST(test_null_cfg_init)
{
    ht16k33_t d;
    ASSERT_EQ(ht16k33_init(&d, &mock_i2c, NULL), HT16K33_ERR_PARAM);
}

TEST(test_null_dev_brightness)
{
    ASSERT_EQ(ht16k33_set_brightness(NULL, 5), HT16K33_ERR_PARAM);
}

TEST(test_null_dev_write_raw)
{
    uint8_t data = 0x00;
    ASSERT_EQ(ht16k33_write_raw(NULL, &data, 1), HT16K33_ERR_PARAM);
}

TEST(test_null_buf_write_raw)
{
    setup();
    ASSERT_EQ(ht16k33_write_raw(&dev, NULL, 1), HT16K33_ERR_PARAM);
}

TEST(test_null_dev_clear)
{
    ASSERT_EQ(ht16k33_clear(NULL), HT16K33_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    /* Zero HAL struct has NULL function pointers — init must reject it */
    ht16k33_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ht16k33_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(ht16k33_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

/* ---------- error code values ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)HT16K33_OK);
    ASSERT_EQ(-1, (int)HT16K33_ERR_I2C);
    ASSERT_EQ(-2, (int)HT16K33_ERR_PARAM);
}

/* ---------- uninit dev safety ---------- */

TEST(test_uninit_dev_brightness)
{
    /* A zeroed dev has i2c == NULL — should return ERR_PARAM */
    ht16k33_t d;
    memset(&d, 0, sizeof(d));
    ASSERT_EQ(ht16k33_set_brightness(&d, 5), HT16K33_ERR_PARAM);
}

TEST(test_uninit_dev_write_raw)
{
    ht16k33_t d;
    memset(&d, 0, sizeof(d));
    uint8_t data = 0x00;
    ASSERT_EQ(ht16k33_write_raw(&d, &data, 1), HT16K33_ERR_PARAM);
}

TEST(test_uninit_dev_clear)
{
    ht16k33_t d;
    memset(&d, 0, sizeof(d));
    ASSERT_EQ(ht16k33_clear(&d), HT16K33_ERR_PARAM);
}

TEST_MAIN("ht16k33")
    RUN(test_init_ok);
    RUN(test_init_sends_oscillator_on);
    RUN(test_init_custom_addr);
    RUN(test_init_invalid_addr_low);
    RUN(test_init_invalid_addr_high);
    RUN(test_init_invalid_brightness);
    RUN(test_init_i2c_fail_first_cmd);
    RUN(test_init_i2c_fail_second_cmd);
    RUN(test_init_i2c_fail_third_cmd);
    RUN(test_brightness_zero);
    RUN(test_brightness_max);
    RUN(test_brightness_over_max);
    RUN(test_brightness_i2c_fail);
    RUN(test_write_raw_single_byte);
    RUN(test_write_raw_full);
    RUN(test_write_raw_too_long);
    RUN(test_write_raw_zero_len);
    RUN(test_write_raw_i2c_fail);
    RUN(test_clear);
    RUN(test_clear_i2c_fail);
    RUN(test_null_dev_init);
    RUN(test_null_i2c_init);
    RUN(test_null_cfg_init);
    RUN(test_null_dev_brightness);
    RUN(test_null_dev_write_raw);
    RUN(test_null_buf_write_raw);
    RUN(test_null_dev_clear);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_uninit_dev_brightness);
    RUN(test_uninit_dev_write_raw);
    RUN(test_uninit_dev_clear);
TEST_MAIN_END()
