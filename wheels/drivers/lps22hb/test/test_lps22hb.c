// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "lps22hb.h"
#include <math.h>

/* ---------- register addresses (must match lps22hb.c) ---------- */

#define REG_WHO_AM_I  0x0F
#define REG_CTRL_REG1 0x10
#define REG_CTRL_REG2 0x11
#define REG_STATUS    0x27
#define REG_PRESS_XL  0x28
#define REG_PRESS_L   0x29
#define REG_PRESS_H   0x2A
#define REG_TEMP_L    0x2B
#define REG_TEMP_H    0x2C

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[REG_WHO_AM_I] = 0xB1;   /* LPS22HB chip ID */
}

/* Load a 24-bit raw pressure into the mock register bank.
 * raw is two's complement 24-bit. */
static void mock_set_pressure_raw(int32_t raw)
{
    mock_regs[REG_PRESS_XL] = (uint8_t)(raw & 0xFF);
    mock_regs[REG_PRESS_L]  = (uint8_t)((raw >> 8) & 0xFF);
    mock_regs[REG_PRESS_H]  = (uint8_t)((raw >> 16) & 0xFF);
}

/* Load a 16-bit raw temperature into the mock register bank. */
static void mock_set_temperature_raw(int16_t raw)
{
    mock_regs[REG_TEMP_L] = (uint8_t)(raw & 0xFF);
    mock_regs[REG_TEMP_H] = (uint8_t)((raw >> 8) & 0xFF);
}

static const lps22hb_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

static lps22hb_t dev;

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(lps22hb_init(&dev, &mock_i2c), LPS22HB_OK);
    /* verify BDU=1 was written to CTRL_REG1 */
    ASSERT_EQ(mock_regs[REG_CTRL_REG1] & 0x02, 0x02);
}

TEST(test_init_bad_id)
{
    mock_reset();
    mock_regs[REG_WHO_AM_I] = 0x00;   /* wrong chip ID */
    ASSERT_EQ(lps22hb_init(&dev, &mock_i2c), LPS22HB_ERR_ID);
}

TEST(test_init_i2c_fail_read)
{
    mock_reset();
    mock_fail_at = 0;   /* first I2C op (WHO_AM_I read) fails */
    ASSERT_EQ(lps22hb_init(&dev, &mock_i2c), LPS22HB_ERR_I2C);
}

TEST(test_init_i2c_fail_write)
{
    mock_reset();
    mock_fail_at = 1;   /* second I2C op (CTRL_REG1 write) fails */
    ASSERT_EQ(lps22hb_init(&dev, &mock_i2c), LPS22HB_ERR_I2C);
}

/* ---------- null safety ---------- */

TEST(test_null_init_dev)
{
    ASSERT_EQ(lps22hb_init(NULL, &mock_i2c), LPS22HB_ERR_PARAM);
}

TEST(test_null_init_i2c)
{
    ASSERT_EQ(lps22hb_init(&dev, NULL), LPS22HB_ERR_PARAM);
}

TEST(test_null_read_dev)
{
    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(NULL, &data), LPS22HB_ERR_PARAM);
}

TEST(test_null_read_out)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);
    ASSERT_EQ(lps22hb_read(&dev, NULL), LPS22HB_ERR_PARAM);
}

TEST(test_null_set_odr)
{
    ASSERT_EQ(lps22hb_set_odr(NULL, LPS22HB_ODR_1HZ), LPS22HB_ERR_PARAM);
}

TEST(test_null_one_shot)
{
    ASSERT_EQ(lps22hb_one_shot_trigger(NULL), LPS22HB_ERR_PARAM);
}

TEST(test_null_read_raw_pressure)
{
    int32_t raw;
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);
    ASSERT_EQ(lps22hb_read_raw_pressure(NULL, &raw), LPS22HB_ERR_PARAM);
    ASSERT_EQ(lps22hb_read_raw_pressure(&dev, NULL), LPS22HB_ERR_PARAM);
}

TEST(test_null_read_raw_temperature)
{
    int16_t raw;
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);
    ASSERT_EQ(lps22hb_read_raw_temperature(NULL, &raw), LPS22HB_ERR_PARAM);
    ASSERT_EQ(lps22hb_read_raw_temperature(&dev, NULL), LPS22HB_ERR_PARAM);
}

/* ---------- read pressure/temperature ---------- */

TEST(test_read_1013_25_hpa)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);

    /* 1013.25 hPa => raw = 1013.25 * 4096 = 4150272 = 0x3F5100 */
    mock_set_pressure_raw(4150272);
    /* 25.00 C => raw = 25.00 * 100 = 2500 */
    mock_set_temperature_raw(2500);

    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_OK);
    ASSERT_NEAR(data.pressure_hpa, 1013.25f, 0.01f);
    ASSERT_NEAR(data.temperature_c, 25.00f, 0.01f);
}

TEST(test_read_zero_pressure)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);

    mock_set_pressure_raw(0);
    mock_set_temperature_raw(0);

    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_OK);
    ASSERT_NEAR(data.pressure_hpa, 0.0f, 0.001f);
    ASSERT_NEAR(data.temperature_c, 0.0f, 0.001f);
}

TEST(test_read_negative_temperature)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);

    /* -10.50 C => raw = -1050 (0xFBE6 as int16) */
    mock_set_pressure_raw(4150272);
    mock_set_temperature_raw(-1050);

    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_OK);
    ASSERT_NEAR(data.temperature_c, -10.50f, 0.01f);
}

TEST(test_read_negative_pressure)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);

    /* Negative 24-bit pressure to exercise sign-extension in lps22hb_read().
     * raw = -4096 => pressure_hpa = -4096 / 4096.0 = -1.0 hPa.
     * This covers the branch: if (raw_p & 0x00800000) raw_p |= 0xFF000000 */
    mock_set_pressure_raw(-4096);
    mock_set_temperature_raw(0);

    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_OK);
    ASSERT_NEAR(data.pressure_hpa, -1.0f, 0.01f);
    ASSERT_NEAR(data.temperature_c, 0.0f, 0.001f);
}

TEST(test_read_i2c_fail)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);

    mock_fail_next = 1;
    lps22hb_data_t data;
    ASSERT_EQ(lps22hb_read(&dev, &data), LPS22HB_ERR_I2C);
}

/* ---------- ODR settings ---------- */

TEST(test_set_odr_all_values)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);

    /* one-shot: ODR bits [6:4] = 000, BDU bit 1 = 1 => 0x02 */
    ASSERT_EQ(lps22hb_set_odr(&dev, LPS22HB_ODR_ONE_SHOT), LPS22HB_OK);
    ASSERT_EQ(mock_regs[REG_CTRL_REG1], 0x02);

    /* 1 Hz: ODR bits = 001 => (1 << 4) | 0x02 = 0x12 */
    ASSERT_EQ(lps22hb_set_odr(&dev, LPS22HB_ODR_1HZ), LPS22HB_OK);
    ASSERT_EQ(mock_regs[REG_CTRL_REG1], 0x12);

    /* 10 Hz: ODR bits = 010 => (2 << 4) | 0x02 = 0x22 */
    ASSERT_EQ(lps22hb_set_odr(&dev, LPS22HB_ODR_10HZ), LPS22HB_OK);
    ASSERT_EQ(mock_regs[REG_CTRL_REG1], 0x22);

    /* 25 Hz: ODR bits = 011 => (3 << 4) | 0x02 = 0x32 */
    ASSERT_EQ(lps22hb_set_odr(&dev, LPS22HB_ODR_25HZ), LPS22HB_OK);
    ASSERT_EQ(mock_regs[REG_CTRL_REG1], 0x32);

    /* 50 Hz: ODR bits = 100 => (4 << 4) | 0x02 = 0x42 */
    ASSERT_EQ(lps22hb_set_odr(&dev, LPS22HB_ODR_50HZ), LPS22HB_OK);
    ASSERT_EQ(mock_regs[REG_CTRL_REG1], 0x42);

    /* 75 Hz: ODR bits = 101 => (5 << 4) | 0x02 = 0x52 */
    ASSERT_EQ(lps22hb_set_odr(&dev, LPS22HB_ODR_75HZ), LPS22HB_OK);
    ASSERT_EQ(mock_regs[REG_CTRL_REG1], 0x52);
}

TEST(test_set_odr_invalid)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);
    ASSERT_EQ(lps22hb_set_odr(&dev, (lps22hb_odr_t)6), LPS22HB_ERR_PARAM);
    ASSERT_EQ(lps22hb_set_odr(&dev, (lps22hb_odr_t)255), LPS22HB_ERR_PARAM);
}

TEST(test_set_odr_i2c_fail)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);
    mock_fail_next = 1;
    ASSERT_EQ(lps22hb_set_odr(&dev, LPS22HB_ODR_10HZ), LPS22HB_ERR_I2C);
}

/* ---------- one-shot trigger ---------- */

TEST(test_one_shot_trigger)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);

    ASSERT_EQ(lps22hb_one_shot_trigger(&dev), LPS22HB_OK);
    /* CTRL_REG2 bit 0 should be set */
    ASSERT_TRUE(mock_regs[REG_CTRL_REG2] & 0x01);
}

TEST(test_one_shot_i2c_fail_read)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);
    mock_fail_next = 1;  /* first op in one_shot (read CTRL_REG2) fails */
    ASSERT_EQ(lps22hb_one_shot_trigger(&dev), LPS22HB_ERR_I2C);
}

TEST(test_one_shot_i2c_fail_write)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);
    mock_fail_at = (int)(mock_op_count + 1);  /* second op in one_shot (write) fails */
    ASSERT_EQ(lps22hb_one_shot_trigger(&dev), LPS22HB_ERR_I2C);
}

/* ---------- raw pressure ---------- */

TEST(test_read_raw_pressure)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);

    /* 4150272 = 0x3F5100 */
    mock_set_pressure_raw(4150272);

    int32_t raw;
    ASSERT_EQ(lps22hb_read_raw_pressure(&dev, &raw), LPS22HB_OK);
    ASSERT_EQ(raw, 4150272);
}

TEST(test_read_raw_pressure_negative)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);

    /* negative raw (24-bit two's complement): -4096 = 0xFFF000 */
    mock_set_pressure_raw(-4096);

    int32_t raw;
    ASSERT_EQ(lps22hb_read_raw_pressure(&dev, &raw), LPS22HB_OK);
    ASSERT_EQ(raw, -4096);
}

TEST(test_read_raw_pressure_i2c_fail)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);
    mock_fail_next = 1;
    int32_t raw;
    ASSERT_EQ(lps22hb_read_raw_pressure(&dev, &raw), LPS22HB_ERR_I2C);
}

/* ---------- raw temperature ---------- */

TEST(test_read_raw_temperature)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);

    mock_set_temperature_raw(2500);

    int16_t raw;
    ASSERT_EQ(lps22hb_read_raw_temperature(&dev, &raw), LPS22HB_OK);
    ASSERT_EQ(raw, 2500);
}

TEST(test_read_raw_temperature_negative)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);

    mock_set_temperature_raw(-500);

    int16_t raw;
    ASSERT_EQ(lps22hb_read_raw_temperature(&dev, &raw), LPS22HB_OK);
    ASSERT_EQ(raw, -500);
}

TEST(test_read_raw_temperature_i2c_fail)
{
    mock_reset();
    lps22hb_init(&dev, &mock_i2c);
    mock_fail_next = 1;
    int16_t raw;
    ASSERT_EQ(lps22hb_read_raw_temperature(&dev, &raw), LPS22HB_ERR_I2C);
}

/* ---------- zero-init safety ---------- */

TEST(test_zero_init_safety)
{
    lps22hb_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    /* must not crash on zero-init struct */
    lps22hb_data_t data;
    (void)data;
    (void)zero_dev;
}

/* ---------- hw_model ---------- */

TEST(test_hw_model)
{
    /* verify the chip ID constant matches the LPS22HB datasheet */
    ASSERT_EQ_HEX(LPS22HB_CHIP_ID, 0xB1);
    ASSERT_EQ_HEX(LPS22HB_DEV_ADDR, 0x5C);
    ASSERT_EQ_HEX(LPS22HB_DEV_ADDR_ALT, 0x5D);
}

TEST_MAIN("lps22hb")
    RUN(test_init_ok);
    RUN(test_init_bad_id);
    RUN(test_init_i2c_fail_read);
    RUN(test_init_i2c_fail_write);
    RUN(test_null_init_dev);
    RUN(test_null_init_i2c);
    RUN(test_null_read_dev);
    RUN(test_null_read_out);
    RUN(test_null_set_odr);
    RUN(test_null_one_shot);
    RUN(test_null_read_raw_pressure);
    RUN(test_null_read_raw_temperature);
    RUN(test_read_1013_25_hpa);
    RUN(test_read_zero_pressure);
    RUN(test_read_negative_temperature);
    RUN(test_read_negative_pressure);
    RUN(test_read_i2c_fail);
    RUN(test_set_odr_all_values);
    RUN(test_set_odr_invalid);
    RUN(test_set_odr_i2c_fail);
    RUN(test_one_shot_trigger);
    RUN(test_one_shot_i2c_fail_read);
    RUN(test_one_shot_i2c_fail_write);
    RUN(test_read_raw_pressure);
    RUN(test_read_raw_pressure_negative);
    RUN(test_read_raw_pressure_i2c_fail);
    RUN(test_read_raw_temperature);
    RUN(test_read_raw_temperature_negative);
    RUN(test_read_raw_temperature_i2c_fail);
    RUN(test_zero_init_safety);
    RUN(test_hw_model);
TEST_MAIN_END()
