// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "bma400.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x00] = 0x90;  /* chip ID */
    mock_regs[0x1A] = 0x49;  /* POR: 4g, OSR 0, 200 Hz */
}

static const bma400_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* ---------- helpers ---------- */

static bma400_t dev;
static const bma400_cfg_t default_cfg = {
    .addr  = BMA400_ADDR_LOW,
    .range = BMA400_RANGE_2G,
    .power = BMA400_POWER_NORMAL,
};

static void setup(void)
{
    mock_reset();
    bma400_init(&dev, &mock_i2c, &default_cfg);
    mock_op_count = 0;
}

/*
 * Plant raw 12-bit signed accel values into mock registers.
 * BMA400 format: lo=full 8 bits, hi=lower 4 bits (bits 11:8).
 * Negative values use 12-bit two's complement (bit 11 = sign).
 */
static void plant_accel(int16_t x, int16_t y, int16_t z)
{
    mock_regs[0x04] = (uint8_t)(x & 0xFF);
    mock_regs[0x05] = (uint8_t)((x >> 8) & 0x0F);
    mock_regs[0x06] = (uint8_t)(y & 0xFF);
    mock_regs[0x07] = (uint8_t)((y >> 8) & 0x0F);
    mock_regs[0x08] = (uint8_t)(z & 0xFF);
    mock_regs[0x09] = (uint8_t)((z >> 8) & 0x0F);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    bma400_t d;
    ASSERT_EQ(bma400_init(&d, &mock_i2c, &default_cfg), BMA400_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x00] = 0xFF;
    bma400_t d;
    ASSERT_EQ(bma400_init(&d, &mock_i2c, &default_cfg), BMA400_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    bma400_t d;
    ASSERT_EQ(bma400_init(&d, &mock_i2c, &default_cfg), BMA400_ERR_I2C);
}

TEST(test_null_safety)
{
    bma400_t d;
    ASSERT_EQ(bma400_init(NULL, &mock_i2c, &default_cfg), BMA400_ERR_PARAM);
    ASSERT_EQ(bma400_init(&d, NULL, &default_cfg), BMA400_ERR_PARAM);
    ASSERT_EQ(bma400_init(&d, &mock_i2c, NULL), BMA400_ERR_PARAM);
    bma400_data_t data;
    ASSERT_EQ(bma400_read(NULL, &data), BMA400_ERR_PARAM);
    ASSERT_EQ(bma400_set_power(NULL, BMA400_POWER_NORMAL), BMA400_ERR_PARAM);
    ASSERT_EQ(bma400_set_range(NULL, BMA400_RANGE_4G), BMA400_ERR_PARAM);
}

TEST(test_read_null_out)
{
    setup();
    ASSERT_EQ(bma400_read(&dev, NULL), BMA400_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    /* Zero HAL struct has NULL function pointers -- init must reject it */
    bma400_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    bma400_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(bma400_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)BMA400_ERR_I2C);
    ASSERT_EQ(-2, (int)BMA400_ERR_ID);
    ASSERT_EQ(-3, (int)BMA400_ERR_PARAM);
}

/* ---------- read tests ---------- */

TEST(test_read_1g_z)
{
    setup();
    /* At +/-2g: 1g = 1024 LSB. 1024 mg expected. */
    plant_accel(0, 0, 1024);
    bma400_data_t data;
    ASSERT_EQ(bma400_read(&dev, &data), BMA400_OK);
    ASSERT_NEAR(data.x_mg, 0.0f, 1.0f);
    ASSERT_NEAR(data.y_mg, 0.0f, 1.0f);
    ASSERT_NEAR(data.z_mg, 1000.0f, 1.0f);
}

TEST(test_read_negative)
{
    setup();
    /* -1g on X at 2g range: raw = -1024 (0xFC00 in 12-bit 2's complement) */
    int16_t neg_1g = -1024;
    plant_accel(neg_1g, 0, 0);
    bma400_data_t data;
    bma400_read(&dev, &data);
    ASSERT_NEAR(data.x_mg, -1000.0f, 1.0f);
}

TEST(test_read_known_values)
{
    setup();
    /* 512 LSB at 2g range = 500 mg */
    plant_accel(512, -512, 256);
    bma400_data_t data;
    ASSERT_EQ(bma400_read(&dev, &data), BMA400_OK);
    ASSERT_NEAR(data.x_mg, 500.0f, 1.0f);
    ASSERT_NEAR(data.y_mg, -500.0f, 1.0f);
    ASSERT_NEAR(data.z_mg, 250.0f, 1.0f);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bma400_data_t data;
    ASSERT_EQ(bma400_read(&dev, &data), BMA400_ERR_I2C);
}

/* ---------- range configuration tests ---------- */

TEST(test_range_4g)
{
    mock_reset();
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW,
                         .range = BMA400_RANGE_4G,
                         .power = BMA400_POWER_NORMAL };
    bma400_t d;
    bma400_init(&d, &mock_i2c, &cfg);
    mock_op_count = 0;
    /* At +/-4g: 1g = 512 LSB. 512 raw -> 1000 mg */
    plant_accel(512, 0, 0);
    bma400_data_t data;
    bma400_read(&d, &data);
    ASSERT_NEAR(data.x_mg, 1000.0f, 2.0f);
}

TEST(test_range_8g)
{
    mock_reset();
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW,
                         .range = BMA400_RANGE_8G,
                         .power = BMA400_POWER_NORMAL };
    bma400_t d;
    bma400_init(&d, &mock_i2c, &cfg);
    mock_op_count = 0;
    /* At +/-8g: 1g = 256 LSB. 256 raw -> 1000 mg */
    plant_accel(256, 0, 0);
    bma400_data_t data;
    bma400_read(&d, &data);
    ASSERT_NEAR(data.x_mg, 1000.0f, 4.0f);
}

TEST(test_range_16g)
{
    mock_reset();
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW,
                         .range = BMA400_RANGE_16G,
                         .power = BMA400_POWER_NORMAL };
    bma400_t d;
    bma400_init(&d, &mock_i2c, &cfg);
    mock_op_count = 0;
    /* At +/-16g: 1g = 128 LSB. 128 raw -> 1000 mg */
    plant_accel(128, 0, 0);
    bma400_data_t data;
    bma400_read(&d, &data);
    ASSERT_NEAR(data.x_mg, 1000.0f, 8.0f);
}

TEST(test_set_range_runtime)
{
    setup();
    ASSERT_EQ(bma400_set_range(&dev, BMA400_RANGE_8G), BMA400_OK);
    /* Verify ACC_CONFIG1 was written: range=8g -> bits [7:6]=0b10, ODR kept */
    ASSERT_EQ(mock_regs[0x1A], 0x89);

    /* At +/-8g: 1g = 256 LSB */
    plant_accel(256, 0, 0);
    bma400_data_t data;
    bma400_read(&dev, &data);
    ASSERT_NEAR(data.x_mg, 1000.0f, 4.0f);
}

TEST(test_set_range_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(bma400_set_range(&dev, BMA400_RANGE_4G), BMA400_ERR_I2C);
}

TEST(test_invalid_range)
{
    mock_reset();
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW,
                         .range = (bma400_range_t)4,
                         .power = BMA400_POWER_NORMAL };
    bma400_t d;
    ASSERT_EQ(bma400_init(&d, &mock_i2c, &cfg), BMA400_ERR_PARAM);

    /* Also via set_range */
    setup();
    ASSERT_EQ(bma400_set_range(&dev, (bma400_range_t)99), BMA400_ERR_PARAM);
}

/* ---------- power mode tests ---------- */

TEST(test_power_mode_sleep)
{
    mock_reset();
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW,
                         .range = BMA400_RANGE_2G,
                         .power = BMA400_POWER_SLEEP };
    bma400_t d;
    ASSERT_EQ(bma400_init(&d, &mock_i2c, &cfg), BMA400_OK);
    /* ACC_CONFIG0 bits [1:0] = 0 (sleep) */
    ASSERT_EQ(mock_regs[0x19] & 0x03, 0x00);
}

TEST(test_power_mode_lowpower)
{
    mock_reset();
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW,
                         .range = BMA400_RANGE_2G,
                         .power = BMA400_POWER_LOW_POWER };
    bma400_t d;
    ASSERT_EQ(bma400_init(&d, &mock_i2c, &cfg), BMA400_OK);
    ASSERT_EQ(mock_regs[0x19] & 0x03, 0x01);
}

TEST(test_power_mode_normal)
{
    setup();
    /* Default config: normal mode */
    ASSERT_EQ(mock_regs[0x19] & 0x03, 0x02);
}

TEST(test_set_power_runtime)
{
    setup();
    ASSERT_EQ(bma400_set_power(&dev, BMA400_POWER_SLEEP), BMA400_OK);
    ASSERT_EQ(mock_regs[0x19] & 0x03, 0x00);
    ASSERT_EQ(bma400_set_power(&dev, BMA400_POWER_NORMAL), BMA400_OK);
    ASSERT_EQ(mock_regs[0x19] & 0x03, 0x02);
}

TEST(test_set_power_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(bma400_set_power(&dev, BMA400_POWER_SLEEP), BMA400_ERR_I2C);
}

TEST(test_invalid_power_mode)
{
    mock_reset();
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW,
                         .range = BMA400_RANGE_2G,
                         .power = (bma400_power_t)3 };
    bma400_t d;
    ASSERT_EQ(bma400_init(&d, &mock_i2c, &cfg), BMA400_ERR_PARAM);

    setup();
    ASSERT_EQ(bma400_set_power(&dev, (bma400_power_t)99), BMA400_ERR_PARAM);
}

/* ---------- address tests ---------- */

TEST(test_addr_high)
{
    mock_reset();
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_HIGH,
                         .range = BMA400_RANGE_2G,
                         .power = BMA400_POWER_NORMAL };
    bma400_t d;
    ASSERT_EQ(bma400_init(&d, &mock_i2c, &cfg), BMA400_OK);
}

/* ---------- config register verification ---------- */

TEST(test_init_config_registers)
{
    mock_reset();
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW,
                         .range = BMA400_RANGE_16G,
                         .power = BMA400_POWER_LOW_POWER };
    bma400_t d;
    bma400_init(&d, &mock_i2c, &cfg);
    /* ACC_CONFIG0: power=low_power(1) -> bits [1:0]=0x01 */
    ASSERT_EQ(mock_regs[0x19] & 0x03, 0x01);
    /* ACC_CONFIG1: range=16g(3) -> bits [7:6]=0b11 -> 0xC0 */
    ASSERT_EQ(mock_regs[0x1A] & 0xC0, 0xC0);
}

/* ---------- 12-bit sign extension edge cases ---------- */

TEST(test_12bit_max_positive)
{
    setup();
    /* Max positive 12-bit: 0x7FF = 2047 */
    plant_accel(2047, 0, 0);
    bma400_data_t data;
    bma400_read(&dev, &data);
    /* 2047/1024*1000 = 1999.02 mg */
    ASSERT_NEAR(data.x_mg, 1999.0f, 2.0f);
}

TEST(test_12bit_max_negative)
{
    setup();
    /* Max negative 12-bit: -2048 (0xF800 sign-extended) */
    plant_accel(-2048, 0, 0);
    bma400_data_t data;
    bma400_read(&dev, &data);
    /* -2048/1024*1000 = -2000.0 mg */
    ASSERT_NEAR(data.x_mg, -2000.0f, 1.0f);
}

TEST(test_12bit_minus_one)
{
    setup();
    /* -1 in 12-bit = 0xFFF. lo=0xFF, hi=0x0F */
    plant_accel(-1, 0, 0);
    bma400_data_t data;
    bma400_read(&dev, &data);
    /* -1/1024*1000 ~ -0.977 mg */
    ASSERT_NEAR(data.x_mg, -0.977f, 0.1f);
}

static int write_order[2];
static int write_count;

static bool record_config_writes(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
{
    if ((reg == 0x19 || reg == 0x1A) && write_count < 2) {
        write_order[write_count++] = reg;
    }
    return mock_i2c_write_reg(ctx, addr, reg, val);
}

static bool drop_config1_write(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
{
    if (reg == 0x1A) {
        return true;
    }
    return mock_i2c_write_reg(ctx, addr, reg, val);
}

TEST(test_init_keeps_odr)
{
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW, .range = BMA400_RANGE_8G,
                         .power = BMA400_POWER_NORMAL };
    mock_reset();
    ASSERT_EQ(bma400_init(&dev, &mock_i2c, &cfg), BMA400_OK);

    /* POR value 0x49 is range 4g, OSR 0, ODR 200 Hz. Only the range may change. */
    ASSERT_EQ(mock_regs[0x1A], 0x89);
}

TEST(test_set_range_keeps_odr)
{
    setup();
    ASSERT_EQ(bma400_set_range(&dev, BMA400_RANGE_16G), BMA400_OK);
    ASSERT_EQ(mock_regs[0x1A] & 0x3F, 0x09);
}

TEST(test_init_sets_range_before_power)
{
    static const bma400_i2c_t i2c = {
        .write_reg = record_config_writes,
        .read_regs = mock_i2c_read_regs,
        .delay_ms  = NULL,
        .ctx       = NULL,
    };
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW, .range = BMA400_RANGE_2G,
                         .power = BMA400_POWER_LOW_POWER };
    mock_reset();
    write_count = 0;

    ASSERT_EQ(bma400_init(&dev, &i2c, &cfg), BMA400_OK);
    ASSERT_EQ(write_order[0], 0x1A);
    ASSERT_EQ(write_order[1], 0x19);
}

TEST(test_set_range_detects_dropped_write)
{
    static const bma400_i2c_t i2c = {
        .write_reg = drop_config1_write,
        .read_regs = mock_i2c_read_regs,
        .delay_ms  = NULL,
        .ctx       = NULL,
    };
    setup();
    float before = dev.sensitivity;
    dev.i2c = &i2c;

    ASSERT_EQ(bma400_set_range(&dev, BMA400_RANGE_16G), BMA400_ERR_I2C);
    ASSERT_TRUE(dev.sensitivity == before);
}

TEST(test_read_in_sleep_is_rejected)
{
    bma400_data_t data;
    setup();
    ASSERT_EQ(bma400_set_power(&dev, BMA400_POWER_SLEEP), BMA400_OK);
    ASSERT_EQ(bma400_read(&dev, &data), BMA400_ERR_PARAM);
}

TEST(test_read_after_failed_init_is_rejected)
{
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW, .range = BMA400_RANGE_2G,
                         .power = BMA400_POWER_NORMAL };
    bma400_data_t data;
    bma400_t d;
    mock_reset();
    mock_regs[0x00] = 0x00; /* wrong chip ID */

    ASSERT_EQ(bma400_init(&d, &mock_i2c, &cfg), BMA400_ERR_ID);
    ASSERT_EQ(bma400_read(&d, &data), BMA400_ERR_PARAM);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(bma400_t *d)
{
    bma400_data_t data;
    ASSERT_EQ(bma400_read(d, &data), BMA400_ERR_PARAM);
    ASSERT_EQ(bma400_set_power(d, BMA400_POWER_NORMAL), BMA400_ERR_PARAM);
    ASSERT_EQ(bma400_set_range(d, BMA400_RANGE_4G), BMA400_ERR_PARAM);
}

/* The read was refused for its sensitivity, but the setters had no such check */
TEST(test_device_refused_after_wrong_id)
{
    bma400_t d;
    mock_reset();
    mock_regs[0x00] = 0x91;
    ASSERT_EQ(bma400_init(&d, &mock_i2c, &default_cfg), BMA400_ERR_ID);
    assert_device_refused(&d);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const bma400_i2c_t no_callbacks;
    bma400_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(bma400_init(&d, NULL, &default_cfg), BMA400_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(bma400_init(&d, &no_callbacks, &default_cfg), BMA400_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("bma400")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_null_out);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_read_1g_z);
    RUN(test_read_negative);
    RUN(test_read_known_values);
    RUN(test_read_i2c_fail);
    RUN(test_range_4g);
    RUN(test_range_8g);
    RUN(test_range_16g);
    RUN(test_set_range_runtime);
    RUN(test_set_range_i2c_fail);
    RUN(test_invalid_range);
    RUN(test_power_mode_sleep);
    RUN(test_power_mode_lowpower);
    RUN(test_power_mode_normal);
    RUN(test_set_power_runtime);
    RUN(test_set_power_i2c_fail);
    RUN(test_invalid_power_mode);
    RUN(test_addr_high);
    RUN(test_init_config_registers);
    RUN(test_12bit_max_positive);
    RUN(test_12bit_max_negative);
    RUN(test_12bit_minus_one);
    RUN(test_init_keeps_odr);
    RUN(test_set_range_keeps_odr);
    RUN(test_init_sets_range_before_power);
    RUN(test_set_range_detects_dropped_write);
    RUN(test_read_in_sleep_is_rejected);
    RUN(test_read_after_failed_init_is_rejected);
    RUN(test_device_refused_after_wrong_id);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
