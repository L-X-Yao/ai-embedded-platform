// SPDX-License-Identifier: MIT
/*
 * test_hw_model_max17048.c — tests using hardware model
 *
 * Validates MAX17048 driver against a datasheet-accurate model.
 * The model enforces register layout, voltage/SOC/CRATE encoding,
 * CONFIG bit fields (sleep/alert/threshold), and quickstart detection.
 */

#include "test_harness.h"
#include "hw_model_max17048.h"
#include "../max17048.h"

static max17048_model_t model;

static const max17048_i2c_t model_i2c = {
    .write_reg = max17048_model_write_reg,
    .read_regs = max17048_model_read_regs,
    .ctx       = &model,
    .write_raw = max17048_model_write_raw,
};

static max17048_t dev;

static void setup(void)
{
    max17048_model_init(&model);
    max17048_init(&dev, &model_i2c);
}

/* ─── write rules ───────────────────────────────────────────── */

TEST(test_model_ignores_a_write_without_both_bytes)
{
    static const uint8_t one_byte[] = { HW_REG_CONFIG, 0x12 };

    max17048_model_init(&model);
    ASSERT_TRUE(max17048_model_write_reg(&model, MAX17048_DEV_ADDR,
                                         HW_REG_CONFIG, 0x12));
    ASSERT_TRUE(max17048_model_write_raw(&model, MAX17048_DEV_ADDR,
                                         one_byte, sizeof(one_byte)));
    ASSERT_EQ_HEX(model.regs[HW_REG_CONFIG], 0x97);
    ASSERT_EQ_HEX(model.regs[HW_REG_CONFIG + 1], 0x1C);
}

TEST(test_model_stores_a_write_with_both_bytes)
{
    static const uint8_t word[] = { HW_REG_CONFIG, 0x12, 0x34 };

    max17048_model_init(&model);
    ASSERT_TRUE(max17048_model_write_raw(&model, MAX17048_DEV_ADDR,
                                         word, sizeof(word)));
    ASSERT_EQ_HEX(model.regs[HW_REG_CONFIG], 0x12);
    ASSERT_EQ_HEX(model.regs[HW_REG_CONFIG + 1], 0x34);
}

/* ─── init ──────────────────────────────────────────────────── */

TEST(test_init_with_model)
{
    max17048_model_init(&model);
    max17048_t d;
    max17048_err_t err = max17048_init(&d, &model_i2c);
    ASSERT_EQ(err, MAX17048_OK);
    ASSERT_TRUE(model.read_count > 0);
}

TEST(test_init_bad_version_zero)
{
    max17048_model_init(&model);
    max17048_model_set_version(&model, 0x0000);
    max17048_t d;
    ASSERT_EQ(max17048_init(&d, &model_i2c), MAX17048_ERR_ID);
}

TEST(test_init_bad_version_ffff)
{
    max17048_model_init(&model);
    max17048_model_set_version(&model, 0xFFFF);
    max17048_t d;
    ASSERT_EQ(max17048_init(&d, &model_i2c), MAX17048_ERR_ID);
}

/* ─── voltage round-trip ────────────────────────────────────── */

TEST(test_voltage_4v05)
{
    setup();
    max17048_model_set_voltage(&model, 4.05f);

    float v;
    ASSERT_EQ(max17048_read_voltage(&dev, &v), MAX17048_OK);
    ASSERT_NEAR(v, 4.05f, 0.01f);
}

TEST(test_voltage_3v70)
{
    setup();
    max17048_model_set_voltage(&model, 3.70f);

    float v;
    ASSERT_EQ(max17048_read_voltage(&dev, &v), MAX17048_OK);
    ASSERT_NEAR(v, 3.70f, 0.01f);
}

TEST(test_voltage_3v00_low_battery)
{
    setup();
    max17048_model_set_voltage(&model, 3.00f);

    float v;
    ASSERT_EQ(max17048_read_voltage(&dev, &v), MAX17048_OK);
    ASSERT_NEAR(v, 3.00f, 0.01f);
}

TEST(test_voltage_zero)
{
    setup();
    max17048_model_set_voltage(&model, 0.0f);

    float v;
    ASSERT_EQ(max17048_read_voltage(&dev, &v), MAX17048_OK);
    ASSERT_NEAR(v, 0.0f, 0.001f);
}

/* ─── SOC round-trip ────────────────────────────────────────── */

TEST(test_soc_100_percent)
{
    setup();
    max17048_model_set_soc(&model, 100.0f);

    float soc;
    ASSERT_EQ(max17048_read_soc(&dev, &soc), MAX17048_OK);
    ASSERT_NEAR(soc, 100.0f, 0.5f);
}

TEST(test_soc_50_percent)
{
    setup();
    max17048_model_set_soc(&model, 50.0f);

    float soc;
    ASSERT_EQ(max17048_read_soc(&dev, &soc), MAX17048_OK);
    ASSERT_NEAR(soc, 50.0f, 0.5f);
}

TEST(test_soc_0_percent)
{
    setup();
    max17048_model_set_soc(&model, 0.0f);

    float soc;
    ASSERT_EQ(max17048_read_soc(&dev, &soc), MAX17048_OK);
    ASSERT_NEAR(soc, 0.0f, 0.5f);
}

TEST(test_soc_fractional)
{
    setup();
    max17048_model_set_soc(&model, 73.5f);

    float soc;
    ASSERT_EQ(max17048_read_soc(&dev, &soc), MAX17048_OK);
    ASSERT_NEAR(soc, 73.5f, 0.5f);
}

/* ─── full read round-trip (voltage + SOC + CRATE) ──────────── */

TEST(test_full_read_roundtrip)
{
    setup();
    max17048_model_set_voltage(&model, 3.85f);
    max17048_model_set_soc(&model, 62.0f);
    max17048_model_set_crate(&model, 5.0f);

    max17048_data_t data;
    ASSERT_EQ(max17048_read(&dev, &data), MAX17048_OK);
    ASSERT_NEAR(data.voltage_v, 3.85f, 0.01f);
    ASSERT_NEAR(data.soc_pct, 62.0f, 0.5f);
    ASSERT_NEAR(data.charge_rate, 5.0f, 0.5f);
}

TEST(test_full_read_discharging)
{
    setup();
    max17048_model_set_voltage(&model, 3.50f);
    max17048_model_set_soc(&model, 30.0f);
    max17048_model_set_crate(&model, -10.0f);

    max17048_data_t data;
    ASSERT_EQ(max17048_read(&dev, &data), MAX17048_OK);
    ASSERT_NEAR(data.voltage_v, 3.50f, 0.01f);
    ASSERT_NEAR(data.soc_pct, 30.0f, 0.5f);
    ASSERT_TRUE(data.charge_rate < 0.0f);
    ASSERT_NEAR(data.charge_rate, -10.0f, 0.5f);
}

TEST(test_full_read_idle)
{
    setup();
    max17048_model_set_voltage(&model, 4.20f);
    max17048_model_set_soc(&model, 100.0f);
    max17048_model_set_crate(&model, 0.0f);

    max17048_data_t data;
    ASSERT_EQ(max17048_read(&dev, &data), MAX17048_OK);
    ASSERT_NEAR(data.charge_rate, 0.0f, 0.5f);
}

/* ─── quickstart detection ──────────────────────────────────── */

TEST(test_quickstart_detected_by_model)
{
    setup();
    ASSERT_FALSE(model.quickstart_triggered);

    ASSERT_EQ(max17048_quickstart(&dev), MAX17048_OK);
    ASSERT_TRUE(model.quickstart_triggered);
    /* MODE register byte 0 should have bit 6 set (0x4000 >> 8 = 0x40) */
    ASSERT_TRUE(model.regs[HW_REG_MODE] & 0x40u);
}

/* ─── alert threshold round-trip ────────────────────────────── */

TEST(test_alert_threshold_10_pct)
{
    setup();
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 10), MAX17048_OK);
    ASSERT_EQ(max17048_model_get_alert_threshold(&model), 10);
}

TEST(test_alert_threshold_1_pct)
{
    setup();
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 1), MAX17048_OK);
    ASSERT_EQ(max17048_model_get_alert_threshold(&model), 1);
}

TEST(test_alert_threshold_32_pct)
{
    setup();
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 32), MAX17048_OK);
    ASSERT_EQ(max17048_model_get_alert_threshold(&model), 32);
}

/* ─── alert flag ────────────────────────────────────────────── */

TEST(test_clear_alert_flag)
{
    setup();
    max17048_model_trigger_alert(&model);
    ASSERT_TRUE(max17048_model_is_alert_set(&model));

    ASSERT_EQ(max17048_clear_alert(&dev), MAX17048_OK);
    ASSERT_FALSE(max17048_model_is_alert_set(&model));
}

/* ─── sleep / wake round-trip ───────────────────────────────── */

TEST(test_sleep_sets_bit)
{
    setup();
    ASSERT_FALSE(max17048_model_is_sleeping(&model));

    ASSERT_EQ(max17048_sleep(&dev), MAX17048_OK);
    ASSERT_TRUE(max17048_model_is_sleeping(&model));
}

TEST(test_wake_clears_bit)
{
    setup();
    ASSERT_EQ(max17048_sleep(&dev), MAX17048_OK);
    ASSERT_TRUE(max17048_model_is_sleeping(&model));

    ASSERT_EQ(max17048_wake(&dev), MAX17048_OK);
    ASSERT_FALSE(max17048_model_is_sleeping(&model));
}

TEST(test_sleep_wake_cycle)
{
    setup();
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(max17048_sleep(&dev), MAX17048_OK);
        ASSERT_TRUE(max17048_model_is_sleeping(&model));
        ASSERT_EQ(max17048_wake(&dev), MAX17048_OK);
        ASSERT_FALSE(max17048_model_is_sleeping(&model));
    }
}

/* ─── version read-back ─────────────────────────────────────── */

TEST(test_version_readback)
{
    setup();
    uint16_t ver;
    ASSERT_EQ(max17048_version(&dev, &ver), MAX17048_OK);
    ASSERT_EQ(ver, 0x0011);
}

TEST(test_version_custom)
{
    max17048_model_init(&model);
    max17048_model_set_version(&model, 0x0023);
    max17048_t d;
    ASSERT_EQ(max17048_init(&d, &model_i2c), MAX17048_OK);

    uint16_t ver;
    ASSERT_EQ(max17048_version(&d, &ver), MAX17048_OK);
    ASSERT_EQ(ver, 0x0023);
}

/* ─── POR defaults read-back ────────────────────────────────── */

TEST(test_por_defaults)
{
    setup();

    /* SOC should read as 100% (POR = 0x6400) */
    float soc;
    ASSERT_EQ(max17048_read_soc(&dev, &soc), MAX17048_OK);
    ASSERT_NEAR(soc, 100.0f, 0.5f);

    /* Voltage should read as ~4.05V (POR = 0xCE40) */
    float v;
    ASSERT_EQ(max17048_read_voltage(&dev, &v), MAX17048_OK);
    ASSERT_NEAR(v, 4.05f, 0.05f);

    /* Not sleeping at POR */
    ASSERT_FALSE(max17048_model_is_sleeping(&model));

    /* No alert at POR */
    ASSERT_FALSE(max17048_model_is_alert_set(&model));
}

/* ─── sequential value updates ──────────────────────────────── */

TEST(test_sequential_voltage_updates)
{
    setup();
    float voltages[] = {4.20f, 3.90f, 3.60f, 3.30f, 3.00f};
    for (int i = 0; i < 5; i++) {
        max17048_model_set_voltage(&model, voltages[i]);
        float v;
        ASSERT_EQ(max17048_read_voltage(&dev, &v), MAX17048_OK);
        ASSERT_NEAR(v, voltages[i], 0.01f);
    }
}

TEST_MAIN("max17048 hardware model")
    RUN(test_init_with_model);
    RUN(test_model_ignores_a_write_without_both_bytes);
    RUN(test_model_stores_a_write_with_both_bytes);
    RUN(test_init_bad_version_zero);
    RUN(test_init_bad_version_ffff);
    RUN(test_voltage_4v05);
    RUN(test_voltage_3v70);
    RUN(test_voltage_3v00_low_battery);
    RUN(test_voltage_zero);
    RUN(test_soc_100_percent);
    RUN(test_soc_50_percent);
    RUN(test_soc_0_percent);
    RUN(test_soc_fractional);
    RUN(test_full_read_roundtrip);
    RUN(test_full_read_discharging);
    RUN(test_full_read_idle);
    RUN(test_quickstart_detected_by_model);
    RUN(test_alert_threshold_10_pct);
    RUN(test_alert_threshold_1_pct);
    RUN(test_alert_threshold_32_pct);
    RUN(test_clear_alert_flag);
    RUN(test_sleep_sets_bit);
    RUN(test_wake_clears_bit);
    RUN(test_sleep_wake_cycle);
    RUN(test_version_readback);
    RUN(test_version_custom);
    RUN(test_por_defaults);
    RUN(test_sequential_voltage_updates);
TEST_MAIN_END()
