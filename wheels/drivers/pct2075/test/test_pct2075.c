// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "pct2075.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* Temperature register POR value = 0x0000 (0 C) */
    mock_regs[0x00] = 0x00;
    mock_regs[0x01] = 0x00;
}

/* Inject a temperature value into mock register 0x00 (2 bytes).
 * PCT2075: 11-bit signed in bits [15:5], 0.125 C/LSB. */
static void inject_temp_raw(int16_t raw)
{
    mock_regs[0x00] = (uint8_t)((uint16_t)raw >> 8);
    mock_regs[0x01] = (uint8_t)((uint16_t)raw & 0xFFu);
}

static uint32_t mock_waited_ms;

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    mock_waited_ms += ms;
}

static const pct2075_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_delay_ms,
    .ctx       = NULL,
};

static const pct2075_cfg_t default_cfg = {
    .addr         = PCT2075_DEV_ADDR,
    .write_config = false,
    .conf_val     = 0x00,
};

static pct2075_t dev;

static void setup(void)
{
    mock_reset();
    (void)pct2075_init(&dev, &mock_i2c, &default_cfg);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    pct2075_t d;
    pct2075_err_t e = pct2075_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_EQ(d.addr, 0x48);
}

TEST(test_init_writes_config)
{
    mock_reset();
    pct2075_t d;
    pct2075_cfg_t cfg = {
        .addr         = PCT2075_DEV_ADDR,
        .write_config = true,
        .conf_val     = 0x08,  /* Table 10: B[4:3] = 01, queue value = 2 */
    };
    pct2075_err_t e = pct2075_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, PCT2075_OK);
    /* Config register at 0x01 should have been written */
    ASSERT_EQ(mock_regs[0x01], 0x08);
}

/* p.7 "not updated until the first conversion is completed in about
 * 28 ms" */
TEST(test_init_waits_for_first_conversion)
{
    mock_reset();
    mock_waited_ms = 0;
    pct2075_t d;
    ASSERT_EQ(pct2075_init(&d, &mock_i2c, &default_cfg), PCT2075_OK);
    ASSERT_GE(mock_waited_ms, 28u);
}

TEST(test_init_refuses_a_hal_without_delay)
{
    mock_reset();
    pct2075_i2c_t hal = mock_i2c;
    hal.delay_ms = NULL;
    pct2075_t d;
    ASSERT_EQ(pct2075_init(&d, &hal, &default_cfg), PCT2075_ERR_PARAM);
}

TEST(test_null_dev)
{
    ASSERT_EQ(pct2075_init(NULL, &mock_i2c, &default_cfg), PCT2075_ERR_PARAM);
}

TEST(test_null_i2c)
{
    pct2075_t d;
    ASSERT_EQ(pct2075_init(&d, NULL, &default_cfg), PCT2075_ERR_PARAM);
}

TEST(test_null_cfg)
{
    pct2075_t d;
    ASSERT_EQ(pct2075_init(&d, &mock_i2c, NULL), PCT2075_ERR_PARAM);
}

TEST(test_null_read_dev)
{
    float temp = 0.0f;
    ASSERT_EQ(pct2075_read(NULL, &temp), PCT2075_ERR_PARAM);
}

TEST(test_null_read_out)
{
    setup();
    ASSERT_EQ(pct2075_read(&dev, NULL), PCT2075_ERR_PARAM);
}

TEST(test_read_25c)
{
    setup();
    /* +25.0 C: 25.0 / 0.125 = 200, raw = 200 << 5 = 6400 = 0x1900 */
    inject_temp_raw(0x1900);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 25.0f, 0.01f);
}

TEST(test_read_0c)
{
    setup();
    inject_temp_raw(0x0000);
    float temp = 99.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 0.0f, 0.01f);
}

TEST(test_read_neg25c)
{
    setup();
    /* -25.0 C: -25.0 / 0.125 = -200, raw = -200 << 5 = -6400 = 0xE700 */
    inject_temp_raw((int16_t)0xE700);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, -25.0f, 0.01f);
}

TEST(test_read_resolution_0_125)
{
    setup();
    /* +0.125 C: 0.125 / 0.125 = 1, raw = 1 << 5 = 32 = 0x0020 */
    inject_temp_raw(0x0020);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 0.125f, 0.001f);
}

TEST(test_read_125c_max)
{
    setup();
    /* +125.0 C: 125.0 / 0.125 = 1000, raw = 1000 << 5 = 32000 = 0x7D00 */
    inject_temp_raw(0x7D00);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, 125.0f, 0.01f);
}

TEST(test_read_neg55c_min)
{
    setup();
    /* -55.0 C: -55.0 / 0.125 = -440, raw = -440 << 5 = -14080 = 0xC900 */
    inject_temp_raw((int16_t)0xC900);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, -55.0f, 0.01f);
}

TEST(test_i2c_failure_read)
{
    setup();
    mock_fail_next = 1;
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_ERR_I2C);
}

TEST(test_i2c_failure_init)
{
    mock_reset();
    mock_fail_at = 0;
    pct2075_t d;
    ASSERT_EQ(pct2075_init(&d, &mock_i2c, &default_cfg), PCT2075_ERR_I2C);
}

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)PCT2075_OK);
    ASSERT_EQ(-1, (int)PCT2075_ERR_I2C);
    ASSERT_EQ(-2, (int)PCT2075_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    pct2075_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    pct2075_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(pct2075_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_invalid_address_low)
{
    mock_reset();
    pct2075_t d;
    pct2075_cfg_t cfg = { .addr = 0x47, .write_config = false, .conf_val = 0 };
    ASSERT_EQ(pct2075_init(&d, &mock_i2c, &cfg), PCT2075_ERR_PARAM);
}

TEST(test_invalid_address_high)
{
    mock_reset();
    pct2075_t d;
    pct2075_cfg_t cfg = { .addr = 0x50, .write_config = false, .conf_val = 0 };
    ASSERT_EQ(pct2075_init(&d, &mock_i2c, &cfg), PCT2075_ERR_PARAM);
}

TEST(test_custom_address)
{
    mock_reset();
    pct2075_t d;
    pct2075_cfg_t cfg = { .addr = 0x4F, .write_config = false, .conf_val = 0 };
    pct2075_err_t e = pct2075_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_EQ(d.addr, 0x4F);
}

/* NXP PCT2075 Rev. 9, Table 5 (p.8-9): the three-state address pins give
 * 27 slave addresses, and Table 6 (6-pin package) uses three of them. */
TEST(test_table5_addresses)
{
    static const uint8_t table5[27] = {
        0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,     /* rows 1-8 */
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,     /* rows 9-16 */
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,     /* rows 17-24 */
        0x35, 0x36, 0x37,                                   /* rows 25-27 */
    };

    for (unsigned addr = 0; addr <= 0xFFu; addr++) {
        pct2075_err_t want = PCT2075_ERR_PARAM;
        for (size_t i = 0; i < sizeof(table5); i++) {
            if (table5[i] == addr) {
                want = PCT2075_OK;
            }
        }

        mock_reset();
        pct2075_t d;
        pct2075_cfg_t cfg = { .addr = (uint8_t)addr, .write_config = false,
                              .conf_val = 0 };
        ASSERT_EQ(pct2075_init(&d, &mock_i2c, &cfg), want);
        if (want == PCT2075_OK) {
            ASSERT_EQ(d.addr, addr);
        }
    }
}

TEST(test_read_neg0_125)
{
    setup();
    /* -0.125 C: -0.125 / 0.125 = -1, raw = -1 << 5 = -32 = 0xFFE0 */
    inject_temp_raw((int16_t)0xFFE0);
    float temp = 0.0f;
    pct2075_err_t e = pct2075_read(&dev, &temp);
    ASSERT_EQ(e, PCT2075_OK);
    ASSERT_NEAR(temp, -0.125f, 0.001f);
}

TEST(test_config_write_i2c_fail)
{
    mock_reset();
    /* First op (read temp) succeeds, second op (write config) fails */
    mock_fail_at = 1;
    pct2075_t d;
    pct2075_cfg_t cfg = {
        .addr         = PCT2075_DEV_ADDR,
        .write_config = true,
        .conf_val     = 0x08,
    };
    ASSERT_EQ(pct2075_init(&d, &mock_i2c, &cfg), PCT2075_ERR_I2C);
}

TEST_MAIN("pct2075")
    RUN(test_init_ok);
    RUN(test_init_writes_config);
    RUN(test_init_waits_for_first_conversion);
    RUN(test_init_refuses_a_hal_without_delay);
    RUN(test_null_dev);
    RUN(test_null_i2c);
    RUN(test_null_cfg);
    RUN(test_null_read_dev);
    RUN(test_null_read_out);
    RUN(test_read_25c);
    RUN(test_read_0c);
    RUN(test_read_neg25c);
    RUN(test_read_resolution_0_125);
    RUN(test_read_125c_max);
    RUN(test_read_neg55c_min);
    RUN(test_i2c_failure_read);
    RUN(test_i2c_failure_init);
    RUN(test_error_codes);
    RUN(test_zero_init_safety);
    RUN(test_invalid_address_low);
    RUN(test_invalid_address_high);
    RUN(test_custom_address);
    RUN(test_table5_addresses);
    RUN(test_read_neg0_125);
    RUN(test_config_write_i2c_fail);
TEST_MAIN_END()
