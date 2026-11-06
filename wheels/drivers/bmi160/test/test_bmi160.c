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

TEST(test_init_refuses_missing_delay)
{
    /* The chip drops a CMD write made before the last command ends (p.85) */
    mock_reset();
    bmi160_i2c_t no_delay = mock_hal;
    no_delay.delay_ms = NULL;
    bmi160_t d;
    ASSERT_EQ(bmi160_init(&d, &no_delay, &default_cfg), BMI160_ERR_PARAM);
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
    plant_accel(0, 0, 16384);  /* manual p.8: 16384 LSB/g at +/-2g */
    plant_gyro(0, 0, 0);

    bmi160_data_t data;
    ASSERT_EQ(bmi160_read(&dev, &data), BMI160_OK);
    ASSERT_NEAR(data.accel_mg[0], 0.0f, 0.01f);
    ASSERT_NEAR(data.accel_mg[1], 0.0f, 0.01f);
    ASSERT_NEAR(data.accel_mg[2], 1000.0f, 0.01f);   /* 1g = 1000 mg */
}

TEST(test_read_accel_negative)
{
    setup();
    plant_accel(-16384, 0, 0);
    plant_gyro(0, 0, 0);

    bmi160_data_t data;
    bmi160_read(&dev, &data);
    ASSERT_NEAR(data.accel_mg[0], -1000.0f, 0.01f);
}

/* ---------- read tests: gyro ---------- */

TEST(test_read_gyro_known_value)
{
    setup();
    plant_accel(0, 0, 0);
    /* Manual p.61: 131.2 LSB/dps at +/-250 dps, so 100 dps = 13120 */
    plant_gyro(13120, 0, 0);

    bmi160_data_t data;
    ASSERT_EQ(bmi160_read(&dev, &data), BMI160_OK);
    ASSERT_NEAR(data.gyro_dps[0], 100.0f, 0.01f);
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
    /* Manual p.8: 8192 LSB/g at +/-4g */
    plant_accel(8192, 0, 0);
    plant_gyro(0, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.accel_mg[0], 1000.0f, 0.01f);
}

TEST(test_accel_range_8g)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_8G,
                         .gyro_range = BMI160_GYRO_250DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* Manual p.8: 4096 LSB/g at +/-8g */
    plant_accel(4096, 0, 0);
    plant_gyro(0, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.accel_mg[0], 1000.0f, 0.01f);
}

TEST(test_accel_range_16g)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_16G,
                         .gyro_range = BMI160_GYRO_250DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* Manual p.8: 2048 LSB/g at +/-16g */
    plant_accel(2048, 0, 0);
    plant_gyro(0, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.accel_mg[0], 1000.0f, 0.01f);
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
    /* Manual p.61: 262.4 LSB/dps, so 100 dps = 26240 */
    plant_accel(0, 0, 0);
    plant_gyro(26240, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.gyro_dps[0], 100.0f, 0.01f);
}

TEST(test_gyro_range_500dps)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_2G,
                         .gyro_range = BMI160_GYRO_500DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* Manual p.61: 65.6 LSB/dps, so 100 dps = 6560 */
    plant_accel(0, 0, 0);
    plant_gyro(6560, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.gyro_dps[0], 100.0f, 0.01f);
}

TEST(test_gyro_range_1000dps)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_2G,
                         .gyro_range = BMI160_GYRO_1000DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* Manual p.61: 32.8 LSB/dps, so 100 dps = 3280 */
    plant_accel(0, 0, 0);
    plant_gyro(3280, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.gyro_dps[0], 100.0f, 0.01f);
}

TEST(test_gyro_range_2000dps)
{
    mock_reset();
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_2G,
                         .gyro_range = BMI160_GYRO_2000DPS };
    bmi160_t d;
    bmi160_init(&d, &mock_hal, &cfg);
    /* Manual p.61: 16.4 LSB/dps, so 100 dps = 1640 */
    plant_accel(0, 0, 0);
    plant_gyro(1640, 0, 0);

    bmi160_data_t data;
    bmi160_read(&d, &data);
    ASSERT_NEAR(data.gyro_dps[0], 100.0f, 0.01f);
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

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(bmi160_t *d)
{
    bmi160_data_t data;
    ASSERT_EQ(bmi160_read(d, &data), BMI160_ERR_PARAM);
    ASSERT_EQ(bmi160_reset(d), BMI160_ERR_PARAM);
}

/* 0x24 is the ID of a BMI270: it answers, but no sensitivity was ever set */
TEST(test_device_refused_after_wrong_id)
{
    bmi160_t d;
    mock_reset();
    mock_regs[0x00] = 0x24;
    ASSERT_EQ(bmi160_init(&d, &mock_hal, &default_cfg), BMI160_ERR_ID);
    assert_device_refused(&d);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const bmi160_i2c_t no_callbacks;
    bmi160_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(bmi160_init(&d, NULL, &default_cfg), BMI160_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(bmi160_init(&d, &no_callbacks, &default_cfg), BMI160_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("bmi160")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_init_refuses_missing_delay);
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
    RUN(test_device_refused_after_wrong_id);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
