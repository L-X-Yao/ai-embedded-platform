// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "tsl2591.h"
#include <math.h>

/* ---------- command byte helpers ---------- */

/* The driver sends (0xA0 | reg) as the register address to the HAL.
 * The mock stores values at that address, so we index mock_regs
 * using the command-byte-prefixed address.                         */
#define CMD(reg) ((uint8_t)(TSL2591_CMD_BIT | (reg)))

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* Device ID register: expect 0x50 at CMD(0x12) = 0xB2 */
    mock_regs[CMD(TSL2591_REG_ID)] = TSL2591_DEVICE_ID;
    /* AVALID: these tests start after an integration has finished */
    mock_regs[CMD(TSL2591_REG_STATUS)] = 0x01;
}

/* Set raw channel values in mock registers (little-endian) */
static void mock_set_channels(uint16_t ch0, uint16_t ch1)
{
    mock_regs[CMD(TSL2591_REG_C0DATAL)] = (uint8_t)(ch0 & 0xFF);
    mock_regs[CMD(TSL2591_REG_C0DATAH)] = (uint8_t)(ch0 >> 8);
    mock_regs[CMD(TSL2591_REG_C1DATAL)] = (uint8_t)(ch1 & 0xFF);
    mock_regs[CMD(TSL2591_REG_C1DATAH)] = (uint8_t)(ch1 >> 8);
}

static const tsl2591_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static tsl2591_t dev;

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_MEDIUM,
                           TSL2591_ITIME_100MS), TSL2591_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[CMD(TSL2591_REG_ID)] = 0xFF;
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;  /* fail on first I2C op (ID read) */
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_ERR_I2C);
}

TEST(test_init_enable_i2c_fail)
{
    mock_reset();
    mock_fail_at = 1;  /* fail on second I2C op (enable write) */
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_ERR_I2C);
}

TEST(test_init_config_i2c_fail)
{
    mock_reset();
    mock_fail_at = 2;  /* fail on third I2C op (config write) */
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_ERR_I2C);
}

TEST(test_init_invalid_gain)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, (tsl2591_gain_t)0xFF,
                           TSL2591_ITIME_100MS), TSL2591_ERR_PARAM);
}

TEST(test_init_invalid_itime)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           (tsl2591_itime_t)0xFF), TSL2591_ERR_PARAM);
}

TEST(test_init_null_write_reg)
{
    mock_reset();
    tsl2591_i2c_t bad_i2c = { .write_reg = NULL, .read_regs = mock_i2c_read_regs };
    ASSERT_EQ(tsl2591_init(&dev, &bad_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_ERR_PARAM);
}

TEST(test_init_null_read_regs)
{
    mock_reset();
    tsl2591_i2c_t bad_i2c = { .write_reg = mock_i2c_write_reg, .read_regs = NULL };
    ASSERT_EQ(tsl2591_init(&dev, &bad_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_ERR_PARAM);
}

/* ---------- null safety ---------- */

TEST(test_null_safety)
{
    ASSERT_EQ(tsl2591_init(NULL, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_init(&dev, NULL, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_ERR_PARAM);

    tsl2591_data_t data;
    ASSERT_EQ(tsl2591_read(NULL, &data), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_read(&dev, NULL), TSL2591_ERR_PARAM);

    uint16_t ch0, ch1;
    ASSERT_EQ(tsl2591_read_raw(NULL, &ch0, &ch1), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_read_raw(&dev, NULL, &ch1), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_read_raw(&dev, &ch0, NULL), TSL2591_ERR_PARAM);

    ASSERT_EQ(tsl2591_set_gain(NULL, TSL2591_GAIN_LOW), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_set_integration_time(NULL, TSL2591_ITIME_100MS),
              TSL2591_ERR_PARAM);

    ASSERT_EQ(tsl2591_enable(NULL), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_disable(NULL), TSL2591_ERR_PARAM);

    uint8_t status;
    ASSERT_EQ(tsl2591_get_status(NULL, &status), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_get_status(&dev, NULL), TSL2591_ERR_PARAM);
}

/* ---------- read tests ---------- */

TEST(test_read_lux_known_value)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_MEDIUM,
                           TSL2591_ITIME_100MS), TSL2591_OK);

    /* ch0=500, ch1=100 at MEDIUM gain (25x), 100ms itime
     * cpl = (100.0 * 25.0) / 408.0 = 6.1274...
     * lux = (500 - 100) / 6.1274 = 65.28... */
    mock_set_channels(500, 100);

    tsl2591_data_t data;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
    ASSERT_EQ(data.ch0_raw, 500);
    ASSERT_EQ(data.ch1_raw, 100);
    ASSERT_EQ(data.infrared, 100);
    ASSERT_EQ(data.visible, 400);
    ASSERT_NEAR(data.lux, 65.28f, 0.1f);
}

TEST(test_read_lux_low_gain)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);

    /* ch0=1000, ch1=200 at LOW gain (1x), 100ms
     * cpl = (100.0 * 1.0) / 408.0 = 0.2451...
     * lux = (1000 - 200) / 0.2451 = 3264.0 */
    mock_set_channels(1000, 200);

    tsl2591_data_t data;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
    ASSERT_NEAR(data.lux, 3264.0f, 1.0f);
}

TEST(test_read_lux_max_gain_600ms)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_MAX,
                           TSL2591_ITIME_600MS), TSL2591_OK);

    /* ch0=100, ch1=10 at MAX gain (9876x), 600ms
     * p.13: ATIME 101b is 600 ms
     * cpl = (600.0 * 9876.0) / 408.0 = 14523.529...
     * lux = (100 - 10) / 14523.529 = 0.0061968... */
    mock_set_channels(100, 10);

    tsl2591_data_t data;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
    ASSERT_NEAR(data.lux, 0.0061968f, 0.00001f);
}

TEST(test_read_zero_channels)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    mock_set_channels(0, 0);

    tsl2591_data_t data;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
    ASSERT_EQ(data.ch0_raw, 0);
    ASSERT_EQ(data.ch1_raw, 0);
    ASSERT_EQ(data.visible, 0);
    ASSERT_NEAR(data.lux, 0.0f, 0.001f);
}

TEST(test_read_ir_exceeds_visible)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);

    /* ch1 > ch0: visible should clamp to 0, lux should be 0 */
    mock_set_channels(50, 200);

    tsl2591_data_t data;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
    ASSERT_EQ(data.visible, 0);
    ASSERT_NEAR(data.lux, 0.0f, 0.001f);
}

TEST(test_read_saturation)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);

    /* both channels at max (0xFFFF) */
    mock_set_channels(0xFFFF, 0xFFFF);

    tsl2591_data_t data;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
    ASSERT_EQ(data.ch0_raw, 0xFFFF);
    ASSERT_EQ(data.ch1_raw, 0xFFFF);
    ASSERT_EQ(data.visible, 0);
    ASSERT_NEAR(data.lux, 0.0f, 0.001f);
}

TEST(test_read_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    mock_set_channels(100, 10);
    mock_fail_next = 1;

    tsl2591_data_t data;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_ERR_I2C);
}

TEST(test_read_data_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    mock_fail_at = mock_op_count + 1;  /* the STATUS read passes */

    tsl2591_data_t data;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_ERR_I2C);
}

/* p.16: AVALID is bit 0 of STATUS, and no other bit stands in for it */
TEST(test_read_not_ready_without_avalid)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    mock_set_channels(1000, 200);
    mock_regs[CMD(TSL2591_REG_STATUS)] = 0x30;  /* NPINTR, AINT */

    tsl2591_data_t data;
    memset(&data, 0xA5, sizeof(data));
    tsl2591_data_t before = data;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_ERR_NOT_READY);
    ASSERT_EQ(memcmp(&data, &before, sizeof(data)), 0);

    uint16_t ch0 = 7, ch1 = 8;
    ASSERT_EQ(tsl2591_read_raw(&dev, &ch0, &ch1), TSL2591_ERR_NOT_READY);
    ASSERT_EQ(ch0, 7);
    ASSERT_EQ(ch1, 8);

    mock_regs[CMD(TSL2591_REG_STATUS)] = 0x31;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
    ASSERT_EQ(data.ch0_raw, 1000);
}

/* ---------- raw read tests ---------- */

TEST(test_read_raw_ok)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    mock_set_channels(0x1234, 0x5678);

    uint16_t ch0, ch1;
    ASSERT_EQ(tsl2591_read_raw(&dev, &ch0, &ch1), TSL2591_OK);
    ASSERT_EQ_HEX(ch0, 0x1234);
    ASSERT_EQ_HEX(ch1, 0x5678);
}

TEST(test_read_raw_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    mock_fail_next = 1;

    uint16_t ch0, ch1;
    ASSERT_EQ(tsl2591_read_raw(&dev, &ch0, &ch1), TSL2591_ERR_I2C);
}

/* ---------- gain / integration time ---------- */

TEST(test_set_gain)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);

    ASSERT_EQ(tsl2591_set_gain(&dev, TSL2591_GAIN_HIGH), TSL2591_OK);

    /* verify config register was written with new gain */
    uint8_t cfg_reg = mock_regs[CMD(TSL2591_REG_CONFIG)];
    ASSERT_EQ_HEX(cfg_reg & 0x30, TSL2591_GAIN_HIGH);
}

TEST(test_set_gain_invalid)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    ASSERT_EQ(tsl2591_set_gain(&dev, (tsl2591_gain_t)0xFF), TSL2591_ERR_PARAM);
}

TEST(test_set_gain_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    mock_fail_next = 1;
    ASSERT_EQ(tsl2591_set_gain(&dev, TSL2591_GAIN_HIGH), TSL2591_ERR_I2C);
}

TEST(test_set_integration_time)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);

    ASSERT_EQ(tsl2591_set_integration_time(&dev, TSL2591_ITIME_400MS), TSL2591_OK);

    uint8_t cfg_reg = mock_regs[CMD(TSL2591_REG_CONFIG)];
    ASSERT_EQ_HEX(cfg_reg & 0x07, TSL2591_ITIME_400MS);
}

TEST(test_set_integration_time_invalid)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    ASSERT_EQ(tsl2591_set_integration_time(&dev, (tsl2591_itime_t)0xFF),
              TSL2591_ERR_PARAM);
}

TEST(test_set_itime_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    mock_fail_next = 1;
    ASSERT_EQ(tsl2591_set_integration_time(&dev, TSL2591_ITIME_300MS),
              TSL2591_ERR_I2C);
}

/* ---------- enable / disable ---------- */

TEST(test_enable_disable)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);

    ASSERT_EQ(tsl2591_disable(&dev), TSL2591_OK);
    ASSERT_EQ_HEX(mock_regs[CMD(TSL2591_REG_ENABLE)], 0x00);

    ASSERT_EQ(tsl2591_enable(&dev), TSL2591_OK);
    ASSERT_EQ_HEX(mock_regs[CMD(TSL2591_REG_ENABLE)], 0x03);  /* PON|AEN */
}

TEST(test_enable_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    mock_fail_next = 1;
    ASSERT_EQ(tsl2591_enable(&dev), TSL2591_ERR_I2C);
}

TEST(test_disable_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    mock_fail_next = 1;
    ASSERT_EQ(tsl2591_disable(&dev), TSL2591_ERR_I2C);
}

/* ---------- status ---------- */

TEST(test_get_status)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);

    mock_regs[CMD(TSL2591_REG_STATUS)] = 0x11;  /* AVALID + AINT */
    uint8_t status = 0;
    ASSERT_EQ(tsl2591_get_status(&dev, &status), TSL2591_OK);
    ASSERT_EQ_HEX(status, 0x11);
}

TEST(test_get_status_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_OK);
    mock_fail_next = 1;

    uint8_t status;
    ASSERT_EQ(tsl2591_get_status(&dev, &status), TSL2591_ERR_I2C);
}

/* ---------- zero-init safety ---------- */

TEST(test_zero_init_safety)
{
    tsl2591_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    /* zero-init struct must not crash on null guard paths */
}

/* ---------- lux calculation at all gain levels ---------- */

TEST(test_lux_all_gains)
{
    /* ch0=1000, ch1=0, 100ms integration (itime_ms=100)
     * lux = ch0 / cpl = 1000 / (100*gain/408) */
    const struct {
        tsl2591_gain_t gain;
        float expected_lux;
    } cases[] = {
        { TSL2591_GAIN_LOW,    1000.0f / (100.0f * 1.0f / 408.0f) },
        { TSL2591_GAIN_MEDIUM, 1000.0f / (100.0f * 25.0f / 408.0f) },
        { TSL2591_GAIN_HIGH,   1000.0f / (100.0f * 428.0f / 408.0f) },
        { TSL2591_GAIN_MAX,    1000.0f / (100.0f * 9876.0f / 408.0f) },
    };

    for (int i = 0; i < 4; i++) {
        mock_reset();
        ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, cases[i].gain,
                               TSL2591_ITIME_100MS), TSL2591_OK);
        mock_set_channels(1000, 0);

        tsl2591_data_t data;
        ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
        ASSERT_NEAR(data.lux, cases[i].expected_lux, cases[i].expected_lux * 0.01f);
    }
}

/* ---------- lux calculation at all integration times ---------- */

TEST(test_lux_all_itimes)
{
    /* ch0=500, ch1=0, LOW gain (1x)
     * cpl = (itime_ms * 1.0) / 408.0
     * itime_ms = 100*(itime+1) */
    for (int t = 0; t <= 5; t++) {
        mock_reset();
        tsl2591_itime_t itime = (tsl2591_itime_t)t;
        ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW, itime),
                  TSL2591_OK);
        mock_set_channels(500, 0);

        float itime_ms = 100.0f * ((float)t + 1.0f);
        float cpl = (itime_ms * 1.0f) / 408.0f;
        float expected = 500.0f / cpl;

        tsl2591_data_t data;
        ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
        ASSERT_NEAR(data.lux, expected, expected * 0.01f);
    }
}

/* ---------- hw_model ---------- */

TEST(test_hw_model_id_check)
{
    /* init with wrong ID must fail */
    mock_reset();
    mock_regs[CMD(TSL2591_REG_ID)] = 0x00;
    ASSERT_EQ(tsl2591_init(&dev, &mock_i2c, TSL2591_GAIN_LOW,
                           TSL2591_ITIME_100MS), TSL2591_ERR_ID);
}

/* ---------- a setting the chip did not take ---------- */

/* 800 counts at low gain and 100 ms: 800 / (100 * 1 / 408) = 3264 lux */
static void assert_low_gain_100ms_scale(tsl2591_t *d)
{
    tsl2591_data_t data;
    ASSERT_EQ(tsl2591_read(d, &data), TSL2591_OK);
    ASSERT_NEAR(data.lux, 3264.0f, 1.0f);
}

static void start_at_low_gain_100ms(tsl2591_t *d)
{
    mock_reset();
    mock_set_channels(1000, 200);
    ASSERT_EQ(tsl2591_init(d, &mock_i2c, TSL2591_GAIN_LOW, TSL2591_ITIME_100MS),
              TSL2591_OK);
    assert_low_gain_100ms_scale(d);
}

TEST(test_failed_set_gain_keeps_the_scale)
{
    tsl2591_t d;
    start_at_low_gain_100ms(&d);

    mock_fail_next = 1;
    ASSERT_EQ(tsl2591_set_gain(&d, TSL2591_GAIN_MAX), TSL2591_ERR_I2C);
    ASSERT_EQ(d.gain, TSL2591_GAIN_LOW);
    assert_low_gain_100ms_scale(&d);
}

TEST(test_failed_set_integration_time_keeps_the_scale)
{
    tsl2591_t d;
    start_at_low_gain_100ms(&d);

    mock_fail_next = 1;
    ASSERT_EQ(tsl2591_set_integration_time(&d, TSL2591_ITIME_600MS), TSL2591_ERR_I2C);
    ASSERT_EQ(d.itime, TSL2591_ITIME_100MS);
    assert_low_gain_100ms_scale(&d);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(tsl2591_t *d)
{
    tsl2591_data_t data;
    uint16_t ch0;
    uint16_t ch1;
    uint8_t status;
    ASSERT_EQ(tsl2591_read(d, &data), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_read_raw(d, &ch0, &ch1), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_set_gain(d, TSL2591_GAIN_LOW), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_set_integration_time(d, TSL2591_ITIME_100MS), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_enable(d), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_disable(d), TSL2591_ERR_PARAM);
    ASSERT_EQ(tsl2591_get_status(d, &status), TSL2591_ERR_PARAM);
}

TEST(test_device_refused_after_wrong_id)
{
    tsl2591_t d;
    mock_reset();
    mock_regs[CMD(TSL2591_REG_ID)] = 0xFF;
    ASSERT_EQ(tsl2591_init(&d, &mock_i2c, TSL2591_GAIN_LOW, TSL2591_ITIME_100MS),
              TSL2591_ERR_ID);
    assert_device_refused(&d);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const tsl2591_i2c_t zeroed;
    tsl2591_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(tsl2591_init(&d, NULL, TSL2591_GAIN_LOW, TSL2591_ITIME_100MS),
              TSL2591_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(tsl2591_init(&d, &zeroed, TSL2591_GAIN_LOW, TSL2591_ITIME_100MS),
              TSL2591_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("tsl2591")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_init_enable_i2c_fail);
    RUN(test_init_config_i2c_fail);
    RUN(test_init_invalid_gain);
    RUN(test_init_invalid_itime);
    RUN(test_init_null_write_reg);
    RUN(test_init_null_read_regs);
    RUN(test_null_safety);
    RUN(test_read_lux_known_value);
    RUN(test_read_lux_low_gain);
    RUN(test_read_lux_max_gain_600ms);
    RUN(test_read_zero_channels);
    RUN(test_read_ir_exceeds_visible);
    RUN(test_read_saturation);
    RUN(test_read_i2c_fail);
    RUN(test_read_data_i2c_fail);
    RUN(test_read_not_ready_without_avalid);
    RUN(test_read_raw_ok);
    RUN(test_read_raw_i2c_fail);
    RUN(test_set_gain);
    RUN(test_set_gain_invalid);
    RUN(test_set_gain_i2c_fail);
    RUN(test_set_integration_time);
    RUN(test_set_integration_time_invalid);
    RUN(test_set_itime_i2c_fail);
    RUN(test_enable_disable);
    RUN(test_enable_i2c_fail);
    RUN(test_disable_i2c_fail);
    RUN(test_get_status);
    RUN(test_get_status_i2c_fail);
    RUN(test_zero_init_safety);
    RUN(test_lux_all_gains);
    RUN(test_lux_all_itimes);
    RUN(test_hw_model_id_check);
    RUN(test_failed_set_gain_keeps_the_scale);
    RUN(test_failed_set_integration_time_keeps_the_scale);
    RUN(test_device_refused_after_wrong_id);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
