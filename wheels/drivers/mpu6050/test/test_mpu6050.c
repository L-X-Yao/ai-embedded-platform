#include "test_harness.h"
#include <string.h>
#include <math.h>
#include "mpu6050.h"

/* ---------- mock I2C ---------- */

static uint8_t mock_regs[256];
static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_fail_next = 0;
    mock_op_count  = 0;
    mock_fail_at   = -1;
    mock_regs[0x75] = 0x68;
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    mock_regs[reg] = val;
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t addr, uint8_t reg,
                            uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    for (uint16_t i = 0; i < len; i++)
        buf[i] = mock_regs[(uint8_t)(reg + i)];
    return true;
}

static const mpu6050_i2c_t mock_i2c = {
    .write_reg = mock_write_reg,
    .read_regs = mock_read_regs,
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
    ASSERT_NEAR(a.ax, 0.0f, 0.01f);
    ASSERT_NEAR(a.ay, 0.0f, 0.01f);
    ASSERT_NEAR(a.az, 1.0f, 0.01f);
}

TEST(test_read_accel_negative)
{
    setup();
    plant_accel(-16384, 0, 0);

    mpu6050_accel_t a;
    mpu6050_read_accel(&dev, &a);
    ASSERT_NEAR(a.ax, -1.0f, 0.01f);
}

TEST(test_read_gyro_250dps)
{
    setup();
    plant_gyro(131, 0, 0);

    mpu6050_gyro_t g;
    ASSERT_EQ(mpu6050_read_gyro(&dev, &g), MPU6050_OK);
    ASSERT_NEAR(g.gx, 1.0f, 0.01f);
    ASSERT_NEAR(g.gy, 0.0f, 0.01f);
    ASSERT_NEAR(g.gz, 0.0f, 0.01f);
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
    ASSERT_NEAR(a.ax, 1.0f, 0.01f);
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
    ASSERT_NEAR(g.gx, 10.0f, 0.1f);
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
    setup();
    ASSERT_EQ(mpu6050_reset(&dev), MPU6050_OK);
    ASSERT_EQ(mock_regs[0x6B], 0x80);
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
    RUN(test_addr_high);
    RUN(test_config_registers);
TEST_MAIN_END()

