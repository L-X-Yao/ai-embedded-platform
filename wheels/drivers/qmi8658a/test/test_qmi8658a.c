#include "test_harness.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "qmi8658a.h"

/* ---------- mock I2C ---------- */

#define MOCK_REG_COUNT 128
static uint8_t mock_regs[MOCK_REG_COUNT];
static int     mock_fail_next;    /* 1 = next I2C op fails */
static int     mock_fail_at_write; /* fail at Nth write (0=disabled) */
static int     mock_write_count;
static int     mock_read_count;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_regs[0x00] = 0x05; /* WHO_AM_I */
    mock_fail_next  = 0;
    mock_fail_at_write = 0;
    mock_write_count = 0;
    mock_read_count  = 0;
}

static bool mock_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)addr;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    mock_write_count++;
    if (mock_fail_at_write && mock_write_count == mock_fail_at_write) { return false; }
    if (reg < MOCK_REG_COUNT) mock_regs[reg] = val;
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
/* ---------- tests ---------- */

TEST(test_default_cfg)
{
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    ASSERT_EQ(c.acc_range, QMI8658A_ACC_RANGE_8G);
    ASSERT_EQ(c.gyr_range, QMI8658A_GYR_RANGE_2048);
}

TEST(test_init_ok)
{
    mock_reset();
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c);
    ASSERT_EQ(e, QMI8658A_OK);
    ASSERT_EQ(mock_regs[0x08], 0x03);
}

TEST(test_init_bad_chip_id)
{
    mock_reset();
    mock_regs[0x00] = 0xFF;
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c);
    ASSERT_EQ(e, QMI8658A_ERR_CHIP_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_next = 1;
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c);
    ASSERT_EQ(e, QMI8658A_ERR_I2C);
}

TEST(test_read_data)
{
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
    ASSERT_EQ(e, QMI8658A_OK);
    ASSERT_NEAR(d.ax, 1.0f, 0.01f);
    ASSERT_NEAR(d.ay, 0.0f, 0.01f);
    ASSERT_NEAR(d.gy, 10.0f, 0.01f);
}

TEST(test_read_i2c_fail)
{
    mock_reset();
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_init(&mock_i2c, &c);
    mock_fail_next = 1;
    qmi8658a_data_t d;
    qmi8658a_err_t e = qmi8658a_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMI8658A_ERR_I2C);
}

TEST(test_config_range)
{
    mock_reset();
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    c.acc_range = QMI8658A_ACC_RANGE_2G;
    c.gyr_range = QMI8658A_GYR_RANGE_512;
    c.acc_odr   = QMI8658A_ODR_250;
    c.gyr_odr   = QMI8658A_ODR_125;
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c);
    ASSERT_EQ(e, QMI8658A_OK);
    ASSERT_EQ(mock_regs[0x03], 0x05);
    ASSERT_EQ(mock_regs[0x04], 0x56);

    /* ±2g → div=16384, raw=16384 → 1.0G */
    mock_regs[0x35] = 0x00; mock_regs[0x36] = 0x40; /* 0x4000 = 16384 */
    mock_regs[0x37] = 0; mock_regs[0x38] = 0;
    mock_regs[0x39] = 0; mock_regs[0x3A] = 0;
    mock_regs[0x3B] = 0; mock_regs[0x3C] = 0;
    mock_regs[0x3D] = 0; mock_regs[0x3E] = 0;
    mock_regs[0x3F] = 0; mock_regs[0x40] = 0;

    qmi8658a_data_t d;
    qmi8658a_read(&mock_i2c, &d);
    ASSERT_NEAR(d.ax, 1.0f, 0.01f);
}

TEST(test_reset)
{
    mock_reset();
    qmi8658a_err_t e = qmi8658a_reset(&mock_i2c);
    ASSERT_EQ(e, QMI8658A_OK);
    ASSERT_EQ(mock_regs[0x60], 0xB0);
}

TEST(test_read_negative)
{
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
    ASSERT_EQ(e, QMI8658A_OK);
    ASSERT_NEAR(d.ax, -1.0f, 0.01f);
}

TEST(test_read_int16_extremes)
{
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
    ASSERT_NEAR(d.ax, 32767.0f / 4096.0f, 0.01f);
    ASSERT_NEAR(d.ay, -32768.0f / 4096.0f, 0.01f);
}

TEST(test_null_safety)
{
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_data_t d;
    ASSERT_EQ(qmi8658a_init(NULL, &c), QMI8658A_ERR_I2C);
    ASSERT_EQ(qmi8658a_init(&mock_i2c, NULL), QMI8658A_ERR_I2C);
    ASSERT_EQ(qmi8658a_read(NULL, &d), QMI8658A_ERR_I2C);
    ASSERT_EQ(qmi8658a_read(&mock_i2c, NULL), QMI8658A_ERR_I2C);
    ASSERT_EQ(qmi8658a_reset(NULL), QMI8658A_ERR_I2C);
}

TEST(test_reinit_changes_config)
{
    mock_reset();
    qmi8658a_cfg_t c1 = qmi8658a_default_cfg();
    c1.acc_range = QMI8658A_ACC_RANGE_8G;
    qmi8658a_init(&mock_i2c, &c1);
    uint8_t ctrl2_first = mock_regs[0x03];

    qmi8658a_cfg_t c2 = qmi8658a_default_cfg();
    c2.acc_range = QMI8658A_ACC_RANGE_2G;
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c2);
    ASSERT_EQ(e, QMI8658A_OK);
    ASSERT_TRUE(mock_regs[0x03] != ctrl2_first);
    ASSERT_EQ(mock_regs[0x03], 0x04);
}

TEST(test_init_ctrl2_write_fail)
{
    mock_reset();
    mock_fail_at_write = 2;
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c);
    ASSERT_EQ(e, QMI8658A_ERR_I2C);
}

TEST(test_init_ctrl3_write_fail)
{
    mock_reset();
    mock_fail_at_write = 3;
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c);
    ASSERT_EQ(e, QMI8658A_ERR_I2C);
}

TEST(test_init_ctrl7_write_fail)
{
    mock_reset();
    mock_fail_at_write = 4;
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_err_t e = qmi8658a_init(&mock_i2c, &c);
    ASSERT_EQ(e, QMI8658A_ERR_I2C);
}

TEST(test_all_acc_ranges)
{
    const float divs[] = {16384.0f, 8192.0f, 4096.0f, 2048.0f};
    const int ranges[] = {QMI8658A_ACC_RANGE_2G, QMI8658A_ACC_RANGE_4G,
                          QMI8658A_ACC_RANGE_8G, QMI8658A_ACC_RANGE_16G};
    for (int r = 0; r < 4; r++) {
        mock_reset();
        qmi8658a_cfg_t c = qmi8658a_default_cfg();
        c.acc_range = ranges[r];
        qmi8658a_init(&mock_i2c, &c);

        int16_t raw = 1000;
        mock_regs[0x35] = (uint8_t)(raw & 0xFF);
        mock_regs[0x36] = (uint8_t)(raw >> 8);
        memset(&mock_regs[0x37], 0, 10);

        qmi8658a_data_t d;
        qmi8658a_read(&mock_i2c, &d);
        ASSERT_NEAR(d.ax, 1000.0f / divs[r], 0.01f);
    }
}

TEST(test_gyro_range_scaling)
{
    mock_reset();
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    c.gyr_range = QMI8658A_GYR_RANGE_256;
    qmi8658a_init(&mock_i2c, &c);

    int16_t raw = 128;
    memset(&mock_regs[0x35], 0, 6);
    mock_regs[0x3B] = (uint8_t)(raw & 0xFF);
    mock_regs[0x3C] = (uint8_t)(raw >> 8);
    mock_regs[0x3D] = 0; mock_regs[0x3E] = 0;
    mock_regs[0x3F] = 0; mock_regs[0x40] = 0;

    qmi8658a_data_t d;
    qmi8658a_read(&mock_i2c, &d);
    ASSERT_NEAR(d.gx, 128.0f / 128.0f, 0.01f);
}

TEST(test_reset_i2c_fail)
{
    mock_reset();
    mock_fail_next = 1;
    qmi8658a_err_t e = qmi8658a_reset(&mock_i2c);
    ASSERT_EQ(e, QMI8658A_ERR_I2C);
}

TEST(test_all_six_axes)
{
    mock_reset();
    qmi8658a_cfg_t c = qmi8658a_default_cfg();
    qmi8658a_init(&mock_i2c, &c);

    int16_t vals[] = {4096, -2048, 1024, 16, -32, 8};
    for (int i = 0; i < 6; i++) {
        mock_regs[0x35 + i*2]     = (uint8_t)(vals[i] & 0xFF);
        mock_regs[0x35 + i*2 + 1] = (uint8_t)(vals[i] >> 8);
    }

    qmi8658a_data_t d;
    qmi8658a_read(&mock_i2c, &d);
    ASSERT_NEAR(d.ax,  4096.0f / 4096.0f, 0.01f);
    ASSERT_NEAR(d.ay, -2048.0f / 4096.0f, 0.01f);
    ASSERT_NEAR(d.az,  1024.0f / 4096.0f, 0.01f);
    ASSERT_NEAR(d.gx,    16.0f / 16.0f,   0.01f);
    ASSERT_NEAR(d.gy,   -32.0f / 16.0f,   0.01f);
    ASSERT_NEAR(d.gz,     8.0f / 16.0f,   0.01f);
}

/* ---------- main ---------- */

TEST_MAIN("qmi8658a")
    RUN(test_default_cfg);
    RUN(test_init_ok);
    RUN(test_init_bad_chip_id);
    RUN(test_init_i2c_fail);
    RUN(test_read_data);
    RUN(test_read_i2c_fail);
    RUN(test_read_negative);
    RUN(test_read_int16_extremes);
    RUN(test_config_range);
    RUN(test_reset);
    RUN(test_null_safety);
    RUN(test_reinit_changes_config);
    RUN(test_init_ctrl2_write_fail);
    RUN(test_init_ctrl3_write_fail);
    RUN(test_init_ctrl7_write_fail);
    RUN(test_all_acc_ranges);
    RUN(test_gyro_range_scaling);
    RUN(test_reset_i2c_fail);
    RUN(test_all_six_axes);
TEST_MAIN_END()

