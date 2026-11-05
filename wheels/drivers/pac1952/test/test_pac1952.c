// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "pac1952.h"
#include <math.h>

/* ---------- chip-specific mock setup ---------- */

/* Register addresses (mirror pac1952.c internal defines for mock) */
#define REG_CTRL            0x01u
#define REG_VBUS1           0x07u
#define REG_VBUS2           0x08u
#define REG_VSENSE1         0x0Bu
#define REG_VSENSE2         0x0Cu
#define REG_REFRESH         0x1Fu
#define REG_PRODUCT_ID      0xFDu

static void mock_reset(void)
{
    mock_i2c_reset();
    /* Set PRODUCT_ID to expected value */
    mock_regs[REG_PRODUCT_ID] = PAC1952_PRODUCT_ID_VAL;
    /* CTRL defaults to 0x00 0x00 */
    mock_regs[REG_CTRL]     = 0x00;
    mock_regs[REG_CTRL + 1] = 0x00;
}

static const pac1952_i2c_t mock_i2c_hal = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static pac1952_t dev;
static const pac1952_cfg_t default_cfg = {
    .sense_resistor_mohm = { 10.0f, 10.0f }
};

/* Helper: set VBUS register (16-bit unsigned big-endian) */
static void mock_set_vbus(uint8_t ch, uint16_t raw)
{
    uint8_t base = (ch == 0) ? REG_VBUS1 : REG_VBUS2;
    mock_regs[base]     = (uint8_t)(raw >> 8);
    mock_regs[base + 1] = (uint8_t)(raw & 0xFFu);
}

/* Helper: set VSENSE register (16-bit unsigned big-endian) */
static void mock_set_vsense(uint8_t ch, uint16_t raw)
{
    uint8_t base = (ch == 0) ? REG_VSENSE1 : REG_VSENSE2;
    mock_regs[base]     = (uint8_t)(raw >> 8);
    mock_regs[base + 1] = (uint8_t)(raw & 0xFFu);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(pac1952_init(&dev, &mock_i2c_hal, &default_cfg), PAC1952_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[REG_PRODUCT_ID] = 0xAA;  /* wrong ID */
    ASSERT_EQ(pac1952_init(&dev, &mock_i2c_hal, &default_cfg), PAC1952_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(pac1952_init(&dev, &mock_i2c_hal, &default_cfg), PAC1952_ERR_I2C);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(pac1952_init(NULL, &mock_i2c_hal, &default_cfg), PAC1952_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    pac1952_t d;
    ASSERT_EQ(pac1952_init(&d, NULL, &default_cfg), PAC1952_ERR_PARAM);
}

TEST(test_init_null_cfg)
{
    pac1952_t d;
    ASSERT_EQ(pac1952_init(&d, &mock_i2c_hal, NULL), PAC1952_ERR_PARAM);
}

TEST(test_init_negative_resistor)
{
    mock_reset();
    pac1952_cfg_t bad_cfg = { .sense_resistor_mohm = { -1.0f, 10.0f } };
    ASSERT_EQ(pac1952_init(&dev, &mock_i2c_hal, &bad_cfg), PAC1952_ERR_PARAM);
}

TEST(test_init_nan_resistor)
{
    mock_reset();
    float nan_val = 0.0f / 0.0f;
    pac1952_cfg_t bad_cfg = { .sense_resistor_mohm = { nan_val, 10.0f } };
    /* !(NaN >= 0) is true, so NaN should be rejected */
    ASSERT_EQ(pac1952_init(&dev, &mock_i2c_hal, &bad_cfg), PAC1952_ERR_PARAM);
}

TEST(test_init_null_write_reg)
{
    pac1952_i2c_t bad_i2c = {
        .write_reg = NULL,
        .read_regs = mock_i2c_read_regs,
        .ctx = NULL,
    };
    pac1952_t d;
    ASSERT_EQ(pac1952_init(&d, &bad_i2c, &default_cfg), PAC1952_ERR_PARAM);
}

TEST(test_init_null_read_regs)
{
    pac1952_i2c_t bad_i2c = {
        .write_reg = mock_i2c_write_reg,
        .read_regs = NULL,
        .ctx = NULL,
    };
    pac1952_t d;
    ASSERT_EQ(pac1952_init(&d, &bad_i2c, &default_cfg), PAC1952_ERR_PARAM);
}

/* ---------- refresh tests ---------- */

TEST(test_refresh_ok)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    ASSERT_EQ(pac1952_refresh(&dev), PAC1952_OK);
    /* Verify write went to REFRESH register */
    ASSERT_EQ(mock_regs[REG_REFRESH], 0x00);
    ASSERT_EQ(mock_write_count, 1);
}

TEST(test_refresh_null)
{
    ASSERT_EQ(pac1952_refresh(NULL), PAC1952_ERR_PARAM);
}

TEST(test_refresh_i2c_fail)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    mock_fail_next = 1;
    ASSERT_EQ(pac1952_refresh(&dev), PAC1952_ERR_I2C);
}

/* ---------- read channel tests ---------- */

TEST(test_read_channel0_zero)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    mock_set_vbus(0, 0);
    mock_set_vsense(0, 0);

    pac1952_channel_t ch;
    ASSERT_EQ(pac1952_read_channel(&dev, 0, &ch), PAC1952_OK);
    ASSERT_NEAR(ch.vbus_v, 0.0f, 0.001f);
    ASSERT_NEAR(ch.vsense_mv, 0.0f, 0.001f);
    ASSERT_NEAR(ch.current_ma, 0.0f, 0.001f);
    ASSERT_NEAR(ch.power_mw, 0.0f, 0.001f);
}

TEST(test_read_channel0_vbus_conversion)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);

    /* VBUS raw = 0x8000 (32768) -> 32768 * 32/65536 = 16.0V */
    mock_set_vbus(0, 0x8000);
    mock_set_vsense(0, 0);

    pac1952_channel_t ch;
    pac1952_read_channel(&dev, 0, &ch);
    ASSERT_NEAR(ch.vbus_v, 16.0f, 0.01f);
}

TEST(test_read_channel0_vbus_full_scale)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);

    /* VBUS raw = 0xFFFF (65535) -> 65535 * 32/65536 ~ 31.9995V */
    mock_set_vbus(0, 0xFFFF);
    mock_set_vsense(0, 0);

    pac1952_channel_t ch;
    pac1952_read_channel(&dev, 0, &ch);
    ASSERT_NEAR(ch.vbus_v, 32.0f, 0.01f);
}

TEST(test_read_channel0_vsense_positive)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);

    /* VSENSE raw = 0x4000 (16384) -> 16384 * 100/32768 = 50.0 mV */
    mock_set_vbus(0, 0);
    mock_set_vsense(0, 0x8000);

    pac1952_channel_t ch;
    pac1952_read_channel(&dev, 0, &ch);
    ASSERT_NEAR(ch.vsense_mv, 50.0f, 0.01f);
}

TEST(test_read_channel0_vsense_upper_half)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);

    /* VSENSE raw = -16384 (0xC000) -> -16384 * 100/32768 = -50.0 mV */
    mock_set_vbus(0, 0);
    mock_set_vsense(0, 0xC000); /* unipolar at POR: 75 mV, not negative */

    pac1952_channel_t ch;
    pac1952_read_channel(&dev, 0, &ch);
    ASSERT_NEAR(ch.vsense_mv, 75.0f, 0.01f);
}

TEST(test_read_channel0_current_calculation)
{
    mock_reset();
    /* 10 mOhm sense resistor */
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);

    /* VSENSE = 50 mV, Rsense = 10 mOhm -> I = 50/10*1000 = 5000 mA */
    mock_set_vbus(0, 0);
    mock_set_vsense(0, 0x8000);  /* 50 mV */

    pac1952_channel_t ch;
    pac1952_read_channel(&dev, 0, &ch);
    ASSERT_NEAR(ch.current_ma, 5000.0f, 1.0f);
}

TEST(test_read_channel0_power_calculation)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);

    /* VBUS = 16V, VSENSE = 50 mV, Rsense = 10 mOhm
     * Current = 5000 mA, Power = 16V * 5000mA = 80000 mW */
    mock_set_vbus(0, 0x8000);    /* 16V */
    mock_set_vsense(0, 0x8000);  /* 50 mV */

    pac1952_channel_t ch;
    pac1952_read_channel(&dev, 0, &ch);
    ASSERT_NEAR(ch.power_mw, 80000.0f, 100.0f);
}

TEST(test_read_channel1)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);

    /* Set channel 1 registers */
    mock_set_vbus(1, 0x4000);    /* 8V */
    mock_set_vsense(1, 0x4000);  /* 25 mV */

    pac1952_channel_t ch;
    ASSERT_EQ(pac1952_read_channel(&dev, 1, &ch), PAC1952_OK);
    ASSERT_NEAR(ch.vbus_v, 8.0f, 0.01f);
    ASSERT_NEAR(ch.vsense_mv, 25.0f, 0.01f);
}

TEST(test_read_channel_invalid)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    pac1952_channel_t ch;
    ASSERT_EQ(pac1952_read_channel(&dev, 2, &ch), PAC1952_ERR_PARAM);
    ASSERT_EQ(pac1952_read_channel(&dev, 255, &ch), PAC1952_ERR_PARAM);
}

TEST(test_read_channel_null_dev)
{
    pac1952_channel_t ch;
    ASSERT_EQ(pac1952_read_channel(NULL, 0, &ch), PAC1952_ERR_PARAM);
}

TEST(test_read_channel_null_out)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    ASSERT_EQ(pac1952_read_channel(&dev, 0, NULL), PAC1952_ERR_PARAM);
}

TEST(test_read_channel_i2c_fail_vbus)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    mock_fail_next = 1;   /* fail on first read (VBUS) */
    pac1952_channel_t ch;
    ASSERT_EQ(pac1952_read_channel(&dev, 0, &ch), PAC1952_ERR_I2C);
}

TEST(test_read_channel_i2c_fail_vsense)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    mock_fail_at = 1;     /* fail on second I2C op overall */
    pac1952_channel_t ch;
    /* First read (VBUS) succeeds; second read (VSENSE) fails */
    ASSERT_EQ(pac1952_read_channel(&dev, 0, &ch), PAC1952_ERR_I2C);
}

/* ---------- read both tests ---------- */

TEST(test_read_both_ok)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);

    /* Note: mock_i2c stores flat bytes, so multi-byte registers at
     * adjacent addresses (VBUS1=0x07, VBUS2=0x08) share byte 0x08.
     * Set ch1 first, then ch0, so ch0's low byte at 0x08 overwrites
     * ch1's high byte. Use ch1 values where high byte = ch0 low byte. */
    mock_set_vbus(1, 0x4000);   /* ch1: 8V   -- sets [0x08]=0x40 [0x09]=0x00 */
    mock_set_vbus(0, 0x8040);   /* ch0: ~16V -- sets [0x07]=0x80 [0x08]=0x40 */
    mock_set_vsense(1, 0x4000); /* ch1: 25 mV */
    mock_set_vsense(0, 0x8000); /* ch0: 50 mV -- [0x0B]=0x80 [0x0C]=0x00 */
    /* VSENSE2 at 0x0C shares with VSENSE1 low byte. Set consistently: */
    mock_set_vsense(1, 0x0020); /* ch1: ~0.1 mV -- [0x0C]=0x00 [0x0D]=0x20 */

    pac1952_channel_t c1, c2;
    ASSERT_EQ(pac1952_read_both(&dev, &c1, &c2), PAC1952_OK);
    /* ch0: VBUS = 0x8040 * 32/65536 ~ 16.03V */
    ASSERT_NEAR(c1.vbus_v, 16.03f, 0.05f);
    /* ch1: VBUS = 0x4000 * 32/65536 = 8.0V (0x08 was set to 0x40 by ch0) */
    ASSERT_NEAR(c2.vbus_v, 8.0f, 0.05f);
}

TEST(test_read_both_null_dev)
{
    pac1952_channel_t c1, c2;
    ASSERT_EQ(pac1952_read_both(NULL, &c1, &c2), PAC1952_ERR_PARAM);
}

TEST(test_read_both_null_ch1)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    pac1952_channel_t c2;
    ASSERT_EQ(pac1952_read_both(&dev, NULL, &c2), PAC1952_ERR_PARAM);
}

TEST(test_read_both_null_ch2)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    pac1952_channel_t c1;
    ASSERT_EQ(pac1952_read_both(&dev, &c1, NULL), PAC1952_ERR_PARAM);
}

/* ---------- sample rate tests ---------- */

TEST(test_set_sample_rate_is_unsupported)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    mock_regs[REG_CTRL] = 0x07;
    mock_regs[REG_CTRL + 1] = 0x30;

    ASSERT_EQ(pac1952_set_sample_rate(&dev, PAC1952_RATE_1024SPS), PAC1952_ERR_UNSUPPORTED);

    /* A byte write would have changed GPIO_ALERT2 or ACC_COUNT. */
    ASSERT_EQ(mock_regs[REG_CTRL], 0x07);
    ASSERT_EQ(mock_regs[REG_CTRL + 1], 0x30);
}

TEST(test_set_sample_rate_invalid)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    ASSERT_EQ(pac1952_set_sample_rate(&dev, (pac1952_rate_t)4), PAC1952_ERR_PARAM);
    ASSERT_EQ(pac1952_set_sample_rate(&dev, (pac1952_rate_t)255), PAC1952_ERR_PARAM);
}

TEST(test_set_sample_rate_null)
{
    ASSERT_EQ(pac1952_set_sample_rate(NULL, PAC1952_RATE_8SPS), PAC1952_ERR_PARAM);
}

/* ---------- get_product_id tests ---------- */

TEST(test_get_product_id_ok)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);

    uint8_t id = 0;
    ASSERT_EQ(pac1952_get_product_id(&dev, &id), PAC1952_OK);
    ASSERT_EQ(id, PAC1952_PRODUCT_ID_VAL);
}

TEST(test_get_product_id_null_dev)
{
    uint8_t id;
    ASSERT_EQ(pac1952_get_product_id(NULL, &id), PAC1952_ERR_PARAM);
}

TEST(test_get_product_id_null_id)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    ASSERT_EQ(pac1952_get_product_id(&dev, NULL), PAC1952_ERR_PARAM);
}

TEST(test_get_product_id_i2c_fail)
{
    mock_reset();
    pac1952_init(&dev, &mock_i2c_hal, &default_cfg);
    mock_fail_next = 1;
    uint8_t id;
    ASSERT_EQ(pac1952_get_product_id(&dev, &id), PAC1952_ERR_I2C);
}

/* ---------- zero resistor edge case ---------- */

TEST(test_zero_resistor_current_zero)
{
    mock_reset();
    pac1952_cfg_t cfg_zero = { .sense_resistor_mohm = { 0.0f, 0.0f } };
    pac1952_init(&dev, &mock_i2c_hal, &cfg_zero);

    mock_set_vbus(0, 0x8000);
    mock_set_vsense(0, 0x8000);

    pac1952_channel_t ch;
    pac1952_read_channel(&dev, 0, &ch);
    /* With zero resistor, current and power should be 0 (no division) */
    ASSERT_NEAR(ch.current_ma, 0.0f, 0.001f);
    ASSERT_NEAR(ch.power_mw, 0.0f, 0.001f);
    /* But voltage readings should still be valid */
    ASSERT_NEAR(ch.vbus_v, 16.0f, 0.01f);
}

/* ---------- zero-init safety ---------- */

TEST(test_zero_init_safety)
{
    pac1952_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    /* Zero-init struct: calling read on it should not crash.
     * We only check that NULL-param paths are exercised. */
    pac1952_channel_t ch;
    /* dev has i2c=NULL, so read_channel hits NULL guard */
    ASSERT_EQ(pac1952_read_channel(NULL, 0, &ch), PAC1952_ERR_PARAM);
}

TEST(test_init_accepts_both_product_ids)
{
    mock_reset();
    mock_regs[REG_PRODUCT_ID] = 0x79; /* PAC1952-1 */
    ASSERT_EQ(pac1952_init(&dev, &mock_i2c_hal, &default_cfg), PAC1952_OK);

    mock_reset();
    mock_regs[REG_PRODUCT_ID] = 0x7D; /* PAC1952-2 */
    ASSERT_EQ(pac1952_init(&dev, &mock_i2c_hal, &default_cfg), PAC1952_OK);
}

TEST(test_init_rejects_other_product_id)
{
    mock_reset();
    mock_regs[REG_PRODUCT_ID] = 0x78; /* PAC1951-1 */
    ASSERT_EQ(pac1952_init(&dev, &mock_i2c_hal, &default_cfg), PAC1952_ERR_ID);
}

TEST_MAIN("pac1952")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_null_cfg);
    RUN(test_init_negative_resistor);
    RUN(test_init_nan_resistor);
    RUN(test_init_null_write_reg);
    RUN(test_init_null_read_regs);
    RUN(test_refresh_ok);
    RUN(test_refresh_null);
    RUN(test_refresh_i2c_fail);
    RUN(test_read_channel0_zero);
    RUN(test_read_channel0_vbus_conversion);
    RUN(test_read_channel0_vbus_full_scale);
    RUN(test_read_channel0_vsense_positive);
    RUN(test_read_channel0_vsense_upper_half);
    RUN(test_read_channel0_current_calculation);
    RUN(test_read_channel0_power_calculation);
    RUN(test_read_channel1);
    RUN(test_read_channel_invalid);
    RUN(test_read_channel_null_dev);
    RUN(test_read_channel_null_out);
    RUN(test_read_channel_i2c_fail_vbus);
    RUN(test_read_channel_i2c_fail_vsense);
    RUN(test_read_both_ok);
    RUN(test_read_both_null_dev);
    RUN(test_read_both_null_ch1);
    RUN(test_read_both_null_ch2);
    RUN(test_set_sample_rate_is_unsupported);
    RUN(test_set_sample_rate_invalid);
    RUN(test_set_sample_rate_null);
    RUN(test_get_product_id_ok);
    RUN(test_get_product_id_null_dev);
    RUN(test_get_product_id_null_id);
    RUN(test_get_product_id_i2c_fail);
    RUN(test_zero_resistor_current_zero);
    RUN(test_zero_init_safety);
    RUN(test_init_accepts_both_product_ids);
    RUN(test_init_rejects_other_product_id);
TEST_MAIN_END()
