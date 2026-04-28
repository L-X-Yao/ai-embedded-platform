#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "../qmi8658a.h"

/* ---------- mock I2C ---------- */

#define MOCK_REG_COUNT 128
static uint8_t mock_regs[MOCK_REG_COUNT];
static int     mock_fail_next;    /* 1 = next I2C op fails */
static int     mock_write_count;
static int     mock_read_count;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_regs[0x00] = 0x05; /* WHO_AM_I */
    mock_fail_next  = 0;
    mock_write_count = 0;
    mock_read_count  = 0;
}

static bool mock_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)addr;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    if (reg < MOCK_REG_COUNT) mock_regs[reg] = val;
    mock_write_count++;
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t addr, uint8_t reg,
                            uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    for (uint16_t i = 0; i < len; i++)
        buf[i] = ((reg + i) < MOCK_REG_COUNT) ? mock_regs[reg + i] : 0xFF;
    mock_read_count++;
    return true;
}

static const qmi8658a_i2c_t mock_i2c = {
    .write_reg = mock_write_reg,
    .read_regs = mock_read_regs,
    .ctx       = NULL,
};

/* ---------- test helpers ---------- */

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

/* ---------- tests ---------- */

static void test_default_cfg(void)
{
    printf("test_default_cfg\n");
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    ASSERT_EQ(c.acc_range, QMI8658A_ACC_RANGE_8G, "acc_range default");
    ASSERT_EQ(c.gyr_range, QMI8658A_GYR_RANGE_2048, "gyr_range default");
}

static void test_init_ok(void)
{
    printf("test_init_ok\n");
    mock_reset();
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c);
    ASSERT_EQ(e, QMI8658A_OK, "init returns OK");
    ASSERT_EQ(mock_regs[0x08], 0x03, "CTRL7 = accel+gyro enable");
}

static void test_init_bad_chip_id(void)
{
    printf("test_init_bad_chip_id\n");
    mock_reset();
    mock_regs[0x00] = 0xFF;
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c);
    ASSERT_EQ(e, QMI8658A_ERR_CHIP_ID, "wrong chip ID detected");
}

static void test_init_i2c_fail(void)
{
    printf("test_init_i2c_fail\n");
    mock_reset();
    mock_fail_next = 1;
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c);
    ASSERT_EQ(e, QMI8658A_ERR_I2C, "I2C failure propagated");
}

static void test_read_data(void)
{
    printf("test_read_data\n");
    mock_reset();
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_init(&mock_i2c, &c);

    /* Plant known raw values at register 0x35..0x40
     * ax = 4096 (raw) → 1.0 G at ±8g (div=4096)
     * gy = 160  (raw) → 10.0 dps at ±2048 (div=16) */
    mock_regs[0x35] = 0x00; mock_regs[0x36] = 0x10; /* ax = 0x1000 = 4096 */
    mock_regs[0x37] = 0x00; mock_regs[0x38] = 0x00; /* ay = 0 */
    mock_regs[0x39] = 0x00; mock_regs[0x3A] = 0x00; /* az = 0 */
    mock_regs[0x3B] = 0x00; mock_regs[0x3C] = 0x00; /* gx = 0 */
    mock_regs[0x3D] = 0xA0; mock_regs[0x3E] = 0x00; /* gy = 0x00A0 = 160 */
    mock_regs[0x3F] = 0x00; mock_regs[0x40] = 0x00; /* gz = 0 */

    qmi8658a_data_t d;
    qmi8658a_err_t e = qmi8658a_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMI8658A_OK, "read OK");
    ASSERT_NEAR(d.ax, 1.0f, 0.01f, "ax = 1.0 G");
    ASSERT_NEAR(d.ay, 0.0f, 0.01f, "ay = 0");
    ASSERT_NEAR(d.gy, 10.0f, 0.01f, "gy = 10.0 dps");
}

static void test_read_i2c_fail(void)
{
    printf("test_read_i2c_fail\n");
    mock_reset();
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_init(&mock_i2c, &c);
    mock_fail_next = 1;
    qmi8658a_data_t d;
    qmi8658a_err_t e = qmi8658a_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMI8658A_ERR_I2C, "read I2C error propagated");
}

static void test_config_range(void)
{
    printf("test_config_range\n");
    mock_reset();
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    c.acc_range = QMI8658A_ACC_RANGE_2G;
    c.gyr_range = QMI8658A_GYR_RANGE_512;
    c.acc_odr   = QMI8658A_ODR_250;
    c.gyr_odr   = QMI8658A_ODR_125;
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c);
    ASSERT_EQ(e, QMI8658A_OK, "init with custom cfg");
    ASSERT_EQ(mock_regs[0x03], 0x05, "CTRL2 = ±2g, 250Hz");
    ASSERT_EQ(mock_regs[0x04], 0x56, "CTRL3 = ±512dps, 125Hz");

    /* ±2g → div=16384, raw=16384 → 1.0G */
    mock_regs[0x35] = 0x00; mock_regs[0x36] = 0x40; /* 0x4000 = 16384 */
    mock_regs[0x37] = 0; mock_regs[0x38] = 0;
    mock_regs[0x39] = 0; mock_regs[0x3A] = 0;
    mock_regs[0x3B] = 0; mock_regs[0x3C] = 0;
    mock_regs[0x3D] = 0; mock_regs[0x3E] = 0;
    mock_regs[0x3F] = 0; mock_regs[0x40] = 0;

    qmi8658a_data_t d;
    qmi8658a_read(&mock_i2c, &d);
    ASSERT_NEAR(d.ax, 1.0f, 0.01f, "ax = 1.0G at ±2g range");
}

static void test_reset(void)
{
    printf("test_reset\n");
    mock_reset();
    qmi8658a_err_t e = qmi8658a_reset(&mock_i2c);
    ASSERT_EQ(e, QMI8658A_OK, "reset OK");
    ASSERT_EQ(mock_regs[0x60], 0xB0, "reset register written");
}

static void test_read_negative(void)
{
    printf("test_read_negative\n");
    mock_reset();
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_init(&mock_i2c, &c);

    /* ax = -4096 (0xF000) → -1.0 G at ±8g (div=4096) */
    mock_regs[0x35] = 0x00; mock_regs[0x36] = 0xF0;
    mock_regs[0x37] = 0; mock_regs[0x38] = 0;
    mock_regs[0x39] = 0; mock_regs[0x3A] = 0;
    mock_regs[0x3B] = 0; mock_regs[0x3C] = 0;
    mock_regs[0x3D] = 0; mock_regs[0x3E] = 0;
    mock_regs[0x3F] = 0; mock_regs[0x40] = 0;

    qmi8658a_data_t d;
    qmi8658a_err_t e = qmi8658a_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMI8658A_OK, "read negative OK");
    ASSERT_NEAR(d.ax, -1.0f, 0.01f, "ax = -1.0 G");
}

static void test_read_int16_extremes(void)
{
    printf("test_read_int16_extremes\n");
    mock_reset();
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_init(&mock_i2c, &c);

    /* ax = INT16_MAX (0x7FFF = 32767) → 32767/4096 ≈ 8.0 G */
    mock_regs[0x35] = 0xFF; mock_regs[0x36] = 0x7F;
    /* ay = INT16_MIN (0x8000 = -32768) → -32768/4096 = -8.0 G */
    mock_regs[0x37] = 0x00; mock_regs[0x38] = 0x80;
    mock_regs[0x39] = 0; mock_regs[0x3A] = 0;
    mock_regs[0x3B] = 0; mock_regs[0x3C] = 0;
    mock_regs[0x3D] = 0; mock_regs[0x3E] = 0;
    mock_regs[0x3F] = 0; mock_regs[0x40] = 0;

    qmi8658a_data_t d;
    qmi8658a_read(&mock_i2c, &d);
    ASSERT_NEAR(d.ax, 32767.0f / 4096.0f, 0.01f, "ax = INT16_MAX / div");
    ASSERT_NEAR(d.ay, -32768.0f / 4096.0f, 0.01f, "ay = INT16_MIN / div");
}

static void test_null_safety(void)
{
    printf("test_null_safety\n");
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_data_t d;
    ASSERT_EQ(qmi8658a_init(NULL, &c), QMI8658A_ERR_I2C, "init(NULL, cfg)");
    ASSERT_EQ(qmi8658a_init(&mock_i2c, NULL), QMI8658A_ERR_I2C, "init(i2c, NULL)");
    ASSERT_EQ(qmi8658a_read(NULL, &d), QMI8658A_ERR_I2C, "read(NULL, &d)");
    ASSERT_EQ(qmi8658a_read(&mock_i2c, NULL), QMI8658A_ERR_I2C, "read(i2c, NULL)");
    ASSERT_EQ(qmi8658a_reset(NULL), QMI8658A_ERR_I2C, "reset(NULL)");
}

static void test_reinit_changes_config(void)
{
    printf("test_reinit_changes_config\n");
    mock_reset();
    qmi8658a_cfg_t c1 = qmi8658a_default_cfg();
    c1.acc_range = QMI8658A_ACC_RANGE_8G;
    qmi8658a_init(&mock_i2c, &c1);
    uint8_t ctrl2_first = mock_regs[0x03];

    qmi8658a_cfg_t c2 = qmi8658a_default_cfg();
    c2.acc_range = QMI8658A_ACC_RANGE_2G;
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c2);
    ASSERT_EQ(e, QMI8658A_OK, "re-init OK");
    ASSERT(mock_regs[0x03] != ctrl2_first, "CTRL2 updated on re-init");
    ASSERT_EQ(mock_regs[0x03], 0x04, "CTRL2 = ±2g, 500Hz");
}

/* ---------- main ---------- */

int main(void)
{
    test_default_cfg();
    test_init_ok();
    test_init_bad_chip_id();
    test_init_i2c_fail();
    test_read_data();
    test_read_i2c_fail();
    test_read_negative();
    test_read_int16_extremes();
    test_config_range();
    test_reset();
    test_null_safety();
    test_reinit_changes_config();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
