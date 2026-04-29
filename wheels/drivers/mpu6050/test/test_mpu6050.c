#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../mpu6050.h"

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

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)
#define ASSERT_NEAR(a, b, tol, msg) ASSERT(fabsf((a)-(b)) < (tol), msg)

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

static void test_init_ok(void)
{
    printf("%s\n", __func__);
    mock_reset();
    mpu6050_t d;
    ASSERT_EQ(mpu6050_init(&d, &mock_i2c, &default_cfg), MPU6050_OK, "init ok");
}

static void test_init_wrong_id(void)
{
    printf("%s\n", __func__);
    mock_reset();
    mock_regs[0x75] = 0x00;
    mpu6050_t d;
    ASSERT_EQ(mpu6050_init(&d, &mock_i2c, &default_cfg), MPU6050_ERR_ID, "wrong WHO_AM_I");
}

static void test_init_i2c_fail(void)
{
    printf("%s\n", __func__);
    mock_reset();
    mock_fail_at = 0;
    mpu6050_t d;
    ASSERT_EQ(mpu6050_init(&d, &mock_i2c, &default_cfg), MPU6050_ERR_I2C, "init I2C fail");
}

static void test_null_safety(void)
{
    printf("%s\n", __func__);
    ASSERT_EQ(mpu6050_init(NULL, &mock_i2c, &default_cfg), MPU6050_ERR_PARAM, "null dev");
    mpu6050_t d;
    ASSERT_EQ(mpu6050_init(&d, NULL, &default_cfg), MPU6050_ERR_PARAM, "null i2c");
    ASSERT_EQ(mpu6050_init(&d, &mock_i2c, NULL), MPU6050_ERR_PARAM, "null cfg");
    mpu6050_accel_t a;
    ASSERT_EQ(mpu6050_read_accel(NULL, &a), MPU6050_ERR_PARAM, "accel null dev");
    ASSERT_EQ(mpu6050_read_accel(&d, NULL), MPU6050_ERR_PARAM, "accel null out");
    mpu6050_gyro_t g;
    ASSERT_EQ(mpu6050_read_gyro(NULL, &g), MPU6050_ERR_PARAM, "gyro null dev");
    ASSERT_EQ(mpu6050_read_gyro(&d, NULL), MPU6050_ERR_PARAM, "gyro null out");
    ASSERT_EQ(mpu6050_reset(NULL), MPU6050_ERR_PARAM, "reset null");
}

static void test_read_accel_1g_z(void)
{
    printf("%s\n", __func__);
    setup();
    plant_accel(0, 0, 16384);

    mpu6050_accel_t a;
    ASSERT_EQ(mpu6050_read_accel(&dev, &a), MPU6050_OK, "accel read ok");
    ASSERT_NEAR(a.ax, 0.0f, 0.01f, "ax=0");
    ASSERT_NEAR(a.ay, 0.0f, 0.01f, "ay=0");
    ASSERT_NEAR(a.az, 1.0f, 0.01f, "az=1g");
}

static void test_read_accel_negative(void)
{
    printf("%s\n", __func__);
    setup();
    plant_accel(-16384, 0, 0);

    mpu6050_accel_t a;
    mpu6050_read_accel(&dev, &a);
    ASSERT_NEAR(a.ax, -1.0f, 0.01f, "ax=-1g");
}

static void test_read_gyro_250dps(void)
{
    printf("%s\n", __func__);
    setup();
    plant_gyro(131, 0, 0);

    mpu6050_gyro_t g;
    ASSERT_EQ(mpu6050_read_gyro(&dev, &g), MPU6050_OK, "gyro read ok");
    ASSERT_NEAR(g.gx, 1.0f, 0.01f, "gx=1 dps");
    ASSERT_NEAR(g.gy, 0.0f, 0.01f, "gy=0");
    ASSERT_NEAR(g.gz, 0.0f, 0.01f, "gz=0");
}

static void test_accel_range_16g(void)
{
    printf("%s\n", __func__);
    mock_reset();
    mpu6050_cfg_t cfg = { .addr = MPU6050_ADDR_LOW,
                          .accel_range = MPU6050_ACCEL_16G,
                          .gyro_range = MPU6050_GYRO_250DPS };
    mpu6050_t d;
    mpu6050_init(&d, &mock_i2c, &cfg);
    plant_accel(2048, 0, 0);

    mpu6050_accel_t a;
    mpu6050_read_accel(&d, &a);
    ASSERT_NEAR(a.ax, 1.0f, 0.01f, "16G range: 2048 = 1g");
}

static void test_gyro_range_2000dps(void)
{
    printf("%s\n", __func__);
    mock_reset();
    mpu6050_cfg_t cfg = { .addr = MPU6050_ADDR_LOW,
                          .accel_range = MPU6050_ACCEL_2G,
                          .gyro_range = MPU6050_GYRO_2000DPS };
    mpu6050_t d;
    mpu6050_init(&d, &mock_i2c, &cfg);
    plant_gyro(164, 0, 0);

    mpu6050_gyro_t g;
    mpu6050_read_gyro(&d, &g);
    ASSERT_NEAR(g.gx, 10.0f, 0.1f, "2000dps: 164/16.4 = 10 dps");
}

static void test_read_accel_i2c_fail(void)
{
    printf("%s\n", __func__);
    setup();
    mock_fail_next = 1;
    mpu6050_accel_t a;
    ASSERT_EQ(mpu6050_read_accel(&dev, &a), MPU6050_ERR_I2C, "accel I2C fail");
}

static void test_read_gyro_i2c_fail(void)
{
    printf("%s\n", __func__);
    setup();
    mock_fail_next = 1;
    mpu6050_gyro_t g;
    ASSERT_EQ(mpu6050_read_gyro(&dev, &g), MPU6050_ERR_I2C, "gyro I2C fail");
}

static void test_reset(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT_EQ(mpu6050_reset(&dev), MPU6050_OK, "reset ok");
    ASSERT_EQ(mock_regs[0x6B], 0x80, "PWR_MGMT_1 reset bit set");
}

static void test_addr_high(void)
{
    printf("%s\n", __func__);
    mock_reset();
    mpu6050_cfg_t cfg = { .addr = MPU6050_ADDR_HIGH,
                          .accel_range = MPU6050_ACCEL_2G,
                          .gyro_range = MPU6050_GYRO_250DPS };
    mpu6050_t d;
    ASSERT_EQ(mpu6050_init(&d, &mock_i2c, &cfg), MPU6050_OK, "init addr 0x69");
}

static void test_config_registers(void)
{
    printf("%s\n", __func__);
    mock_reset();
    mpu6050_cfg_t cfg = { .addr = MPU6050_ADDR_LOW,
                          .accel_range = MPU6050_ACCEL_8G,
                          .gyro_range = MPU6050_GYRO_1000DPS };
    mpu6050_t d;
    mpu6050_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(mock_regs[0x1C], (2 << 3), "accel config 8G");
    ASSERT_EQ(mock_regs[0x1B], (2 << 3), "gyro config 1000dps");
}

int main(void)
{
    printf("test_mpu6050\n");
    test_init_ok();
    test_init_wrong_id();
    test_init_i2c_fail();
    test_null_safety();
    test_read_accel_1g_z();
    test_read_accel_negative();
    test_read_gyro_250dps();
    test_accel_range_16g();
    test_gyro_range_2000dps();
    test_read_accel_i2c_fail();
    test_read_gyro_i2c_fail();
    test_reset();
    test_addr_high();
    test_config_registers();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
