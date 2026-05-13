// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "lsm6dso.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x0F] = 0x6C;  /* WHO_AM_I */
}

static const lsm6dso_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

/* ---------- helpers ---------- */
static lsm6dso_t dev;
static lsm6dso_cfg_t default_cfg = {
    .addr        = LSM6DSO_ADDR_LOW,
    .accel_range = LSM6DSO_ACCEL_2G,
    .gyro_range  = LSM6DSO_GYRO_250DPS,
    .odr         = LSM6DSO_ODR_104HZ,
};

static void setup(void)
{
    mock_reset();
    lsm6dso_init(&dev, &mock_i2c, &default_cfg);
    mock_op_count = 0;
}

/* Plant little-endian accel data at 0x28..0x2D */
static void plant_accel(int16_t x, int16_t y, int16_t z)
{
    mock_regs[0x28] = (uint8_t)(x & 0xFF);
    mock_regs[0x29] = (uint8_t)((uint16_t)x >> 8);
    mock_regs[0x2A] = (uint8_t)(y & 0xFF);
    mock_regs[0x2B] = (uint8_t)((uint16_t)y >> 8);
    mock_regs[0x2C] = (uint8_t)(z & 0xFF);
    mock_regs[0x2D] = (uint8_t)((uint16_t)z >> 8);
}

/* Plant little-endian gyro data at 0x22..0x27 */
static void plant_gyro(int16_t x, int16_t y, int16_t z)
{
    mock_regs[0x22] = (uint8_t)(x & 0xFF);
    mock_regs[0x23] = (uint8_t)((uint16_t)x >> 8);
    mock_regs[0x24] = (uint8_t)(y & 0xFF);
    mock_regs[0x25] = (uint8_t)((uint16_t)y >> 8);
    mock_regs[0x26] = (uint8_t)(z & 0xFF);
    mock_regs[0x27] = (uint8_t)((uint16_t)z >> 8);
}

/* Plant little-endian temperature data at 0x20..0x21 */
static void plant_temp(int16_t raw)
{
    mock_regs[0x20] = (uint8_t)(raw & 0xFF);
    mock_regs[0x21] = (uint8_t)((uint16_t)raw >> 8);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    lsm6dso_t d;
    ASSERT_EQ(lsm6dso_init(&d, &mock_i2c, &default_cfg), LSM6DSO_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x0F] = 0x00;
    lsm6dso_t d;
    ASSERT_EQ(lsm6dso_init(&d, &mock_i2c, &default_cfg), LSM6DSO_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    lsm6dso_t d;
    ASSERT_EQ(lsm6dso_init(&d, &mock_i2c, &default_cfg), LSM6DSO_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(lsm6dso_init(NULL, &mock_i2c, &default_cfg), LSM6DSO_ERR_PARAM);
    lsm6dso_t d;
    ASSERT_EQ(lsm6dso_init(&d, NULL, &default_cfg), LSM6DSO_ERR_PARAM);
    ASSERT_EQ(lsm6dso_init(&d, &mock_i2c, NULL), LSM6DSO_ERR_PARAM);

    lsm6dso_accel_data_t a;
    ASSERT_EQ(lsm6dso_read_accel(NULL, &a), LSM6DSO_ERR_PARAM);
    ASSERT_EQ(lsm6dso_read_accel(&d, NULL), LSM6DSO_ERR_PARAM);

    lsm6dso_gyro_data_t g;
    ASSERT_EQ(lsm6dso_read_gyro(NULL, &g), LSM6DSO_ERR_PARAM);
    ASSERT_EQ(lsm6dso_read_gyro(&d, NULL), LSM6DSO_ERR_PARAM);

    float temp;
    ASSERT_EQ(lsm6dso_read_temp(NULL, &temp), LSM6DSO_ERR_PARAM);
    ASSERT_EQ(lsm6dso_read_temp(&d, NULL), LSM6DSO_ERR_PARAM);

    ASSERT_EQ(lsm6dso_reset(NULL), LSM6DSO_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    lsm6dso_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    lsm6dso_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    lsm6dso_cfg_t cfg = default_cfg;
    ASSERT_TRUE(lsm6dso_init(&zero_dev, &zero_hal, &cfg) < 0);
}

TEST(test_init_configures_ctrl1_xl)
{
    mock_reset();
    lsm6dso_cfg_t cfg = {
        .addr = LSM6DSO_ADDR_LOW,
        .accel_range = LSM6DSO_ACCEL_8G,
        .gyro_range = LSM6DSO_GYRO_250DPS,
        .odr = LSM6DSO_ODR_104HZ,
    };
    lsm6dso_t d;
    lsm6dso_init(&d, &mock_i2c, &cfg);
    /* CTRL1_XL = ODR[7:4]=4 | FS_XL[3:2]=3 (8G) -> 0x4C */
    ASSERT_EQ(mock_regs[0x10], (uint8_t)((4 << 4) | (3 << 2)));
}

TEST(test_init_configures_ctrl2_g)
{
    mock_reset();
    lsm6dso_cfg_t cfg = {
        .addr = LSM6DSO_ADDR_LOW,
        .accel_range = LSM6DSO_ACCEL_2G,
        .gyro_range = LSM6DSO_GYRO_1000DPS,
        .odr = LSM6DSO_ODR_104HZ,
    };
    lsm6dso_t d;
    lsm6dso_init(&d, &mock_i2c, &cfg);
    /* CTRL2_G = ODR[7:4]=4 | FS_G[3:0]=8 (1000DPS) -> 0x48 */
    ASSERT_EQ(mock_regs[0x11], (uint8_t)((4 << 4) | 8));
}

/* ---------- accel read tests ---------- */

TEST(test_read_accel_1g_z)
{
    setup();
    /* At 2G range, sensitivity = 0.061 mg/LSB, 1000mg / 0.061 ~ 16393 */
    plant_accel(0, 0, 16393);

    lsm6dso_accel_data_t a;
    ASSERT_EQ(lsm6dso_read_accel(&dev, &a), LSM6DSO_OK);
    ASSERT_NEAR(a.x_mg, 0.0f, 1.0f);
    ASSERT_NEAR(a.y_mg, 0.0f, 1.0f);
    ASSERT_NEAR(a.z_mg, 1000.0f, 2.0f);
}

TEST(test_read_accel_negative)
{
    setup();
    plant_accel(-16393, 0, 0);

    lsm6dso_accel_data_t a;
    lsm6dso_read_accel(&dev, &a);
    ASSERT_NEAR(a.x_mg, -1000.0f, 2.0f);
}

TEST(test_read_accel_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    lsm6dso_accel_data_t a;
    ASSERT_EQ(lsm6dso_read_accel(&dev, &a), LSM6DSO_ERR_I2C);
}

/* ---------- gyro read tests ---------- */

TEST(test_read_gyro_250dps)
{
    setup();
    /* At 250DPS range, sensitivity = 8.75 mdps/LSB
     * For 250 dps: 250000 mdps / 8.75 mdps/LSB ~ 28571 raw */
    int16_t raw = 28571;
    plant_gyro(raw, 0, 0);

    lsm6dso_gyro_data_t g;
    ASSERT_EQ(lsm6dso_read_gyro(&dev, &g), LSM6DSO_OK);
    ASSERT_NEAR(g.x_dps, 250.0f, 1.0f);
    ASSERT_NEAR(g.y_dps, 0.0f, 0.01f);
    ASSERT_NEAR(g.z_dps, 0.0f, 0.01f);
}

TEST(test_read_gyro_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    lsm6dso_gyro_data_t g;
    ASSERT_EQ(lsm6dso_read_gyro(&dev, &g), LSM6DSO_ERR_I2C);
}

/* ---------- temperature tests ---------- */

TEST(test_read_temp_25c)
{
    setup();
    plant_temp(0);  /* raw=0 -> 0/256 + 25 = 25.0C */

    float temp;
    ASSERT_EQ(lsm6dso_read_temp(&dev, &temp), LSM6DSO_OK);
    ASSERT_NEAR(temp, 25.0f, 0.01f);
}

TEST(test_read_temp_0c)
{
    setup();
    /* 0C -> raw = (0 - 25) * 256 = -6400 */
    plant_temp(-6400);

    float temp;
    ASSERT_EQ(lsm6dso_read_temp(&dev, &temp), LSM6DSO_OK);
    ASSERT_NEAR(temp, 0.0f, 0.01f);
}

TEST(test_read_temp_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    float temp;
    ASSERT_EQ(lsm6dso_read_temp(&dev, &temp), LSM6DSO_ERR_I2C);
}

/* ---------- range sensitivity tests ---------- */

TEST(test_accel_range_4g)
{
    mock_reset();
    lsm6dso_cfg_t cfg = { .addr = LSM6DSO_ADDR_LOW,
                           .accel_range = LSM6DSO_ACCEL_4G,
                           .gyro_range = LSM6DSO_GYRO_250DPS,
                           .odr = LSM6DSO_ODR_104HZ };
    lsm6dso_t d;
    lsm6dso_init(&d, &mock_i2c, &cfg);
    /* 1000 mg / 0.122 mg/LSB ~ 8197 raw */
    plant_accel(8197, 0, 0);
    lsm6dso_accel_data_t a;
    lsm6dso_read_accel(&d, &a);
    ASSERT_NEAR(a.x_mg, 1000.0f, 2.0f);
}

TEST(test_accel_range_16g)
{
    mock_reset();
    lsm6dso_cfg_t cfg = { .addr = LSM6DSO_ADDR_LOW,
                           .accel_range = LSM6DSO_ACCEL_16G,
                           .gyro_range = LSM6DSO_GYRO_250DPS,
                           .odr = LSM6DSO_ODR_104HZ };
    lsm6dso_t d;
    lsm6dso_init(&d, &mock_i2c, &cfg);
    /* 1000 mg / 0.488 mg/LSB ~ 2049 raw */
    plant_accel(2049, 0, 0);
    lsm6dso_accel_data_t a;
    lsm6dso_read_accel(&d, &a);
    ASSERT_NEAR(a.x_mg, 1000.0f, 2.0f);
}

TEST(test_gyro_range_2000dps)
{
    mock_reset();
    lsm6dso_cfg_t cfg = { .addr = LSM6DSO_ADDR_LOW,
                           .accel_range = LSM6DSO_ACCEL_2G,
                           .gyro_range = LSM6DSO_GYRO_2000DPS,
                           .odr = LSM6DSO_ODR_104HZ };
    lsm6dso_t d;
    lsm6dso_init(&d, &mock_i2c, &cfg);
    /* 100 dps at 70.0 mdps/LSB: 100*1000/70 ~ 1429 raw */
    int16_t raw = 1429;
    plant_gyro(raw, 0, 0);
    lsm6dso_gyro_data_t g;
    lsm6dso_read_gyro(&d, &g);
    ASSERT_NEAR(g.x_dps, 100.0f, 1.0f);
}

TEST(test_gyro_range_125dps)
{
    mock_reset();
    lsm6dso_cfg_t cfg = { .addr = LSM6DSO_ADDR_LOW,
                           .accel_range = LSM6DSO_ACCEL_2G,
                           .gyro_range = LSM6DSO_GYRO_125DPS,
                           .odr = LSM6DSO_ODR_104HZ };
    lsm6dso_t d;
    lsm6dso_init(&d, &mock_i2c, &cfg);
    /* 10 dps at 4.375 mdps/LSB: 10*1000/4.375 ~ 2286 raw */
    int16_t raw = 2286;
    plant_gyro(raw, 0, 0);
    lsm6dso_gyro_data_t g;
    lsm6dso_read_gyro(&d, &g);
    ASSERT_NEAR(g.x_dps, 10.0f, 0.1f);
}

/* ---------- address tests ---------- */

TEST(test_addr_high)
{
    mock_reset();
    lsm6dso_cfg_t cfg = { .addr = LSM6DSO_ADDR_HIGH,
                           .accel_range = LSM6DSO_ACCEL_2G,
                           .gyro_range = LSM6DSO_GYRO_250DPS,
                           .odr = LSM6DSO_ODR_104HZ };
    lsm6dso_t d;
    ASSERT_EQ(lsm6dso_init(&d, &mock_i2c, &cfg), LSM6DSO_OK);
}

/* ---------- reset test ---------- */

TEST(test_reset)
{
    setup();
    ASSERT_EQ(lsm6dso_reset(&dev), LSM6DSO_OK);
    /* CTRL3_C should have SW_RESET bit set */
    ASSERT_EQ(mock_regs[0x12], 0x01);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(lsm6dso_reset(&dev), LSM6DSO_ERR_I2C);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)LSM6DSO_ERR_I2C);
    ASSERT_EQ(-2, (int)LSM6DSO_ERR_ID);
    ASSERT_EQ(-3, (int)LSM6DSO_ERR_PARAM);
}

TEST_MAIN("lsm6dso")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_init_configures_ctrl1_xl);
    RUN(test_init_configures_ctrl2_g);
    RUN(test_read_accel_1g_z);
    RUN(test_read_accel_negative);
    RUN(test_read_accel_i2c_fail);
    RUN(test_read_gyro_250dps);
    RUN(test_read_gyro_i2c_fail);
    RUN(test_read_temp_25c);
    RUN(test_read_temp_0c);
    RUN(test_read_temp_i2c_fail);
    RUN(test_accel_range_4g);
    RUN(test_accel_range_16g);
    RUN(test_gyro_range_2000dps);
    RUN(test_gyro_range_125dps);
    RUN(test_addr_high);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_error_codes);
TEST_MAIN_END()
