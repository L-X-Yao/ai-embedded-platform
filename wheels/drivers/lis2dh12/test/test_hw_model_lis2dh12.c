// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_lis2dh12.h"
#include "lis2dh12.h"

static const lis2dh12_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

static const lis2dh12_cfg_t cfg = {
    .addr  = LIS2DH12_ADDR_HIGH,
    .range = LIS2DH12_RANGE_2G,
    .odr   = LIS2DH12_ODR_100HZ,
};

static lis2dh12_t dev;

TEST(test_hw_init_por)
{
    hw_model_reset();
    ASSERT_EQ(lis2dh12_init(&dev, &hw_i2c, &cfg), LIS2DH12_OK);
}

TEST(test_hw_wrong_id)
{
    hw_model_reset();
    hw_regs[0x0F] = 0x00;
    ASSERT_EQ(lis2dh12_init(&dev, &hw_i2c, &cfg), LIS2DH12_ERR_ID);
}

TEST(test_hw_roundtrip_1g)
{
    hw_model_reset();
    lis2dh12_init(&dev, &hw_i2c, &cfg);
    hw_model_inject_accel(0.0f, 0.0f, 1000.0f, 4.0f);
    lis2dh12_data_t data;
    lis2dh12_read(&dev, &data);
    ASSERT_NEAR(data.z_mg, 1000.0f, 8.0f);
}

TEST(test_hw_ctrl_regs_written)
{
    hw_model_reset();
    lis2dh12_init(&dev, &hw_i2c, &cfg);
    /* Tables 30 and 31, p.34 and p.35: ODR 0101 is 100 Hz, LPen 0, Z, Y, X on */
    ASSERT_EQ_HEX(hw_regs[0x20], 0x57);
    /* Table 38, p.37: BDU 1, FS 00 is +/-2 g, HR 0 */
    ASSERT_EQ_HEX(hw_regs[0x23], 0x80);
}

/* p.31, Table 21: the defaults */
TEST(test_hw_model_defaults)
{
    hw_model_reset();
    ASSERT_EQ_HEX(hw_regs[0x0F], 0x33);
    ASSERT_EQ_HEX(hw_regs[0x1E], 0x10);
    ASSERT_EQ_HEX(hw_regs[0x1F], 0x00);
    ASSERT_EQ_HEX(hw_regs[0x20], 0x07);
    ASSERT_EQ_HEX(hw_regs[0x21], 0x00);
    ASSERT_EQ_HEX(hw_regs[0x23], 0x00);
}

/* p.25: "the 7 LSb represent the actual register address while the MSb enables
 * address auto increment. If the MSb of the SUB field is '1', the SUB (register
 * address) is automatically increased to allow multiple data read/writes." */
TEST(test_hw_model_sub_msb_decides_the_increment)
{
    uint8_t buf[6];

    hw_model_reset();
    hw_regs[0x28] = 0x11;
    hw_regs[0x29] = 0x22;
    hw_regs[0x2A] = 0x33;

    ASSERT_TRUE(hw_model_read_regs(NULL, LIS2DH12_ADDR_HIGH, 0x28, buf, 3));
    ASSERT_EQ_HEX(buf[0], 0x11);
    ASSERT_EQ_HEX(buf[1], 0x11);
    ASSERT_EQ_HEX(buf[2], 0x11);

    ASSERT_TRUE(hw_model_read_regs(NULL, LIS2DH12_ADDR_HIGH, 0xA8, buf, 3));
    ASSERT_EQ_HEX(buf[0], 0x11);
    ASSERT_EQ_HEX(buf[1], 0x22);
    ASSERT_EQ_HEX(buf[2], 0x33);
}

/* A driver that lost the 0x80 would read OUT_X_L six times */
TEST(test_hw_read_gets_all_three_axes)
{
    hw_model_reset();
    ASSERT_EQ(lis2dh12_init(&dev, &hw_i2c, &cfg), LIS2DH12_OK);
    hw_model_inject_accel(100.0f, -200.0f, 1000.0f, 4.0f);

    lis2dh12_data_t data;
    ASSERT_EQ(lis2dh12_read(&dev, &data), LIS2DH12_OK);
    ASSERT_NEAR(data.x_mg, 100.0f, 4.0f);
    ASSERT_NEAR(data.y_mg, -200.0f, 4.0f);
    ASSERT_NEAR(data.z_mg, 1000.0f, 4.0f);
}

/* p.31: type r. p.32: "Registers marked as Reserved or not listed in the table
 * above must not be changed. Writing to those registers may cause permanent
 * damage to the device." */
TEST(test_hw_model_read_only_and_reserved)
{
    hw_model_reset();
    ASSERT_TRUE(hw_model_write_reg(NULL, LIS2DH12_ADDR_HIGH, 0x0F, 0x00));
    ASSERT_TRUE(hw_model_write_reg(NULL, LIS2DH12_ADDR_HIGH, 0x28, 0x55));
    ASSERT_EQ_HEX(hw_regs[0x0F], 0x33);
    ASSERT_EQ_HEX(hw_regs[0x28], 0x00);
    ASSERT_EQ(hw_reserved_writes, 0);

    ASSERT_TRUE(hw_model_write_reg(NULL, LIS2DH12_ADDR_HIGH, 0x10, 0x55));
    ASSERT_EQ(hw_reserved_writes, 1);
}

/* The driver keeps to the registers it may write, whatever it is told */
TEST(test_hw_driver_writes_no_reserved_register)
{
    hw_model_reset();
    ASSERT_EQ(lis2dh12_init(&dev, &hw_i2c, &cfg), LIS2DH12_OK);
    ASSERT_EQ(hw_reserved_writes, 0);
    ASSERT_EQ_HEX(hw_regs[0x1E], 0x10);   /* CTRL_REG0 stays at its default */
}

TEST_MAIN("lis2dh12_hw_model")
    RUN(test_hw_init_por);
    RUN(test_hw_wrong_id);
    RUN(test_hw_roundtrip_1g);
    RUN(test_hw_ctrl_regs_written);
    RUN(test_hw_model_defaults);
    RUN(test_hw_model_sub_msb_decides_the_increment);
    RUN(test_hw_read_gets_all_three_axes);
    RUN(test_hw_model_read_only_and_reserved);
    RUN(test_hw_driver_writes_no_reserved_register);
TEST_MAIN_END()
