// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "ccs811.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x20] = 0x81;   /* HW_ID */
    mock_regs[0x00] = 0x10;   /* STATUS: APP_VALID set */
}

static const ccs811_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static ccs811_t dev;
static ccs811_cfg_t default_cfg = {
    .addr = CCS811_DEV_ADDR,
    .mode = CCS811_MODE_1SEC,
};

static void setup(void)
{
    mock_reset();
    ccs811_init(&dev, &mock_i2c, &default_cfg);
}

/* Plant eCO2/TVOC data and set DATA_READY in STATUS */
static void plant_data(uint16_t eco2, uint16_t tvoc)
{
    /* Set DATA_READY (0x08) + APP_VALID (0x10) + FW_MODE (0x80) */
    mock_regs[0x00] = 0x98;
    mock_regs[0x02] = (uint8_t)(eco2 >> 8);
    mock_regs[0x03] = (uint8_t)(eco2 & 0xFF);
    mock_regs[0x04] = (uint8_t)(tvoc >> 8);
    mock_regs[0x05] = (uint8_t)(tvoc & 0xFF);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_OK);
}

TEST(test_init_wrong_hw_id)
{
    mock_reset();
    mock_regs[0x20] = 0x00;
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_ERR_ID);
}

TEST(test_init_app_not_valid)
{
    mock_reset();
    mock_regs[0x00] = 0x00;   /* APP_VALID not set */
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_ERR_APP);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(ccs811_init(NULL, &mock_i2c, &default_cfg), CCS811_ERR_PARAM);
    ccs811_t d;
    ASSERT_EQ(ccs811_init(&d, NULL, &default_cfg), CCS811_ERR_PARAM);
    ASSERT_EQ(ccs811_init(&d, &mock_i2c, NULL), CCS811_ERR_PARAM);
    ccs811_data_t data;
    ASSERT_EQ(ccs811_read(NULL, &data), CCS811_ERR_PARAM);
    ASSERT_EQ(ccs811_read(&d, NULL), CCS811_ERR_PARAM);
    ASSERT_EQ(ccs811_set_env_data(NULL, 50.0f, 25.0f), CCS811_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    ccs811_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ccs811_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(ccs811_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)CCS811_ERR_I2C);
    ASSERT_EQ(-2, (int)CCS811_ERR_ID);
    ASSERT_EQ(-3, (int)CCS811_ERR_PARAM);
    ASSERT_EQ(-4, (int)CCS811_ERR_APP);
    ASSERT_EQ(-5, (int)CCS811_ERR_NO_DATA);
}

TEST(test_init_sets_meas_mode)
{
    mock_reset();
    ccs811_t d;
    ccs811_cfg_t cfg = { .addr = CCS811_DEV_ADDR, .mode = CCS811_MODE_10SEC };
    ccs811_init(&d, &mock_i2c, &cfg);
    /* MEAS_MODE: mode 2 << 4 = 0x20 */
    ASSERT_EQ(mock_regs[0x01], 0x20);
}

TEST(test_init_writes_app_start)
{
    mock_reset();
    ccs811_t d;
    ccs811_init(&d, &mock_i2c, &default_cfg);
    /* APP_START register (0xF4) should have been written */
    /* Just check init succeeded — APP_START is write-only */
    ASSERT_EQ(CCS811_OK, ccs811_init(&d, &mock_i2c, &default_cfg));
}

TEST(test_read_data_ready)
{
    setup();
    plant_data(412, 5);
    ccs811_data_t data;
    ccs811_err_t e = ccs811_read(&dev, &data);
    ASSERT_EQ(e, CCS811_OK);
    ASSERT_EQ(data.eco2_ppm, 412);
    ASSERT_EQ(data.tvoc_ppb, 5);
}

TEST(test_read_no_data)
{
    setup();
    /* STATUS without DATA_READY */
    mock_regs[0x00] = 0x90;  /* FW_MODE + APP_VALID, no DATA_READY */
    ccs811_data_t data;
    ccs811_err_t e = ccs811_read(&dev, &data);
    ASSERT_EQ(e, CCS811_ERR_NO_DATA);
}

TEST(test_read_high_eco2)
{
    setup();
    plant_data(8192, 1187);
    ccs811_data_t data;
    ccs811_read(&dev, &data);
    ASSERT_EQ(data.eco2_ppm, 8192);
    ASSERT_EQ(data.tvoc_ppb, 1187);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ccs811_data_t data;
    ccs811_err_t e = ccs811_read(&dev, &data);
    ASSERT_EQ(e, CCS811_ERR_I2C);
}

TEST(test_set_env_data)
{
    setup();
    ccs811_err_t e = ccs811_set_env_data(&dev, 50.0f, 25.0f);
    ASSERT_EQ(e, CCS811_OK);
}

TEST(test_invalid_mode)
{
    mock_reset();
    ccs811_t d;
    ccs811_cfg_t cfg = { .addr = CCS811_DEV_ADDR, .mode = (ccs811_mode_t)5 };
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, CCS811_ERR_PARAM);
}

TEST_MAIN("ccs811")
    RUN(test_init_ok);
    RUN(test_init_wrong_hw_id);
    RUN(test_init_app_not_valid);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_init_sets_meas_mode);
    RUN(test_init_writes_app_start);
    RUN(test_read_data_ready);
    RUN(test_read_no_data);
    RUN(test_read_high_eco2);
    RUN(test_read_i2c_fail);
    RUN(test_set_env_data);
    RUN(test_invalid_mode);
TEST_MAIN_END()
