// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "ccs811.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x20] = 0x81;  /* HW_ID */
    mock_regs[0x00] = 0x90;  /* STATUS: APP_VALID(0x10) | FW_MODE(0x80) */
}

static const ccs811_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static ccs811_t dev;
static ccs811_cfg_t default_cfg = {
    .addr = CCS811_ADDR_PRIMARY,
    .mode = CCS811_MODE_1SEC,
};

static void setup(void)
{
    mock_reset();
    ccs811_init(&dev, &mock_i2c, &default_cfg);
}

/* Plant measurement data in ALG_RESULT registers */
static void plant_data(uint16_t eco2, uint16_t tvoc)
{
    mock_regs[0x02] = (uint8_t)(eco2 >> 8);
    mock_regs[0x03] = (uint8_t)(eco2 & 0xFF);
    mock_regs[0x04] = (uint8_t)(tvoc >> 8);
    mock_regs[0x05] = (uint8_t)(tvoc & 0xFF);
    /* Set DATA_READY bit in STATUS */
    mock_regs[0x00] |= 0x08;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x20] = 0x00;
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_ERR_I2C);
}

TEST(test_init_app_not_valid)
{
    mock_reset();
    mock_regs[0x00] = 0x80;  /* FW_MODE but no APP_VALID */
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_ERR_BOOT);
}

TEST(test_init_fw_mode_not_set)
{
    mock_reset();
    /* First STATUS read: APP_VALID set */
    /* After APP_START write, the mock regs are modified */
    /* We simulate FW_MODE not set after APP_START by clearing it */
    mock_regs[0x00] = 0x10;  /* APP_VALID but no FW_MODE */
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_ERR_BOOT);
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
    ASSERT_EQ(ccs811_set_mode(NULL, CCS811_MODE_1SEC), CCS811_ERR_PARAM);
    ASSERT_EQ(ccs811_set_env_data(NULL, 0, 0), CCS811_ERR_PARAM);
}

TEST(test_read_ok)
{
    setup();
    plant_data(450, 10);
    ccs811_data_t data;
    ccs811_err_t e = ccs811_read(&dev, &data);
    ASSERT_EQ(e, CCS811_OK);
    ASSERT_EQ(data.eco2_ppm, 450);
    ASSERT_EQ(data.tvoc_ppb, 10);
}

TEST(test_read_not_ready)
{
    setup();
    /* DATA_READY bit is NOT set */
    mock_regs[0x00] = 0x90;  /* FW_MODE | APP_VALID, but no DATA_READY */
    ccs811_data_t data;
    ccs811_err_t e = ccs811_read(&dev, &data);
    ASSERT_EQ(e, CCS811_ERR_NOT_READY);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ccs811_data_t data;
    ccs811_err_t e = ccs811_read(&dev, &data);
    ASSERT_EQ(e, CCS811_ERR_I2C);
}

TEST(test_set_mode)
{
    setup();
    ccs811_err_t e = ccs811_set_mode(&dev, CCS811_MODE_10SEC);
    ASSERT_EQ(e, CCS811_OK);
    /* MEAS_MODE: mode 2 shifted left 4 = 0x20 */
    ASSERT_EQ(mock_regs[0x01], 0x20);
}

TEST(test_set_mode_invalid)
{
    setup();
    ccs811_err_t e = ccs811_set_mode(&dev, (ccs811_drive_mode_t)99);
    ASSERT_EQ(e, CCS811_ERR_PARAM);
}

TEST(test_set_mode_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ccs811_err_t e = ccs811_set_mode(&dev, CCS811_MODE_60SEC);
    ASSERT_EQ(e, CCS811_ERR_I2C);
}

TEST(test_meas_mode_register)
{
    setup();
    /* After init with MODE_1SEC, MEAS_MODE should be (1 << 4) = 0x10 */
    ASSERT_EQ(mock_regs[0x01], 0x10);
}

TEST(test_addr_alt)
{
    mock_reset();
    ccs811_cfg_t cfg = {
        .addr = CCS811_ADDR_ALT,
        .mode = CCS811_MODE_1SEC,
    };
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, CCS811_OK);
}

TEST(test_init_invalid_mode)
{
    mock_reset();
    ccs811_cfg_t cfg = {
        .addr = CCS811_ADDR_PRIMARY,
        .mode = (ccs811_drive_mode_t)99,
    };
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, CCS811_ERR_PARAM);
}

TEST(test_read_high_values)
{
    setup();
    plant_data(8192, 1187);
    ccs811_data_t data;
    ccs811_err_t e = ccs811_read(&dev, &data);
    ASSERT_EQ(e, CCS811_OK);
    ASSERT_EQ(data.eco2_ppm, 8192);
    ASSERT_EQ(data.tvoc_ppb, 1187);
}

TEST_MAIN("ccs811")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_init_app_not_valid);
    RUN(test_init_fw_mode_not_set);
    RUN(test_null_safety);
    RUN(test_read_ok);
    RUN(test_read_not_ready);
    RUN(test_read_i2c_fail);
    RUN(test_set_mode);
    RUN(test_set_mode_invalid);
    RUN(test_set_mode_i2c_fail);
    RUN(test_meas_mode_register);
    RUN(test_addr_alt);
    RUN(test_init_invalid_mode);
    RUN(test_read_high_values);
TEST_MAIN_END()
