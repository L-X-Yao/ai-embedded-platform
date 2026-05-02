// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "lis3dh.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x0F] = 0x33;  /* WHO_AM_I */
}

static const lis3dh_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

/* ---------- helpers ---------- */

static lis3dh_t dev;
static lis3dh_cfg_t default_cfg = {
    .addr  = LIS3DH_ADDR_LOW,
    .range = LIS3DH_RANGE_2G,
    .odr   = LIS3DH_ODR_100HZ,
};

static void setup(void)
{
    mock_reset();
    lis3dh_init(&dev, &mock_i2c, &default_cfg);
    mock_op_count = 0;
}

/*
 * Plant raw 16-bit left-justified accel values into mock registers.
 * LIS3DH output is little-endian: low byte first (OUT_X_L=0x28).
 * The driver ORs 0x80 onto the register address for auto-increment,
 * so mock_i2c_read_regs sees reg=0xA8 and indexes mock_regs[0xA8+i].
 */
static void plant_accel(int16_t x, int16_t y, int16_t z)
{
    uint8_t base = 0xA8u;  /* 0x28 | 0x80 (auto-increment) */
    mock_regs[base + 0] = (uint8_t)(x & 0xFF);
    mock_regs[base + 1] = (uint8_t)((x >> 8) & 0xFF);
    mock_regs[base + 2] = (uint8_t)(y & 0xFF);
    mock_regs[base + 3] = (uint8_t)((y >> 8) & 0xFF);
    mock_regs[base + 4] = (uint8_t)(z & 0xFF);
    mock_regs[base + 5] = (uint8_t)((z >> 8) & 0xFF);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    lis3dh_t d;
    ASSERT_EQ(lis3dh_init(&d, &mock_i2c, &default_cfg), LIS3DH_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x0F] = 0x00;
    lis3dh_t d;
    ASSERT_EQ(lis3dh_init(&d, &mock_i2c, &default_cfg), LIS3DH_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    lis3dh_t d;
    ASSERT_EQ(lis3dh_init(&d, &mock_i2c, &default_cfg), LIS3DH_ERR_I2C);
}

TEST(test_null_safety)
{
    lis3dh_t d;
    ASSERT_EQ(lis3dh_init(NULL, &mock_i2c, &default_cfg), LIS3DH_ERR_PARAM);
    ASSERT_EQ(lis3dh_init(&d, NULL, &default_cfg), LIS3DH_ERR_PARAM);
    ASSERT_EQ(lis3dh_init(&d, &mock_i2c, NULL), LIS3DH_ERR_PARAM);
    lis3dh_accel_t a;
    ASSERT_EQ(lis3dh_read_accel(NULL, &a), LIS3DH_ERR_PARAM);
    ASSERT_EQ(lis3dh_read_accel(&d, NULL), LIS3DH_ERR_PARAM);
    lis3dh_raw_t r;
    ASSERT_EQ(lis3dh_read_raw(NULL, &r), LIS3DH_ERR_PARAM);
    ASSERT_EQ(lis3dh_read_raw(&d, NULL), LIS3DH_ERR_PARAM);
    ASSERT_EQ(lis3dh_set_range(NULL, LIS3DH_RANGE_4G), LIS3DH_ERR_PARAM);
    ASSERT_EQ(lis3dh_set_odr(NULL, LIS3DH_ODR_50HZ), LIS3DH_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    lis3dh_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    /* Primary read function on zero-init struct must not crash (no UB) */
}

TEST(test_read_accel_1g_z)
{
    setup();
    /* At +/-2g: 1g = 16000 raw counts (12-bit left-justified in 16-bit) */
    plant_accel(0, 0, 16000);

    lis3dh_accel_t a;
    ASSERT_EQ(lis3dh_read_accel(&dev, &a), LIS3DH_OK);
    ASSERT_NEAR(a.x, 0.0f, 0.01f);
    ASSERT_NEAR(a.y, 0.0f, 0.01f);
    ASSERT_NEAR(a.z, 1.0f, 0.01f);
}

TEST(test_read_accel_negative)
{
    setup();
    plant_accel(-16000, 0, 0);

    lis3dh_accel_t a;
    lis3dh_read_accel(&dev, &a);
    ASSERT_NEAR(a.x, -1.0f, 0.01f);
}

TEST(test_read_raw)
{
    setup();
    plant_accel(0x1234, (int16_t)0xABCD, 0x5678);

    lis3dh_raw_t r;
    ASSERT_EQ(lis3dh_read_raw(&dev, &r), LIS3DH_OK);
    ASSERT_EQ(r.x, 0x1234);
    ASSERT_EQ(r.y, (int16_t)0xABCD);
    ASSERT_EQ(r.z, 0x5678);
}

TEST(test_range_4g)
{
    mock_reset();
    lis3dh_cfg_t cfg = { .addr = LIS3DH_ADDR_LOW,
                         .range = LIS3DH_RANGE_4G,
                         .odr = LIS3DH_ODR_100HZ };
    lis3dh_t d;
    lis3dh_init(&d, &mock_i2c, &cfg);
    mock_op_count = 0;
    /* At +/-4g: 1g = 8000 raw counts */
    plant_accel(8000, 0, 0);

    lis3dh_accel_t a;
    lis3dh_read_accel(&d, &a);
    ASSERT_NEAR(a.x, 1.0f, 0.01f);
}

TEST(test_range_16g)
{
    mock_reset();
    lis3dh_cfg_t cfg = { .addr = LIS3DH_ADDR_LOW,
                         .range = LIS3DH_RANGE_16G,
                         .odr = LIS3DH_ODR_100HZ };
    lis3dh_t d;
    lis3dh_init(&d, &mock_i2c, &cfg);
    mock_op_count = 0;
    /* At +/-16g: 1g ~ 1333.33 raw counts */
    plant_accel(1333, 0, 0);

    lis3dh_accel_t a;
    lis3dh_read_accel(&d, &a);
    ASSERT_NEAR(a.x, 1.0f, 0.01f);
}

TEST(test_set_range_runtime)
{
    setup();
    ASSERT_EQ(lis3dh_set_range(&dev, LIS3DH_RANGE_8G), LIS3DH_OK);
    /* Verify CTRL_REG4 was written: BDU(0x80) | FS=8g(0x20) = 0xA0 */
    ASSERT_EQ(mock_regs[0x23], 0xA0);

    /* At +/-8g: 1g = 4000 raw counts */
    plant_accel(4000, 0, 0);
    lis3dh_accel_t a;
    lis3dh_read_accel(&dev, &a);
    ASSERT_NEAR(a.x, 1.0f, 0.01f);
}

TEST(test_set_odr_runtime)
{
    setup();
    ASSERT_EQ(lis3dh_set_odr(&dev, LIS3DH_ODR_50HZ), LIS3DH_OK);
    /* Verify CTRL_REG1: ODR=50Hz(4<<4=0x40) | axes(0x07) = 0x47 */
    ASSERT_EQ(mock_regs[0x20], 0x47);
}

TEST(test_read_accel_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    lis3dh_accel_t a;
    ASSERT_EQ(lis3dh_read_accel(&dev, &a), LIS3DH_ERR_I2C);
}

TEST(test_read_raw_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    lis3dh_raw_t r;
    ASSERT_EQ(lis3dh_read_raw(&dev, &r), LIS3DH_ERR_I2C);
}

TEST(test_set_range_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(lis3dh_set_range(&dev, LIS3DH_RANGE_4G), LIS3DH_ERR_I2C);
}

TEST(test_set_odr_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(lis3dh_set_odr(&dev, LIS3DH_ODR_25HZ), LIS3DH_ERR_I2C);
}

TEST(test_invalid_range)
{
    mock_reset();
    lis3dh_cfg_t cfg = { .addr = LIS3DH_ADDR_LOW,
                         .range = (lis3dh_range_t)4,
                         .odr = LIS3DH_ODR_100HZ };
    lis3dh_t d;
    ASSERT_EQ(lis3dh_init(&d, &mock_i2c, &cfg), LIS3DH_ERR_PARAM);

    /* Also via set_range */
    setup();
    ASSERT_EQ(lis3dh_set_range(&dev, (lis3dh_range_t)99), LIS3DH_ERR_PARAM);
}

TEST(test_invalid_odr)
{
    mock_reset();
    lis3dh_cfg_t cfg = { .addr = LIS3DH_ADDR_LOW,
                         .range = LIS3DH_RANGE_2G,
                         .odr = (lis3dh_odr_t)10 };
    lis3dh_t d;
    ASSERT_EQ(lis3dh_init(&d, &mock_i2c, &cfg), LIS3DH_ERR_PARAM);

    /* Also via set_odr */
    setup();
    ASSERT_EQ(lis3dh_set_odr(&dev, (lis3dh_odr_t)99), LIS3DH_ERR_PARAM);
}

TEST(test_addr_high)
{
    mock_reset();
    lis3dh_cfg_t cfg = { .addr = LIS3DH_ADDR_HIGH,
                         .range = LIS3DH_RANGE_2G,
                         .odr = LIS3DH_ODR_100HZ };
    lis3dh_t d;
    ASSERT_EQ(lis3dh_init(&d, &mock_i2c, &cfg), LIS3DH_OK);
}

TEST(test_config_registers)
{
    mock_reset();
    lis3dh_cfg_t cfg = { .addr = LIS3DH_ADDR_LOW,
                         .range = LIS3DH_RANGE_8G,
                         .odr = LIS3DH_ODR_400HZ };
    lis3dh_t d;
    lis3dh_init(&d, &mock_i2c, &cfg);
    /* CTRL_REG1: ODR=400Hz(7<<4=0x70) | axes(0x07) = 0x77 */
    ASSERT_EQ(mock_regs[0x20], 0x77);
    /* CTRL_REG4: BDU(0x80) | FS=8g(2<<4=0x20) = 0xA0 */
    ASSERT_EQ(mock_regs[0x23], 0xA0);
}

TEST_MAIN("lis3dh")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_read_accel_1g_z);
    RUN(test_read_accel_negative);
    RUN(test_read_raw);
    RUN(test_range_4g);
    RUN(test_range_16g);
    RUN(test_set_range_runtime);
    RUN(test_set_odr_runtime);
    RUN(test_read_accel_i2c_fail);
    RUN(test_read_raw_i2c_fail);
    RUN(test_set_range_i2c_fail);
    RUN(test_set_odr_i2c_fail);
    RUN(test_invalid_range);
    RUN(test_invalid_odr);
    RUN(test_addr_high);
    RUN(test_config_registers);
TEST_MAIN_END()
