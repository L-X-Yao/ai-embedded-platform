// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "max17048.h"

#define REG_VCELL   0x02
#define REG_SOC     0x04
#define REG_MODE    0x06
#define REG_VERSION 0x08
#define REG_CONFIG  0x0C
#define REG_CRATE   0x16

static const max17048_i2c_t mock_bus = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static max17048_t dev;

static void mock_reset_chip(void)
{
    mock_i2c_reset();
    mock_regs[REG_VERSION]     = 0x00;
    mock_regs[REG_VERSION + 1] = 0x11;
    mock_regs[REG_CONFIG]      = 0x97;
    mock_regs[REG_CONFIG + 1]  = 0x1C;
}

/* ─── init tests ─────────────────────────────────────────────── */

TEST(test_init_ok)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);
}

TEST(test_init_null)
{
    ASSERT_EQ(max17048_init(NULL, &mock_bus), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_init(&dev, NULL), MAX17048_ERR_PARAM);
    max17048_i2c_t bad = { .write_reg = NULL, .read_regs = mock_i2c_read_regs };
    ASSERT_EQ(max17048_init(&dev, &bad), MAX17048_ERR_PARAM);
    max17048_i2c_t bad2 = { .write_reg = mock_i2c_write_reg, .read_regs = NULL };
    ASSERT_EQ(max17048_init(&dev, &bad2), MAX17048_ERR_PARAM);
}

TEST(test_init_i2c_fail)
{
    mock_reset_chip();
    mock_fail_at = 0;
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_ERR_I2C);
}

TEST(test_init_bad_version_zero)
{
    mock_i2c_reset();
    mock_regs[REG_VERSION] = 0x00;
    mock_regs[REG_VERSION + 1] = 0x00;
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_ERR_ID);
}

TEST(test_init_bad_version_ffff)
{
    mock_i2c_reset();
    mock_regs[REG_VERSION] = 0xFF;
    mock_regs[REG_VERSION + 1] = 0xFF;
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_ERR_ID);
}

/* ─── read tests ─────────────────────────────────────────────── */

TEST(test_read_full)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);

    mock_regs[REG_VCELL]     = 0xA0;
    mock_regs[REG_VCELL + 1] = 0x00;
    mock_regs[REG_SOC]       = 0x32;
    mock_regs[REG_SOC + 1]   = 0x80;
    mock_regs[REG_CRATE]     = 0x00;
    mock_regs[REG_CRATE + 1] = 0x00;

    max17048_data_t data;
    ASSERT_EQ(max17048_read(&dev, &data), MAX17048_OK);
    ASSERT_NEAR(data.voltage_v, (float)0xA000 * 78.125e-6f, 0.01f);
    ASSERT_NEAR(data.soc_pct, 50.5f, 0.01f);
    ASSERT_NEAR(data.charge_rate, 0.0f, 0.01f);
}

TEST(test_read_null)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    max17048_data_t data;
    ASSERT_EQ(max17048_read(NULL, &data), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_read(&dev, NULL), MAX17048_ERR_PARAM);
}

TEST(test_read_i2c_fail_vcell)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    mock_fail_at = (int)mock_op_count;
    max17048_data_t data;
    ASSERT_EQ(max17048_read(&dev, &data), MAX17048_ERR_I2C);
}

TEST(test_read_voltage)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    mock_regs[REG_VCELL]     = 0x80;
    mock_regs[REG_VCELL + 1] = 0x00;
    float v;
    ASSERT_EQ(max17048_read_voltage(&dev, &v), MAX17048_OK);
    ASSERT_NEAR(v, (float)0x8000 * 78.125e-6f, 0.01f);
}

TEST(test_read_voltage_null)
{
    ASSERT_EQ(max17048_read_voltage(NULL, NULL), MAX17048_ERR_PARAM);
}

TEST(test_read_soc)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    mock_regs[REG_SOC]     = 0x64;
    mock_regs[REG_SOC + 1] = 0x00;
    float soc;
    ASSERT_EQ(max17048_read_soc(&dev, &soc), MAX17048_OK);
    ASSERT_NEAR(soc, 100.0f, 0.01f);
}

TEST(test_read_soc_null)
{
    ASSERT_EQ(max17048_read_soc(NULL, NULL), MAX17048_ERR_PARAM);
}

/* ─── quickstart test ────────────────────────────────────────── */

TEST(test_quickstart)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    ASSERT_EQ(max17048_quickstart(&dev), MAX17048_OK);
    ASSERT_EQ(mock_regs[REG_MODE], 0x40);
}

TEST(test_quickstart_null)
{
    ASSERT_EQ(max17048_quickstart(NULL), MAX17048_ERR_PARAM);
}

/* ─── alert threshold tests ──────────────────────────────────── */

TEST(test_set_alert_threshold)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 10), MAX17048_OK);
    ASSERT_EQ(mock_regs[REG_CONFIG + 1] & 0x1F, 22);
}

TEST(test_set_alert_threshold_bounds)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 0), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 33), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 1), MAX17048_OK);
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 32), MAX17048_OK);
}

TEST(test_set_alert_null)
{
    ASSERT_EQ(max17048_set_alert_threshold(NULL, 10), MAX17048_ERR_PARAM);
}

/* ─── clear alert test ───────────────────────────────────────── */

TEST(test_clear_alert)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    mock_regs[REG_CONFIG] = 0x97;
    mock_regs[REG_CONFIG + 1] = 0x3C;
    ASSERT_EQ(max17048_clear_alert(&dev), MAX17048_OK);
    ASSERT_EQ(mock_regs[REG_CONFIG + 1] & 0x20, 0);
}

TEST(test_clear_alert_null)
{
    ASSERT_EQ(max17048_clear_alert(NULL), MAX17048_ERR_PARAM);
}

/* ─── sleep / wake tests ─────────────────────────────────────── */

TEST(test_sleep_wake)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    ASSERT_EQ(max17048_sleep(&dev), MAX17048_OK);
    ASSERT_TRUE(mock_regs[REG_CONFIG + 1] & 0x80);
    ASSERT_EQ(max17048_wake(&dev), MAX17048_OK);
    ASSERT_FALSE(mock_regs[REG_CONFIG + 1] & 0x80);
}

TEST(test_sleep_null)
{
    ASSERT_EQ(max17048_sleep(NULL), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_wake(NULL), MAX17048_ERR_PARAM);
}

/* ─── version test ───────────────────────────────────────────── */

TEST(test_version)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    uint16_t ver;
    ASSERT_EQ(max17048_version(&dev, &ver), MAX17048_OK);
    ASSERT_EQ(ver, 0x0011);
}

TEST(test_version_null)
{
    ASSERT_EQ(max17048_version(NULL, NULL), MAX17048_ERR_PARAM);
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    ASSERT_EQ(max17048_version(&dev, NULL), MAX17048_ERR_PARAM);
}

TEST(test_sleep_sets_ensleep_before_sleep)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);

    ASSERT_EQ(max17048_sleep(&dev), MAX17048_OK);
    /* MODE.EnSleep is bit 13, CONFIG.SLEEP is bit 7 */
    ASSERT_EQ(mock_regs[0x06] & 0x20, 0x20);
    ASSERT_EQ(mock_regs[REG_CONFIG + 1] & 0x80, 0x80);
}

TEST(test_clear_alert_clears_the_status_flags)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);

    /* EnVR stays set, the SC and VL flags go */
    mock_regs[0x1A] = 0x64;
    ASSERT_EQ(max17048_clear_alert(&dev), MAX17048_OK);
    ASSERT_EQ_HEX(mock_regs[0x1A], 0x40);
}

TEST_MAIN("max17048")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_init_i2c_fail);
    RUN(test_init_bad_version_zero);
    RUN(test_init_bad_version_ffff);
    RUN(test_read_full);
    RUN(test_read_null);
    RUN(test_read_i2c_fail_vcell);
    RUN(test_read_voltage);
    RUN(test_read_voltage_null);
    RUN(test_read_soc);
    RUN(test_read_soc_null);
    RUN(test_quickstart);
    RUN(test_quickstart_null);
    RUN(test_set_alert_threshold);
    RUN(test_set_alert_threshold_bounds);
    RUN(test_set_alert_null);
    RUN(test_clear_alert);
    RUN(test_clear_alert_null);
    RUN(test_sleep_wake);
    RUN(test_sleep_null);
    RUN(test_version);
    RUN(test_version_null);
    RUN(test_sleep_sets_ensleep_before_sleep);
    RUN(test_clear_alert_clears_the_status_flags);
TEST_MAIN_END()
