// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "bmi160.h"
#include <math.h>

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x00] = 0xD1;   /* chip ID */
    mock_regs[0x03] = 0x00;   /* PMU_STATUS: all suspended */
    mock_regs[0x41] = 0x03;   /* ACC_RANGE: +/-2g */
    mock_regs[0x43] = 0x00;   /* GYR_RANGE: +/-2000 dps */
}

static const bmi160_i2c_t mock_hal = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* ---------- helpers ---------- */
static bmi160_t dev;
static const bmi160_cfg_t default_cfg = {
    .addr        = BMI160_ADDR_LOW,
    .accel_range = BMI160_ACCEL_2G,
    .gyro_range  = BMI160_GYRO_250DPS,
};

static void setup(void)
{
    mock_reset();
    bmi160_init(&dev, &mock_hal, &default_cfg);
    mock_op_count = 0;
}

/* Plant little-endian accel data at 0x12..0x17 */
static void plant_accel(int16_t x, int16_t y, int16_t z)
{
    mock_regs[0x12] = (uint8_t)((uint16_t)x & 0xFF);
    mock_regs[0x13] = (uint8_t)((uint16_t)x >> 8);
    mock_regs[0x14] = (uint8_t)((uint16_t)y & 0xFF);
    mock_regs[0x15] = (uint8_t)((uint16_t)y >> 8);
    mock_regs[0x16] = (uint8_t)((uint16_t)z & 0xFF);
    mock_regs[0x17] = (uint8_t)((uint16_t)z >> 8);
}

/* Plant little-endian gyro data at 0x0C..0x11 */
static void plant_gyro(int16_t x, int16_t y, int16_t z)
{
    mock_regs[0x0C] = (uint8_t)((uint16_t)x & 0xFF);
    mock_regs[0x0D] = (uint8_t)((uint16_t)x >> 8);
    mock_regs[0x0E] = (uint8_t)((uint16_t)y & 0xFF);
    mock_regs[0x0F] = (uint8_t)((uint16_t)y >> 8);
    mock_regs[0x10] = (uint8_t)((uint16_t)z & 0xFF);
    mock_regs[0x11] = (uint8_t)((uint16_t)z >> 8);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    bmi160_t d;
    ASSERT_EQ(bmi160_init(&d, &mock_hal, &default_cfg), BMI160_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x00] = 0x00;
    bmi160_t d;
    ASSERT_EQ(bmi160_init(&d, &mock_hal, &default_cfg), BMI160_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    bmi160_t d;
    ASSERT_EQ(bmi160_init(&d, &mock_hal, &default_cfg), BMI160_ERR_I2C);
}

/* ---------- NULL safety ---------- */

TEST(test_null_safety)
{
    ASSERT_EQ(bmi160_init(NULL, &mock_hal, &default_cfg), BMI160_ERR_PARAM);
    bmi160_t d;
    ASSERT_EQ(bmi160_init(&d, NULL, &default_cfg), BMI160_ERR_PARAM);
    ASSERT_EQ(bmi160_init(&d, &mock_hal, NULL), BMI160_ERR_PARAM);

    bmi160_data_t data;
    ASSERT_EQ(bmi160_read(NULL, &data), BMI160_ERR_PARAM);
    setup();
    ASSERT_EQ(bmi160_read(&dev, NULL), BMI160_ERR_PARAM);

    ASSERT_EQ(bmi160_reset(NULL), BMI160_ERR_PARAM);
}

TEST(test_zero_hal_safety)
{
    bmi160_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    bmi160_t d;
    ASSERT_TRUE(bmi160_init(&d, &zero_hal, &default_cfg) < 0);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)BMI160_OK,        0);
    ASSERT_EQ((int)BMI160_ERR_I2C,  -1);
    ASSERT_EQ((int)BMI160_ERR_ID,   -2);
    ASSERT_EQ((int)BMI160_ERR_PARAM,-3);
}

/* ---------- read tests: accel ---------- */

TEST(test_read_accel_1g_z)
{
    setup();
    plant_accel(0, 0, 16393);  /* 1g / 0.061 mg/LSB = 16393 */
    plant_gyro(0, 0, 0);

    bmi160_data_t data;
    ASSERT_EQ(bmi160_read(&dev, &data), BMI160_OK);
    ASSERT_NEAR(data.accel_mg[0], 0.0f, 1.0f);
    ASSERT_NEAR(data.accel_mg[1], 0.0f, 1.0f);
    ASSERT_NEAR(data.accel_mg[2], 1000.0f, 1.0f);   /* 1g = 1000 mg */
}

TEST(test_read_accel_negative)
{
    setup();
    plant_accel(-16393, 0, 0);
    plant_gyro(0, 0, 0);

    bmi160_data_t data;
    bmi160_read(&dev, &data);
    ASSERT_NEAR(data.accel_mg[0], -1000.0f, 1.0f);
}

/* ---------- read tests: gyro ---------- */

TEST(test_read_gyro_known_value)
{
    setup();
    plant_accel(0, 0, 0);
    /* At +/-250 dps: sensitivity = 0.00763 dps/LSB
     * For 1 dps: raw = 1 / 0.00763 = ~131.1 -> use 131 */
    plant_gyro(131, 0, 0);

    bmi160_data_t data;
    ASSERT_EQ(bmi160_read(&dev, &data), BMI160_OK);
    ASSERT_NEAR(data.gyro_dps[0], 1.0f, 0.01f);
    ASSERT_NEAR(data.gyro_dps[1], 0.0f, 0.01f);
    ASSERT_NEAR(data.gyro_dps[2], 0.0f, 0.01f);
}

/* ---------- accel range tests ---------- */

TEST(test_accel_range_4g)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_4G,
                         .gyro_range = BMI160_GYRO_250DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* 1g at 4g range: raw = 1000 / 0.122 = 8197 */
    plant_accel(8197, 0, 0);
    plant_gyro(0, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.accel_mg[0], 1000.0f, 1.0f);
}

TEST(test_accel_range_8g)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_8G,
                         .gyro_range = BMI160_GYRO_250DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* 1g at 8g range: raw = 1000 / 0.244 = 4098 */
    plant_accel(4098, 0, 0);
    plant_gyro(0, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.accel_mg[0], 1000.0f, 1.0f);
}

TEST(test_accel_range_16g)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_16G,
                         .gyro_range = BMI160_GYRO_250DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* 1g at 16g range: raw = 1000 / 0.488 = 2049 */
    plant_accel(2049, 0, 0);
    plant_gyro(0, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.accel_mg[0], 1000.0f, 1.0f);
}

/* ---------- gyro range tests ---------- */

TEST(test_gyro_range_125dps)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_2G,
                         .gyro_range = BMI160_GYRO_125DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* 10 dps at 125 range: raw = 10 / 0.00381 = 2625 */
    plant_accel(0, 0, 0);
    plant_gyro(2625, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.gyro_dps[0], 10.0f, 0.1f);
}

TEST(test_gyro_range_500dps)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_2G,
                         .gyro_range = BMI160_GYRO_500DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* 10 dps at 500 range: raw = 10 / 0.01526 = 655 */
    plant_accel(0, 0, 0);
    plant_gyro(655, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.gyro_dps[0], 10.0f, 0.1f);
}

TEST(test_gyro_range_1000dps)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_2G,
                         .gyro_range = BMI160_GYRO_1000DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* 10 dps at 1000 range: raw = 10 / 0.03052 = 328 */
    plant_accel(0, 0, 0);
    plant_gyro(328, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.gyro_dps[0], 10.0f, 0.1f);
}

TEST(test_gyro_range_2000dps)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_2G,
                         .gyro_range = BMI160_GYRO_2000DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* 10 dps at 2000 range: raw = 10 / 0.06104 = 164 */
    plant_accel(0, 0, 0);
    plant_gyro(164, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.gyro_dps[0], 10.0f, 0.1f);
}

/* ---------- read I2C failure ---------- */

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bmi160_data_t data;
    ASSERT_EQ(bmi160_read(&dev, &data), BMI160_ERR_I2C);
}

/* ---------- reset ---------- */

TEST(test_reset)
{
    setup();
    ASSERT_EQ(bmi160_reset(&dev), BMI160_OK);
    ASSERT_EQ(mock_regs[0x7E], 0xB6);
}

/* ---------- config register writes ---------- */

TEST(test_config_registers)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_8G,
                         .gyro_range = BMI160_GYRO_1000DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    ASSERT_EQ(mock_regs[0x41], (int)BMI160_ACCEL_8G);
    ASSERT_EQ(mock_regs[0x43], (int)BMI160_GYRO_1000DPS);
}

/* ---------- addr high ---------- */

TEST(test_addr_high)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_HIGH,
                         .accel_range = BMI160_ACCEL_2G,
                         .gyro_range = BMI160_GYRO_250DPS };
    bmi160_t d;
    ASSERT_EQ(bmi160_init(&d, &mock_hal, &cfg), BMI160_OK);
}

/* ---------- invalid range ---------- */

TEST(test_invalid_accel_range)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = (bmi160_accel_range_t)0xFF,
                         .gyro_range = BMI160_GYRO_250DPS };
    bmi160_t d;
    ASSERT_EQ(bmi160_init(&d, &mock_hal, &cfg), BMI160_ERR_PARAM);
}

TEST(test_invalid_gyro_range)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_2G,
                         .gyro_range = (bmi160_gyro_range_t)0xFF };
    bmi160_t d;
    ASSERT_EQ(bmi160_init(&d, &mock_hal, &cfg), BMI160_ERR_PARAM);
}

TEST_MAIN("bmi160")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_hal_safety);
    RUN(test_error_codes);
    RUN(test_read_accel_1g_z);
    RUN(test_read_accel_negative);
    RUN(test_read_gyro_known_value);
    RUN(test_accel_range_4g);
    RUN(test_accel_range_8g);
    RUN(test_accel_range_16g);
    RUN(test_gyro_range_125dps);
    RUN(test_gyro_range_500dps);
    RUN(test_gyro_range_1000dps);
    RUN(test_gyro_range_2000dps);
    RUN(test_read_i2c_fail);
    RUN(test_reset);
    RUN(test_config_registers);
    RUN(test_addr_high);
    RUN(test_invalid_accel_range);
    RUN(test_invalid_gyro_range);
TEST_MAIN_END()
