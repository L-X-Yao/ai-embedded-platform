// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include <math.h>
#include "xgzp6816d.h"

/* ---------- mock I2C transport ---------- */

#define MOCK_BUF_MAX  16

static int      mock_write_called;
static int      mock_read_called;
static uint8_t  mock_last_addr;
static uint8_t  mock_last_write[MOCK_BUF_MAX];
static uint16_t mock_last_write_len;
static uint8_t  mock_read_response[MOCK_BUF_MAX];
static uint16_t mock_read_response_len;
static int      mock_write_should_fail;
static int      mock_read_should_fail;
static void mock_reset(void)
{
    mock_write_called = 0;
    mock_read_called = 0;
    mock_last_addr = 0;
    memset(mock_last_write, 0, sizeof(mock_last_write));
    mock_last_write_len = 0;
    memset(mock_read_response, 0, sizeof(mock_read_response));
    mock_read_response_len = 0;
    mock_write_should_fail = 0;
    mock_read_should_fail = 0;
}

static bool mock_write(void *ctx, uint8_t dev_addr, const uint8_t *buf, uint16_t len)
{
    (void)ctx;
    mock_write_called++;
    mock_last_addr = dev_addr;
    if (len > MOCK_BUF_MAX) return false;
    if (buf && len > 0) memcpy(mock_last_write, buf, len);
    mock_last_write_len = len;
    return mock_write_should_fail ? false : true;
}

static bool mock_read(void *ctx, uint8_t dev_addr, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    mock_read_called++;
    mock_last_addr = dev_addr;
    if (mock_read_should_fail) return false;
    if (len > mock_read_response_len) return false;
    memcpy(buf, mock_read_response, len);
    return true;
}

static const xgzp6816d_i2c_t mock_i2c = {
    .write = mock_write,
    .read  = mock_read,
    .ctx   = NULL,
};

/* ---------- helpers ---------- */

static void mock_set_pt_response(uint32_t p_raw, uint16_t t_raw)
{
    mock_read_response[0] = 0x00;                          /* status */
    mock_read_response[1] = (uint8_t)((p_raw >> 16) & 0xFF);
    mock_read_response[2] = (uint8_t)((p_raw >>  8) & 0xFF);
    mock_read_response[3] = (uint8_t)( p_raw        & 0xFF);
    mock_read_response[4] = (uint8_t)((t_raw >>  8) & 0xFF);
    mock_read_response[5] = (uint8_t)( t_raw        & 0xFF);
    mock_read_response_len = 6;
}

/* ---------- tests ---------- */

TEST(test_probe_ok)
{
    mock_reset();
    xgzp6816d_err_t e = xgzp6816d_probe(&mock_i2c);
    ASSERT_EQ(e, XGZP6816D_OK);
    ASSERT_EQ(mock_write_called, 1);
    ASSERT_EQ(mock_last_addr, XGZP6816D_DEV_ADDR);
    ASSERT_EQ(mock_last_write_len, 0);
}

TEST(test_probe_no_ack)
{
    mock_reset();
    mock_write_should_fail = 1;
    xgzp6816d_err_t e = xgzp6816d_probe(&mock_i2c);
    ASSERT_EQ(e, XGZP6816D_ERR_NO_ACK);
}

TEST(test_trigger_ok)
{
    mock_reset();
    xgzp6816d_err_t e = xgzp6816d_trigger(&mock_i2c);
    ASSERT_EQ(e, XGZP6816D_OK);
    ASSERT_EQ(mock_write_called, 1);
    ASSERT_EQ(mock_last_addr, XGZP6816D_DEV_ADDR);
    ASSERT_EQ(mock_last_write_len, 1);
    ASSERT_EQ(mock_last_write[0], 0xAC);
}

TEST(test_trigger_i2c_err)
{
    mock_reset();
    mock_write_should_fail = 1;
    xgzp6816d_err_t e = xgzp6816d_trigger(&mock_i2c);
    ASSERT_EQ(e, XGZP6816D_ERR_I2C);
}

TEST(test_read_midrange)
{
    mock_reset();
    /* p_raw = 8388608 (mid 24-bit) → ~70 kPa = 70000 Pa */
    /* t_raw = 22417 → ~25 °C */
    mock_set_pt_response(8388608, 22417);
    xgzp6816d_data_t out = {0};
    xgzp6816d_err_t e = xgzp6816d_read(&mock_i2c, &out);
    ASSERT_EQ(e, XGZP6816D_OK);
    ASSERT_NEAR(out.pressure_pa,    70000.0f, 1.0f);
    ASSERT_NEAR(out.temperature_c,    25.0f, 0.05f);
}

TEST(test_read_low_rail)
{
    mock_reset();
    mock_set_pt_response(1677722, 0);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.pressure_pa, 30000.0f, 1.0f);
}

TEST(test_read_below_low_clamps)
{
    mock_reset();
    mock_set_pt_response(1000000, 0);   /* below low rail */
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.pressure_pa, 30000.0f, 1.0f);
}

TEST(test_read_high_rail)
{
    mock_reset();
    mock_set_pt_response(15099494, 0);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.pressure_pa, 110000.0f, 1.0f);
}

TEST(test_read_above_high_clamps)
{
    mock_reset();
    mock_set_pt_response(16000000, 0);   /* above high rail */
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    /* This is the NEW behavior — pre-wheel flybox driver only clamped low,
     * letting saturated readings produce 119 kPa garbage. */
    ASSERT_NEAR(out.pressure_pa, 110000.0f, 1.0f);
}

TEST(test_read_temp_min)
{
    mock_reset();
    mock_set_pt_response(8388608, 0);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.temperature_c, -40.0f, 0.001f);
}

TEST(test_read_temp_max)
{
    mock_reset();
    mock_set_pt_response(8388608, 65535);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    /* (65535 / 65536) * 190 - 40 ≈ 149.997 */
    ASSERT_NEAR(out.temperature_c, 150.0f, 0.01f);
}

TEST(test_read_i2c_err)
{
    mock_reset();
    mock_read_should_fail = 1;
    xgzp6816d_data_t out = {0};
    xgzp6816d_err_t e = xgzp6816d_read(&mock_i2c, &out);
    ASSERT_EQ(e, XGZP6816D_ERR_I2C);
}

TEST(test_full_cycle)
{
    mock_reset();
    /* probe → trigger → (caller waits) → read */
    ASSERT_EQ(xgzp6816d_probe(&mock_i2c), XGZP6816D_OK);
    ASSERT_EQ(xgzp6816d_trigger(&mock_i2c), XGZP6816D_OK);
    mock_set_pt_response(8388608, 22417);
    xgzp6816d_data_t out = {0};
    ASSERT_EQ(xgzp6816d_read(&mock_i2c, &out), XGZP6816D_OK);
    ASSERT_TRUE(out.pressure_pa > 0.0f);
    ASSERT_TRUE(out.temperature_c > -40.0f && out.temperature_c < 150.0f);
}

TEST(test_null_safety)
{
    xgzp6816d_data_t out;
    ASSERT_EQ(xgzp6816d_probe(NULL), XGZP6816D_ERR_I2C);
    ASSERT_EQ(xgzp6816d_trigger(NULL), XGZP6816D_ERR_I2C);
    ASSERT_EQ(xgzp6816d_read(NULL, &out), XGZP6816D_ERR_I2C);
    mock_reset();
    mock_set_pt_response(8388608, 22417);
    ASSERT_EQ(xgzp6816d_read(&mock_i2c, NULL), XGZP6816D_ERR_I2C);
}

TEST(test_reprobe)
{
    mock_reset();
    ASSERT_EQ(xgzp6816d_probe(&mock_i2c), XGZP6816D_OK);
    ASSERT_EQ(xgzp6816d_probe(&mock_i2c), XGZP6816D_OK);
    ASSERT_EQ(mock_write_called, 2);
}

TEST(test_pressure_standard_atmosphere)
{
    mock_reset();
    /* p_raw=13644071 ↔ ~101.325 kPa (standard atmosphere)
     * Exact: (13644071-1677722)/13421772*80+30 ≈ 101.323 kPa */
    mock_set_pt_response(13644071u, 22417);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.pressure_pa, 101325.0f, 50.0f);
}

TEST(test_pressure_quarter_range)
{
    mock_reset();
    /* p_raw=5033165 ↔ 50 kPa (quarter range) */
    mock_set_pt_response(5033165u, 0);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.pressure_pa, 50000.0f, 5.0f);
}

TEST(test_pressure_three_quarter_range)
{
    mock_reset();
    /* p_raw=11744051 ↔ 90 kPa (three-quarter range) */
    mock_set_pt_response(11744051u, 0);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.pressure_pa, 90000.0f, 5.0f);
}

TEST(test_temp_zero_celsius)
{
    mock_reset();
    /* t_raw=13796 ↔ ~0°C: (13796/65536)*190-40 ≈ -0.005 */
    mock_set_pt_response(8388608u, 13796);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.temperature_c, 0.0f, 0.05f);
}

TEST(test_temp_midrange)
{
    mock_reset();
    /* t_raw=32768 ↔ 55°C: (32768/65536)*190-40 = 55 */
    mock_set_pt_response(8388608u, 32768);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.temperature_c, 55.0f, 0.01f);
}

TEST(test_temp_hundred_celsius)
{
    mock_reset();
    /* t_raw=48286 ↔ ~100°C */
    mock_set_pt_response(8388608u, 48286);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.temperature_c, 100.0f, 0.1f);
}

/* ---------- main ---------- */

TEST_MAIN("xgzp6816d")
    RUN(test_probe_ok);
    RUN(test_probe_no_ack);
    RUN(test_trigger_ok);
    RUN(test_trigger_i2c_err);
    RUN(test_read_midrange);
    RUN(test_read_low_rail);
    RUN(test_read_below_low_clamps);
    RUN(test_read_high_rail);
    RUN(test_read_above_high_clamps);
    RUN(test_read_temp_min);
    RUN(test_read_temp_max);
    RUN(test_read_i2c_err);
    RUN(test_full_cycle);
    RUN(test_null_safety);
    RUN(test_reprobe);
    RUN(test_pressure_standard_atmosphere);
    RUN(test_pressure_quarter_range);
    RUN(test_pressure_three_quarter_range);
    RUN(test_temp_zero_celsius);
    RUN(test_temp_midrange);
    RUN(test_temp_hundred_celsius);
TEST_MAIN_END()

