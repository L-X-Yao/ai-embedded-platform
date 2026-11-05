// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_time.h"
#include <string.h>
#include "bmi270.h"

/* ---------- mock SPI with BMI270 register model ---------- */

#define MOCK_REG_COUNT 256
static uint8_t mock_regs[MOCK_REG_COUNT];
static int     mock_fail_at;
static int     mock_op_count;
static uint8_t mock_cs_state;
static int     mock_byte_idx;
static bool    mock_is_read;
static uint8_t mock_addr_latch;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_fail_at   = -1;
    mock_op_count  = 0;
    mock_cs_state  = 1;
    mock_byte_idx  = 0;
    mock_is_read   = false;
    mock_addr_latch = 0;

    /* POR defaults */
    mock_regs[0x00] = 0x24;  /* CHIP_ID */
    mock_regs[0x21] = 0x01;  /* INTERNAL_STATUS: init OK */
    mock_regs[0x40] = 0xA8;  /* ACC_CONF */
    mock_regs[0x42] = 0xA9;  /* GYR_CONF */
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_cs_state = 0;
    mock_byte_idx = 0;
}

static void mock_cs_high(void *ctx) { (void)ctx; mock_cs_state = 1; }

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    mock_op_count++;

    if (mock_byte_idx == 0) {
        mock_is_read = (tx & 0x80u) != 0;
        mock_addr_latch = (uint8_t)(tx & 0x7Fu);
        mock_byte_idx++;
        return 0x00;
    }

    if (mock_is_read) {
        if (mock_byte_idx == 1) {
            /* Dummy byte */
            mock_byte_idx++;
            return 0x00;
        }
        uint8_t offset = (uint8_t)(mock_byte_idx - 2);
        uint8_t reg = (uint8_t)(mock_addr_latch + offset);
        mock_byte_idx++;
        return mock_regs[reg];
    } else {
        if (mock_byte_idx == 1) {
            mock_regs[mock_addr_latch] = tx;
        } else {
            uint8_t offset = (uint8_t)(mock_byte_idx - 1);
            uint8_t reg = (uint8_t)(mock_addr_latch + offset);
            mock_regs[reg] = tx;
        }
        mock_byte_idx++;
        return 0x00;
    }
}

static const bmi270_spi_t mock_spi = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

/* Helper: inject raw accel+gyro data into mock registers */
static void inject_accel_gyro(int16_t ax, int16_t ay, int16_t az,
                               int16_t gx, int16_t gy, int16_t gz)
{
    mock_regs[0x0C] = (uint8_t)(ax & 0xFFu);
    mock_regs[0x0D] = (uint8_t)((uint16_t)ax >> 8);
    mock_regs[0x0E] = (uint8_t)(ay & 0xFFu);
    mock_regs[0x0F] = (uint8_t)((uint16_t)ay >> 8);
    mock_regs[0x10] = (uint8_t)(az & 0xFFu);
    mock_regs[0x11] = (uint8_t)((uint16_t)az >> 8);
    mock_regs[0x12] = (uint8_t)(gx & 0xFFu);
    mock_regs[0x13] = (uint8_t)((uint16_t)gx >> 8);
    mock_regs[0x14] = (uint8_t)(gy & 0xFFu);
    mock_regs[0x15] = (uint8_t)((uint16_t)gy >> 8);
    mock_regs[0x16] = (uint8_t)(gz & 0xFFu);
    mock_regs[0x17] = (uint8_t)((uint16_t)gz >> 8);
}

static void inject_temp(int16_t raw)
{
    mock_regs[0x22] = (uint8_t)(raw & 0xFFu);
    mock_regs[0x23] = (uint8_t)((uint16_t)raw >> 8);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    ASSERT_EQ(bmi270_init(&dev, &mock_spi, &cfg), BMI270_OK);
}

TEST(test_init_bad_id)
{
    mock_reset();
    mock_regs[0x00] = 0x00;  /* wrong chip ID */
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    ASSERT_EQ(bmi270_init(&dev, &mock_spi, &cfg), BMI270_ERR_ID);
}

TEST(test_init_config_fail)
{
    mock_reset();
    mock_regs[0x21] = 0x00;  /* INTERNAL_STATUS: init NOT ok */
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    ASSERT_EQ(bmi270_init(&dev, &mock_spi, &cfg), BMI270_ERR_CONFIG);
}

TEST(test_null_safety)
{
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    ASSERT_EQ(bmi270_init(NULL, &mock_spi, &cfg), BMI270_ERR_PARAM);
    ASSERT_EQ(bmi270_init(&dev, NULL, &cfg), BMI270_ERR_PARAM);
    ASSERT_EQ(bmi270_init(&dev, &mock_spi, NULL), BMI270_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    bmi270_spi_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    bmi270_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    bmi270_cfg_t cfg = bmi270_default_cfg();
    ASSERT_TRUE(bmi270_init(&zero_dev, &zero_hal, &cfg) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)BMI270_ERR_PARAM);
    ASSERT_EQ(-2, (int)BMI270_ERR_SPI);
    ASSERT_EQ(-3, (int)BMI270_ERR_ID);
    ASSERT_EQ(-4, (int)BMI270_ERR_TIMEOUT);
    ASSERT_EQ(-5, (int)BMI270_ERR_CONFIG);
}

TEST(test_default_cfg)
{
    bmi270_cfg_t cfg = bmi270_default_cfg();
    ASSERT_EQ(cfg.acc_range, BMI270_ACC_RANGE_8G);
    ASSERT_EQ(cfg.gyro_range, BMI270_GYRO_RANGE_2000DPS);
    ASSERT_EQ(cfg.acc_odr, BMI270_ODR_100HZ);
    ASSERT_EQ(cfg.gyro_odr, BMI270_ODR_200HZ);
}

TEST(test_read_accel_gyro)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    cfg.acc_range = BMI270_ACC_RANGE_2G;    /* 16384 LSB/g */
    cfg.gyro_range = BMI270_GYRO_RANGE_2000DPS;  /* 16.4 LSB/dps */
    bmi270_init(&dev, &mock_spi, &cfg);

    /* Inject known raw values */
    /* AX = 16384 (1g), AY = -16384 (-1g), AZ = 0 */
    /* GX = 164 (~10 dps), GY = 0, GZ = -164 (~-10 dps) */
    inject_accel_gyro(16384, -16384, 0, 164, 0, -164);

    bmi270_accel_gyro_t data;
    ASSERT_EQ(bmi270_read_accel_gyro(&dev, &data), BMI270_OK);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.01f);
    ASSERT_NEAR(data.ay_g, -1.0f, 0.01f);
    ASSERT_NEAR(data.az_g, 0.0f, 0.01f);
    ASSERT_NEAR(data.gx_dps, 10.0f, 0.1f);
    ASSERT_NEAR(data.gy_dps, 0.0f, 0.01f);
    ASSERT_NEAR(data.gz_dps, -10.0f, 0.1f);
}

TEST(test_read_accel_gyro_8g)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    cfg.acc_range = BMI270_ACC_RANGE_8G;  /* 4096 LSB/g */
    bmi270_init(&dev, &mock_spi, &cfg);

    /* AX = 4096 (1g at 8g range) */
    inject_accel_gyro(4096, 0, 0, 0, 0, 0);

    bmi270_accel_gyro_t data;
    ASSERT_EQ(bmi270_read_accel_gyro(&dev, &data), BMI270_OK);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.01f);
}

TEST(test_read_accel_null)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &mock_spi, &cfg);
    ASSERT_EQ(bmi270_read_accel_gyro(NULL, &(bmi270_accel_gyro_t){0}), BMI270_ERR_PARAM);
    ASSERT_EQ(bmi270_read_accel_gyro(&dev, NULL), BMI270_ERR_PARAM);
}

TEST(test_read_temp)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &mock_spi, &cfg);

    /* Temperature: raw / 512 + 23.
     * raw = 5120 => 5120/512 + 23 = 33.0C */
    inject_temp(5120);

    float temp;
    ASSERT_EQ(bmi270_read_temp(&dev, &temp), BMI270_OK);
    ASSERT_NEAR(temp, 33.0f, 0.1f);
}

TEST(test_read_temp_negative)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &mock_spi, &cfg);

    /* raw = -5120 => -5120/512 + 23 = 13.0C */
    inject_temp(-5120);

    float temp;
    ASSERT_EQ(bmi270_read_temp(&dev, &temp), BMI270_OK);
    ASSERT_NEAR(temp, 13.0f, 0.1f);
}

TEST(test_read_temp_null)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &mock_spi, &cfg);
    ASSERT_EQ(bmi270_read_temp(NULL, &(float){0}), BMI270_ERR_PARAM);
    ASSERT_EQ(bmi270_read_temp(&dev, NULL), BMI270_ERR_PARAM);
}

TEST(test_read_temp_invalid)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &mock_spi, &cfg);

    /* 0x8000 means invalid reading */
    inject_temp((int16_t)0x8000);

    float temp;
    ASSERT_EQ(bmi270_read_temp(&dev, &temp), BMI270_ERR_SPI);
}

TEST(test_set_acc_odr)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &mock_spi, &cfg);

    ASSERT_EQ(bmi270_set_acc_odr(&dev, BMI270_ODR_400HZ), BMI270_OK);
    /* ACC_CONF lower nibble should be 0x0A */
    ASSERT_EQ(mock_regs[0x40] & 0x0Fu, 0x0A);
}

TEST(test_set_gyro_odr)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &mock_spi, &cfg);

    ASSERT_EQ(bmi270_set_gyro_odr(&dev, BMI270_ODR_800HZ), BMI270_OK);
    /* GYR_CONF lower nibble should be 0x0B */
    ASSERT_EQ(mock_regs[0x42] & 0x0Fu, 0x0B);
}

TEST(test_set_acc_range)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &mock_spi, &cfg);

    ASSERT_EQ(bmi270_set_acc_range(&dev, BMI270_ACC_RANGE_16G), BMI270_OK);
    ASSERT_EQ(mock_regs[0x41], 3);
    /* Verify divisor was updated */
    inject_accel_gyro(2048, 0, 0, 0, 0, 0);
    bmi270_accel_gyro_t data;
    bmi270_read_accel_gyro(&dev, &data);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.01f);
}

TEST(test_set_gyro_range)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &mock_spi, &cfg);

    ASSERT_EQ(bmi270_set_gyro_range(&dev, BMI270_GYRO_RANGE_250DPS), BMI270_OK);
    ASSERT_EQ(mock_regs[0x43], 3);
}

TEST(test_set_range_invalid)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &mock_spi, &cfg);

    ASSERT_EQ(bmi270_set_acc_range(&dev, (bmi270_acc_range_t)4), BMI270_ERR_PARAM);
    ASSERT_EQ(bmi270_set_gyro_range(&dev, (bmi270_gyro_range_t)5), BMI270_ERR_PARAM);
}

TEST(test_set_odr_null)
{
    ASSERT_EQ(bmi270_set_acc_odr(NULL, BMI270_ODR_100HZ), BMI270_ERR_PARAM);
    ASSERT_EQ(bmi270_set_gyro_odr(NULL, BMI270_ODR_100HZ), BMI270_ERR_PARAM);
}

TEST(test_set_range_null)
{
    ASSERT_EQ(bmi270_set_acc_range(NULL, BMI270_ACC_RANGE_2G), BMI270_ERR_PARAM);
    ASSERT_EQ(bmi270_set_gyro_range(NULL, BMI270_GYRO_RANGE_250DPS), BMI270_ERR_PARAM);
}

TEST(test_soft_reset)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    bmi270_init(&dev, &mock_spi, &cfg);

    ASSERT_EQ(bmi270_soft_reset(&dev), BMI270_OK);
    ASSERT_EQ(mock_regs[0x7E], 0xB6);  /* CMD register */
}

TEST(test_soft_reset_null)
{
    ASSERT_EQ(bmi270_soft_reset(NULL), BMI270_ERR_PARAM);
}

static uint32_t delay_total_ms;

static void count_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    delay_total_ms += ms;
}

TEST(test_init_enables_accel_gyro_temp_not_aux)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    ASSERT_EQ(bmi270_init(&dev, &mock_spi, &cfg), BMI270_OK);

    ASSERT_EQ(mock_regs[0x7D], 0x0E); /* the datasheet's own example value */
}

TEST(test_init_selects_normal_filter_mode)
{
    mock_reset();
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    ASSERT_EQ(bmi270_init(&dev, &mock_spi, &cfg), BMI270_OK);

    /* 100 Hz accel and 200 Hz gyro, the values in the datasheet's quick start. */
    ASSERT_EQ(mock_regs[0x40], 0xA8);
    ASSERT_EQ(mock_regs[0x42], 0xA9);
}

TEST(test_init_rejects_error_messages)
{
    static const uint8_t errors[] = { 0x00, 0x03, 0x05, 0x07 };
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();

    for (unsigned i = 0; i < sizeof(errors); i++) {
        mock_reset();
        mock_regs[0x21] = errors[i];
        ASSERT_EQ(bmi270_init(&dev, &mock_spi, &cfg), BMI270_ERR_CONFIG);
    }
}

TEST(test_init_waits_for_each_step)
{
    static const bmi270_spi_t spi = {
        .cs_low   = mock_cs_low,
        .cs_high  = mock_cs_high,
        .xfer     = mock_xfer,
        .delay_ms = count_delay,
        .ctx      = NULL,
    };
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    mock_reset();
    delay_total_ms = 0;

    ASSERT_EQ(bmi270_init(&dev, &spi, &cfg), BMI270_OK);

    /* 2 ms reset, 1 ms SPI switch, 1 ms after PWR_CONF, 20 ms config load. */
    ASSERT_EQ(delay_total_ms, 24);
}

TEST(test_init_requires_delay)
{
    static const bmi270_spi_t spi = {
        .cs_low   = mock_cs_low,
        .cs_high  = mock_cs_high,
        .xfer     = mock_xfer,
        .delay_ms = NULL,
        .ctx      = NULL,
    };
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    mock_reset();

    ASSERT_EQ(bmi270_init(&dev, &spi, &cfg), BMI270_ERR_PARAM);
}

TEST(test_read_after_failed_init_is_rejected)
{
    bmi270_accel_gyro_t data;
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    mock_reset();
    mock_regs[0x00] = 0x00; /* wrong chip ID */

    ASSERT_EQ(bmi270_init(&dev, &mock_spi, &cfg), BMI270_ERR_ID);
    ASSERT_EQ(bmi270_read_accel_gyro(&dev, &data), BMI270_ERR_PARAM);
}

TEST(test_gyro_scale_is_exact)
{
    bmi270_accel_gyro_t data;
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    cfg.gyro_range = BMI270_GYRO_RANGE_2000DPS;
    mock_reset();
    ASSERT_EQ(bmi270_init(&dev, &mock_spi, &cfg), BMI270_OK);

    inject_accel_gyro(0, 0, 0, 32767, 0, 0); /* full scale */
    ASSERT_EQ(bmi270_read_accel_gyro(&dev, &data), BMI270_OK);
    ASSERT_NEAR(data.gx_dps, 1999.94f, 0.01f);
}

TEST(test_soft_reset_waits_for_the_chip)
{
    static const bmi270_spi_t spi = {
        .cs_low   = mock_cs_low,
        .cs_high  = mock_cs_high,
        .xfer     = mock_xfer,
        .delay_ms = count_delay,
        .ctx      = NULL,
    };
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();
    mock_reset();
    ASSERT_EQ(bmi270_init(&dev, &spi, &cfg), BMI270_OK);
    delay_total_ms = 0;

    ASSERT_EQ(bmi270_soft_reset(&dev), BMI270_OK);
    ASSERT_EQ(delay_total_ms, 3);
}

TEST(test_range_change_waits_for_the_next_sample)
{
    bmi270_spi_t timed = mock_spi;
    timed.delay_ms = count_delay;
    bmi270_t dev;
    bmi270_cfg_t cfg = bmi270_default_cfg();

    mock_reset();
    ASSERT_EQ(bmi270_init(&dev, &timed, &cfg), BMI270_OK);

    /* a sample takes 40 ms at the slowest ODR the API offers, 25 Hz */
    delay_total_ms = 0;
    ASSERT_EQ(bmi270_set_acc_range(&dev, BMI270_ACC_RANGE_16G), BMI270_OK);
    ASSERT_TRUE(delay_total_ms >= 40);

    delay_total_ms = 0;
    ASSERT_EQ(bmi270_set_gyro_range(&dev, BMI270_GYRO_RANGE_250DPS), BMI270_OK);
    ASSERT_TRUE(delay_total_ms >= 40);
}

TEST_MAIN("bmi270")
    RUN(test_init_ok);
    RUN(test_init_bad_id);
    RUN(test_init_config_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_default_cfg);
    RUN(test_read_accel_gyro);
    RUN(test_read_accel_gyro_8g);
    RUN(test_read_accel_null);
    RUN(test_read_temp);
    RUN(test_read_temp_negative);
    RUN(test_read_temp_null);
    RUN(test_read_temp_invalid);
    RUN(test_set_acc_odr);
    RUN(test_set_gyro_odr);
    RUN(test_set_acc_range);
    RUN(test_set_gyro_range);
    RUN(test_set_range_invalid);
    RUN(test_set_odr_null);
    RUN(test_set_range_null);
    RUN(test_soft_reset);
    RUN(test_soft_reset_null);
    RUN(test_init_enables_accel_gyro_temp_not_aux);
    RUN(test_init_selects_normal_filter_mode);
    RUN(test_init_rejects_error_messages);
    RUN(test_init_waits_for_each_step);
    RUN(test_init_requires_delay);
    RUN(test_read_after_failed_init_is_rejected);
    RUN(test_gyro_scale_is_exact);
    RUN(test_soft_reset_waits_for_the_chip);
    RUN(test_range_change_waits_for_the_next_sample);
TEST_MAIN_END()
