// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "as5600.h"

/* ---------- chip-specific mock setup ---------- */

#define REG_ZPOS_H    0x01u
#define REG_ZPOS_L    0x02u
#define REG_MANG_H    0x05u
#define REG_MANG_L    0x06u
#define REG_STATUS    0x0Bu
#define REG_RAW_ANG_H 0x0Cu
#define REG_RAW_ANG_L 0x0Du

static void mock_reset(void)
{
    mock_i2c_reset();
    /* Magnet detected, no errors */
    mock_regs[REG_STATUS] = AS5600_STATUS_MD;
    /* Default raw angle: 0 */
    mock_regs[REG_RAW_ANG_H] = 0x00;
    mock_regs[REG_RAW_ANG_L] = 0x00;
}

static const as5600_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static as5600_t dev;

static void setup(void)
{
    mock_reset();
    as5600_init(&dev, &mock_i2c);
}

/* Plant a 12-bit raw angle into mock registers */
static void plant_angle(uint16_t raw)
{
    mock_regs[REG_RAW_ANG_H] = (uint8_t)(raw >> 8);
    mock_regs[REG_RAW_ANG_L] = (uint8_t)(raw & 0xFF);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    as5600_t d;
    ASSERT_EQ(as5600_init(&d, &mock_i2c), AS5600_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    as5600_t d;
    ASSERT_EQ(as5600_init(&d, &mock_i2c), AS5600_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(as5600_init(NULL, &mock_i2c), AS5600_ERR_PARAM);
    ASSERT_EQ(as5600_init(&dev, NULL), AS5600_ERR_PARAM);
    uint16_t raw;
    ASSERT_EQ(as5600_read_raw_angle(NULL, &raw), AS5600_ERR_PARAM);
    ASSERT_EQ(as5600_read_raw_angle(&dev, NULL), AS5600_ERR_PARAM);
    float deg;
    ASSERT_EQ(as5600_read_degrees(NULL, &deg), AS5600_ERR_PARAM);
    ASSERT_EQ(as5600_read_degrees(&dev, NULL), AS5600_ERR_PARAM);
    uint8_t st;
    ASSERT_EQ(as5600_read_status(NULL, &st), AS5600_ERR_PARAM);
    ASSERT_EQ(as5600_set_start_position(NULL, 0), AS5600_ERR_PARAM);
    ASSERT_EQ(as5600_set_max_angle(NULL, 0), AS5600_ERR_PARAM);
}

TEST(test_read_raw_angle_zero)
{
    setup();
    plant_angle(0);
    uint16_t raw;
    ASSERT_EQ(as5600_read_raw_angle(&dev, &raw), AS5600_OK);
    ASSERT_EQ(raw, 0);
}

TEST(test_read_raw_angle_mid)
{
    setup();
    plant_angle(2048);  /* 180 degrees */
    uint16_t raw;
    ASSERT_EQ(as5600_read_raw_angle(&dev, &raw), AS5600_OK);
    ASSERT_EQ(raw, 2048);
}

TEST(test_read_raw_angle_max)
{
    setup();
    plant_angle(4095);  /* ~360 degrees */
    uint16_t raw;
    ASSERT_EQ(as5600_read_raw_angle(&dev, &raw), AS5600_OK);
    ASSERT_EQ(raw, 4095);
}

TEST(test_read_raw_angle_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    uint16_t raw;
    ASSERT_EQ(as5600_read_raw_angle(&dev, &raw), AS5600_ERR_I2C);
}

TEST(test_read_degrees_zero)
{
    setup();
    plant_angle(0);
    float deg;
    ASSERT_EQ(as5600_read_degrees(&dev, &deg), AS5600_OK);
    ASSERT_NEAR(deg, 0.0f, 0.1f);
}

TEST(test_read_degrees_180)
{
    setup();
    plant_angle(2048);
    float deg;
    ASSERT_EQ(as5600_read_degrees(&dev, &deg), AS5600_OK);
    ASSERT_NEAR(deg, 180.0f, 0.1f);
}

TEST(test_read_degrees_near_360)
{
    setup();
    plant_angle(4095);
    float deg;
    ASSERT_EQ(as5600_read_degrees(&dev, &deg), AS5600_OK);
    ASSERT_TRUE(deg > 359.0f && deg < 360.0f);
}

TEST(test_read_status)
{
    setup();
    mock_regs[REG_STATUS] = AS5600_STATUS_MD | AS5600_STATUS_MH;
    uint8_t st;
    ASSERT_EQ(as5600_read_status(&dev, &st), AS5600_OK);
    ASSERT_TRUE(st & AS5600_STATUS_MD);
    ASSERT_TRUE(st & AS5600_STATUS_MH);
    ASSERT_FALSE(st & AS5600_STATUS_ML);
}

TEST(test_read_status_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    uint8_t st;
    ASSERT_EQ(as5600_read_status(&dev, &st), AS5600_ERR_I2C);
}

TEST(test_set_start_position)
{
    setup();
    ASSERT_EQ(as5600_set_start_position(&dev, 1024), AS5600_OK);
    ASSERT_EQ(mock_regs[REG_ZPOS_H], 0x04);
    ASSERT_EQ(mock_regs[REG_ZPOS_L], 0x00);
}

TEST(test_set_start_position_overflow)
{
    setup();
    ASSERT_EQ(as5600_set_start_position(&dev, 4096), AS5600_ERR_PARAM);
    ASSERT_EQ(as5600_set_start_position(&dev, 0xFFFF), AS5600_ERR_PARAM);
}

TEST(test_set_start_position_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(as5600_set_start_position(&dev, 512), AS5600_ERR_I2C);
}

TEST(test_set_max_angle)
{
    setup();
    ASSERT_EQ(as5600_set_max_angle(&dev, 2048), AS5600_OK);
    ASSERT_EQ(mock_regs[REG_MANG_H], 0x08);
    ASSERT_EQ(mock_regs[REG_MANG_L], 0x00);
}

TEST(test_set_max_angle_overflow)
{
    setup();
    ASSERT_EQ(as5600_set_max_angle(&dev, 4096), AS5600_ERR_PARAM);
}

/* v1-06 p.21: "The angular range must be greater than 18 degrees." At 4096
 * counts per turn, 18 degrees is 204.8 counts: 204 is too small, 205 is not.
 * 0 is the unprogrammed default, which p.25 gives as the full 360 degrees. */
TEST(test_set_max_angle_minimum_range)
{
    setup();
    ASSERT_EQ(as5600_set_max_angle(&dev, 1), AS5600_ERR_PARAM);
    ASSERT_EQ(as5600_set_max_angle(&dev, 204), AS5600_ERR_PARAM);
    ASSERT_EQ(mock_write_count, 0);
    ASSERT_EQ(as5600_set_max_angle(&dev, 205), AS5600_OK);
    ASSERT_EQ(mock_regs[REG_MANG_L], 205);
    ASSERT_EQ(as5600_set_max_angle(&dev, 0), AS5600_OK);
    ASSERT_EQ(mock_regs[REG_MANG_L], 0);
}

TEST(test_set_max_angle_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(as5600_set_max_angle(&dev, 2048), AS5600_ERR_I2C);
}

/* v1-06 p.18 note 1: "Blank fields may contain factory settings", so bits 7:4
 * of the high registers must survive a write of the 12-bit value. */
TEST(test_set_start_position_keeps_blank_bits)
{
    setup();
    mock_regs[REG_ZPOS_H] = 0xAF;  /* old ZPOS(11:8) must go, blank bits stay */
    ASSERT_EQ(as5600_set_start_position(&dev, 0x0234), AS5600_OK);
    ASSERT_EQ(mock_regs[REG_ZPOS_H], 0xA2);
    ASSERT_EQ(mock_regs[REG_ZPOS_L], 0x34);
}

TEST(test_set_max_angle_keeps_blank_bits)
{
    setup();
    mock_regs[REG_MANG_H] = 0x50;
    ASSERT_EQ(as5600_set_max_angle(&dev, 0x0F56), AS5600_OK);
    ASSERT_EQ(mock_regs[REG_MANG_H], 0x5F);
    ASSERT_EQ(mock_regs[REG_MANG_L], 0x56);
}

TEST(test_set_12bit_failed_read_writes_nothing)
{
    setup();
    int writes = mock_write_count;
    mock_fail_next = 1;
    ASSERT_EQ(as5600_set_start_position(&dev, 512), AS5600_ERR_I2C);
    mock_fail_next = 1;
    ASSERT_EQ(as5600_set_max_angle(&dev, 2048), AS5600_ERR_I2C);
    ASSERT_EQ(mock_write_count, writes);
}

TEST(test_set_12bit_write_fail)
{
    setup();
    /* Read is op +0, high write +1, low write +2 */
    mock_fail_at = mock_op_count + 1;
    ASSERT_EQ(as5600_set_start_position(&dev, 512), AS5600_ERR_I2C);
    mock_fail_at = mock_op_count + 2;
    ASSERT_EQ(as5600_set_max_angle(&dev, 2048), AS5600_ERR_I2C);
    mock_fail_at = -1;
}

TEST(test_read_raw_angle_12bit_mask)
{
    setup();
    /* Write full 16-bit value with upper nibble set — must be masked to 12 bits */
    mock_regs[REG_RAW_ANG_H] = 0xFF;
    mock_regs[REG_RAW_ANG_L] = 0xFF;
    uint16_t raw;
    ASSERT_EQ(as5600_read_raw_angle(&dev, &raw), AS5600_OK);
    ASSERT_EQ(raw, 4095);
}

TEST_MAIN("as5600")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_raw_angle_zero);
    RUN(test_read_raw_angle_mid);
    RUN(test_read_raw_angle_max);
    RUN(test_read_raw_angle_i2c_fail);
    RUN(test_read_degrees_zero);
    RUN(test_read_degrees_180);
    RUN(test_read_degrees_near_360);
    RUN(test_read_status);
    RUN(test_read_status_i2c_fail);
    RUN(test_set_start_position);
    RUN(test_set_start_position_overflow);
    RUN(test_set_start_position_i2c_fail);
    RUN(test_set_max_angle);
    RUN(test_set_max_angle_overflow);
    RUN(test_set_max_angle_minimum_range);
    RUN(test_set_max_angle_i2c_fail);
    RUN(test_set_start_position_keeps_blank_bits);
    RUN(test_set_max_angle_keeps_blank_bits);
    RUN(test_set_12bit_failed_read_writes_nothing);
    RUN(test_set_12bit_write_fail);
    RUN(test_read_raw_angle_12bit_mask);
TEST_MAIN_END()
