// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include <math.h>
#include "mpu6050.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x75] = 0x68;
}

static const mpu6050_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

/* DEVICE_RESET as the chip does it. Registers go to their reset values, with
 * register 107 at 0x40 (manual p.8), and "The bit automatically clears to 0 once
 * the reset is done" (p.42), here after mock_reset_busy_reads more reads. */
static int mock_reset_busy_reads;

static bool chip_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
{
    bool ok = mock_i2c_write_reg(ctx, addr, reg, val);
    if (ok && reg == 0x6B && (val & 0x80)) {
        mock_regs[0x1B] = 0;
        mock_regs[0x1C] = 0;
    }
    return ok;
}

static bool chip_read_regs(void *ctx, uint8_t addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    if (reg == 0x6B && (mock_regs[0x6B] & 0x80)) {
        if (mock_reset_busy_reads == 0) {
            mock_regs[0x6B] = 0x40;
        } else {
            mock_reset_busy_reads--;
        }
    }
    return mock_i2c_read_regs(ctx, addr, reg, buf, len);
}

static const mpu6050_i2c_t chip_i2c = {
    .write_reg = chip_write_reg,
    .read_regs = chip_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static const mpu6050_i2c_t chip_i2c_no_delay = {
    .write_reg = chip_write_reg,
    .read_regs = chip_read_regs,
    .delay_ms  = NULL,
    .ctx       = NULL,
};

/* ---------- helpers ---------- */
static mpu6050_t dev;
static mpu6050_cfg_t default_cfg = {
    .addr = MPU6050_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_2G,
    .gyro_range = MPU6050_GYRO_250DPS,
};

static void setup(void)
{
    mock_reset();
    mpu6050_init(&dev, &mock_i2c, &default_cfg);
    mock_op_count = 0;
}

/* Ranges other than the reset value, so a lost restore shows */
static void setup_chip(const mpu6050_i2c_t *i2c)
{
    mock_reset();
    mock_reset_busy_reads = 0;
    mpu6050_cfg_t cfg = { .addr = MPU6050_ADDR_LOW,
                          .accel_range = MPU6050_ACCEL_8G,
                          .gyro_range = MPU6050_GYRO_1000DPS };
    mpu6050_init(&dev, i2c, &cfg);
    mock_op_count = 0;
    mock_delay_ms_count = 0;
}

static void plant_accel(int16_t x, int16_t y, int16_t z)
{
    mock_regs[0x3B] = (uint8_t)(x >> 8); mock_regs[0x3C] = (uint8_t)x;
    mock_regs[0x3D] = (uint8_t)(y >> 8); mock_regs[0x3E] = (uint8_t)y;
    mock_regs[0x3F] = (uint8_t)(z >> 8); mock_regs[0x40] = (uint8_t)z;
}

static void plant_gyro(int16_t x, int16_t y, int16_t z)
{
    mock_regs[0x43] = (uint8_t)(x >> 8); mock_regs[0x44] = (uint8_t)x;
    mock_regs[0x45] = (uint8_t)(y >> 8); mock_regs[0x46] = (uint8_t)y;
    mock_regs[0x47] = (uint8_t)(z >> 8); mock_regs[0x48] = (uint8_t)z;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    mpu6050_t d;
    ASSERT_EQ(mpu6050_init(&d, &mock_i2c, &default_cfg), MPU6050_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x75] = 0x00;
    mpu6050_t d;
    ASSERT_EQ(mpu6050_init(&d, &mock_i2c, &default_cfg), MPU6050_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    mpu6050_t d;
    ASSERT_EQ(mpu6050_init(&d, &mock_i2c, &default_cfg), MPU6050_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(mpu6050_init(NULL, &mock_i2c, &default_cfg), MPU6050_ERR_PARAM);
    mpu6050_t d;
    ASSERT_EQ(mpu6050_init(&d, NULL, &default_cfg), MPU6050_ERR_PARAM);
    ASSERT_EQ(mpu6050_init(&d, &mock_i2c, NULL), MPU6050_ERR_PARAM);
    mpu6050_accel_t a;
    ASSERT_EQ(mpu6050_read_accel(NULL, &a), MPU6050_ERR_PARAM);
    ASSERT_EQ(mpu6050_read_accel(&d, NULL), MPU6050_ERR_PARAM);
    mpu6050_gyro_t g;
    ASSERT_EQ(mpu6050_read_gyro(NULL, &g), MPU6050_ERR_PARAM);
    ASSERT_EQ(mpu6050_read_gyro(&d, NULL), MPU6050_ERR_PARAM);
    ASSERT_EQ(mpu6050_reset(NULL), MPU6050_ERR_PARAM);
}

TEST(test_read_accel_1g_z)
{
    setup();
    plant_accel(0, 0, 16384);

    mpu6050_accel_t a;
    ASSERT_EQ(mpu6050_read_accel(&dev, &a), MPU6050_OK);
    ASSERT_NEAR(a.ax_g, 0.0f, 0.01f);
    ASSERT_NEAR(a.ay_g, 0.0f, 0.01f);
    ASSERT_NEAR(a.az_g, 1.0f, 0.01f);
}

TEST(test_read_accel_negative)
{
    setup();
    plant_accel(-16384, 0, 0);

    mpu6050_accel_t a;
    mpu6050_read_accel(&dev, &a);
    ASSERT_NEAR(a.ax_g, -1.0f, 0.01f);
}

TEST(test_read_gyro_250dps)
{
    setup();
    plant_gyro(131, 0, 0);

    mpu6050_gyro_t g;
    ASSERT_EQ(mpu6050_read_gyro(&dev, &g), MPU6050_OK);
    ASSERT_NEAR(g.gx_dps, 1.0f, 0.01f);
    ASSERT_NEAR(g.gy_dps, 0.0f, 0.01f);
    ASSERT_NEAR(g.gz_dps, 0.0f, 0.01f);
}

TEST(test_accel_range_16g)
{
    mock_reset();
    mpu6050_cfg_t cfg = { .addr = MPU6050_ADDR_LOW,
                          .accel_range = MPU6050_ACCEL_16G,
                          .gyro_range = MPU6050_GYRO_250DPS };
    mpu6050_t d;
    mpu6050_init(&d, &mock_i2c, &cfg);
    plant_accel(2048, 0, 0);

    mpu6050_accel_t a;
    mpu6050_read_accel(&d, &a);
    ASSERT_NEAR(a.ax_g, 1.0f, 0.01f);
}

TEST(test_gyro_range_2000dps)
{
    mock_reset();
    mpu6050_cfg_t cfg = { .addr = MPU6050_ADDR_LOW,
                          .accel_range = MPU6050_ACCEL_2G,
                          .gyro_range = MPU6050_GYRO_2000DPS };
    mpu6050_t d;
    mpu6050_init(&d, &mock_i2c, &cfg);
    plant_gyro(164, 0, 0);

    mpu6050_gyro_t g;
    mpu6050_read_gyro(&d, &g);
    ASSERT_NEAR(g.gx_dps, 10.0f, 0.1f);
}

TEST(test_read_accel_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    mpu6050_accel_t a;
    ASSERT_EQ(mpu6050_read_accel(&dev, &a), MPU6050_ERR_I2C);
}

TEST(test_read_gyro_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    mpu6050_gyro_t g;
    ASSERT_EQ(mpu6050_read_gyro(&dev, &g), MPU6050_ERR_I2C);
}

TEST(test_reset)
{
    setup_chip(&chip_i2c);
    ASSERT_EQ(mpu6050_reset(&dev), MPU6050_OK);
    /* Awake on the PLL (not 0x40), with both ranges back (not 0) */
    ASSERT_EQ(mock_regs[0x6B], 0x01);
    ASSERT_EQ(mock_regs[0x1C], (2 << 3));
    ASSERT_EQ(mock_regs[0x1B], (2 << 3));
}

TEST(test_reset_waits_for_the_bit_to_clear)
{
    setup_chip(&chip_i2c);
    mock_reset_busy_reads = 3;
    ASSERT_EQ(mpu6050_reset(&dev), MPU6050_OK);
    ASSERT_EQ(mock_delay_ms_count, 3);
    ASSERT_EQ(mock_regs[0x6B], 0x01);
}

TEST(test_reset_polls_without_delay_ms)
{
    setup_chip(&chip_i2c_no_delay);
    mock_reset_busy_reads = 3;
    ASSERT_EQ(mpu6050_reset(&dev), MPU6050_OK);
    ASSERT_EQ(mock_regs[0x6B], 0x01);
}

TEST(test_reset_failed_read_means_not_done)
{
    /* write(107)=op0, then read(107)=op1 gets no acknowledge */
    setup_chip(&chip_i2c);
    mock_fail_at = 1;
    ASSERT_EQ(mpu6050_reset(&dev), MPU6050_OK);
    ASSERT_EQ(mock_regs[0x6B], 0x01);
}

TEST(test_reset_timeout)
{
    setup_chip(&chip_i2c);
    mock_reset_busy_reads = 1000000;
    ASSERT_EQ(mpu6050_reset(&dev), MPU6050_ERR_TIMEOUT);
    ASSERT_EQ(mock_regs[0x6B], 0x80);   /* nothing written during the reset */
}

TEST(test_reset_write_failures)
{
    /* write(107)=op0, read(107)=op1, write(107)=op2, write(0x1C)=op3,
     * write(0x1B)=op4 */
    static const int ops[] = { 0, 2, 3, 4 };
    for (unsigned i = 0; i < 4; i++) {
        setup_chip(&chip_i2c);
        mock_fail_at = ops[i];
        ASSERT_EQ(mpu6050_reset(&dev), MPU6050_ERR_I2C);
    }
}

TEST(test_addr_high)
{
    mock_reset();
    mpu6050_cfg_t cfg = { .addr = MPU6050_ADDR_HIGH,
                          .accel_range = MPU6050_ACCEL_2G,
                          .gyro_range = MPU6050_GYRO_250DPS };
    mpu6050_t d;
    ASSERT_EQ(mpu6050_init(&d, &mock_i2c, &cfg), MPU6050_OK);
}

TEST(test_config_registers)
{
    mock_reset();
    mpu6050_cfg_t cfg = { .addr = MPU6050_ADDR_LOW,
                          .accel_range = MPU6050_ACCEL_8G,
                          .gyro_range = MPU6050_GYRO_1000DPS };
    mpu6050_t d;
    mpu6050_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(mock_regs[0x1C], (2 << 3));
    ASSERT_EQ(mock_regs[0x1B], (2 << 3));
}

TEST_MAIN("mpu6050")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_accel_1g_z);
    RUN(test_read_accel_negative);
    RUN(test_read_gyro_250dps);
    RUN(test_accel_range_16g);
    RUN(test_gyro_range_2000dps);
    RUN(test_read_accel_i2c_fail);
    RUN(test_read_gyro_i2c_fail);
    RUN(test_reset);
    RUN(test_reset_waits_for_the_bit_to_clear);
    RUN(test_reset_polls_without_delay_ms);
    RUN(test_reset_failed_read_means_not_done);
    RUN(test_reset_timeout);
    RUN(test_reset_write_failures);
    RUN(test_addr_high);
    RUN(test_config_registers);
TEST_MAIN_END()
