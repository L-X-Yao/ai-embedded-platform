// SPDX-License-Identifier: MIT
#include "test_harness.h"
#define MOCK_I2C_REG_COUNT 16
#include "mock_i2c.h"
#include "hmc5883l.h"
#include <math.h>

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x0A] = 'H';
    mock_regs[0x0B] = '4';
    mock_regs[0x0C] = '3';
}

static hmc5883l_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL
};

/* A chip with a delay hook at 15 Hz: RDY rises with the first set, after 2/fDO,
 * unless the test says the sensor is stuck. It stays up across a read: Rev E
 * p16 has it "Cleared when device initiates a write to the data output
 * registers". */
static int      data_reads;
static uint32_t delay_total;
static bool     sensor_stuck;

static bool mock_count_data_reads(void *ctx, uint8_t dev_addr, uint8_t reg,
                                  uint8_t *buf, uint16_t len)
{
    bool ok = mock_i2c_read_regs(ctx, dev_addr, reg, buf, len);
    if (reg == 0x03) data_reads++;
    return ok;
}

static void mock_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    delay_total += ms;
    if (delay_total >= 134u && !sensor_stuck) mock_regs[0x09] = 0x01;
}

static hmc5883l_i2c_t timed_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_count_data_reads,
    .delay_ms  = mock_delay,
    .ctx       = NULL
};

static void set_mock_data(int16_t x, int16_t y, int16_t z)
{
    mock_regs[0x03] = (uint8_t)((uint16_t)x >> 8);
    mock_regs[0x04] = (uint8_t)(x & 0xFF);
    mock_regs[0x05] = (uint8_t)((uint16_t)z >> 8);
    mock_regs[0x06] = (uint8_t)(z & 0xFF);
    mock_regs[0x07] = (uint8_t)((uint16_t)y >> 8);
    mock_regs[0x08] = (uint8_t)(y & 0xFF);
}

TEST(test_init_null)
{
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(NULL, &mock_i2c, &cfg), HMC5883L_ERR_PARAM);
    hmc5883l_t dev;
    ASSERT_EQ(hmc5883l_init(&dev, NULL, &cfg), HMC5883L_ERR_PARAM);
}

TEST(test_init_id_check)
{
    mock_reset();
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(&dev, &mock_i2c, &cfg), HMC5883L_OK);
}

TEST(test_init_bad_id)
{
    mock_reset();
    mock_regs[0x0A] = 'X';
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(&dev, &mock_i2c, &cfg), HMC5883L_ERR_ID);
}

TEST(test_read_raw)
{
    mock_reset();
    set_mock_data(1090, -545, 200);
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &mock_i2c, &cfg);

    hmc5883l_raw_t raw;
    ASSERT_EQ(hmc5883l_read_raw(&dev, &raw), HMC5883L_OK);
    ASSERT_EQ(raw.x, 1090);
    ASSERT_EQ(raw.y, -545);
    ASSERT_EQ(raw.z, 200);
}

TEST(test_read_gauss)
{
    mock_reset();
    set_mock_data(1090, -1090, 0);
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &mock_i2c, &cfg);

    hmc5883l_data_t data;
    ASSERT_EQ(hmc5883l_read_gauss(&dev, &data), HMC5883L_OK);
    ASSERT_NEAR(data.x_gauss, 1.0f, 0.01f);
    ASSERT_NEAR(data.y_gauss, -1.0f, 0.01f);
    ASSERT_NEAR(data.z_gauss, 0.0f, 0.01f);
}

TEST(test_different_gain)
{
    mock_reset();
    set_mock_data(440, 0, 0);
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_440, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &mock_i2c, &cfg);

    hmc5883l_data_t data;
    ASSERT_EQ(hmc5883l_read_gauss(&dev, &data), HMC5883L_OK);
    ASSERT_NEAR(data.x_gauss, 1.0f, 0.01f);
}

TEST(test_i2c_failure)
{
    mock_reset();
    mock_fail_next = 1;
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(&dev, &mock_i2c, &cfg), HMC5883L_ERR_I2C);
}

TEST(test_read_null)
{
    mock_reset();
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &mock_i2c, &cfg);
    ASSERT_EQ(hmc5883l_read_raw(&dev, NULL), HMC5883L_ERR_PARAM);
    ASSERT_EQ(hmc5883l_read_gauss(&dev, NULL), HMC5883L_ERR_PARAM);
}

TEST(test_idle_writes_mode_idle)
{
    mock_reset();
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(&dev, &mock_i2c, &cfg), HMC5883L_OK);

    /* init sets REG_MODE (0x02) = 0x00 (continuous); idle must set it to 0x02 */
    ASSERT_EQ(hmc5883l_idle(&dev), HMC5883L_OK);
    ASSERT_EQ(mock_regs[0x02], 0x02);
}

TEST(test_idle_null_param)
{
    ASSERT_EQ(hmc5883l_idle(NULL), HMC5883L_ERR_PARAM);
}

TEST(test_read_reports_adc_overflow)
{
    mock_reset();
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(&dev, &mock_i2c, &cfg), HMC5883L_OK);

    /* -4096 is the overflow marker, not a field of -3.76 Ga */
    set_mock_data(100, -4096, 100);
    hmc5883l_raw_t raw;
    ASSERT_EQ(hmc5883l_read_raw(&dev, &raw), HMC5883L_ERR_OVERFLOW);
    hmc5883l_data_t out;
    ASSERT_EQ(hmc5883l_read_gauss(&dev, &out), HMC5883L_ERR_OVERFLOW);
}

TEST(test_init_drops_the_sample_taken_at_the_old_gain)
{
    mock_reset();
    data_reads = 0;
    delay_total = 0;
    sensor_stuck = false;
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(&dev, &timed_i2c, &cfg), HMC5883L_OK);

    /* 2/fDO at 15 Hz is 133 ms, then one set is read and thrown away, and the
     * next one comes at 3/fDO. RDY cannot show that, as it is already up. */
    ASSERT_GE(delay_total, 200u);
    ASSERT_EQ(data_reads, 1);
    ASSERT_EQ(mock_regs[0x09], 0x01);
}

TEST(test_init_times_out_when_no_sample_arrives)
{
    mock_reset();
    data_reads = 0;
    delay_total = 0;
    sensor_stuck = true;
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(&dev, &timed_i2c, &cfg), HMC5883L_ERR_TIMEOUT);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(hmc5883l_t *d)
{
    hmc5883l_raw_t raw;
    hmc5883l_data_t data;
    ASSERT_EQ(hmc5883l_read_raw(d, &raw), HMC5883L_ERR_PARAM);
    ASSERT_EQ(hmc5883l_read_gauss(d, &data), HMC5883L_ERR_PARAM);
    ASSERT_EQ(hmc5883l_idle(d), HMC5883L_ERR_PARAM);
}

TEST(test_zeroed_device_refused)
{
    static hmc5883l_t zeroed;
    assert_device_refused(&zeroed);
}

TEST(test_device_refused_after_wrong_id)
{
    hmc5883l_t d;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    mock_reset();
    mock_regs[0x0C] = '0';
    ASSERT_EQ(hmc5883l_init(&d, &mock_i2c, &cfg), HMC5883L_ERR_ID);
    assert_device_refused(&d);
}

TEST(test_device_refused_after_init_timeout)
{
    hmc5883l_t d;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    mock_reset();
    data_reads = 0;
    delay_total = 0;
    sensor_stuck = true;
    ASSERT_EQ(hmc5883l_init(&d, &timed_i2c, &cfg), HMC5883L_ERR_TIMEOUT);
    sensor_stuck = false;
    assert_device_refused(&d);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    hmc5883l_t d;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_cfg_t bad = { .gain = HMC5883L_GAIN_1090, .rate = (hmc5883l_rate_t)7 };
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(hmc5883l_init(&d, NULL, &cfg), HMC5883L_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(hmc5883l_init(&d, &mock_i2c, &bad), HMC5883L_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("hmc5883l")
    RUN(test_init_null);
    RUN(test_init_id_check);
    RUN(test_init_bad_id);
    RUN(test_read_raw);
    RUN(test_read_gauss);
    RUN(test_different_gain);
    RUN(test_i2c_failure);
    RUN(test_read_null);
    RUN(test_idle_writes_mode_idle);
    RUN(test_idle_null_param);
    RUN(test_read_reports_adc_overflow);
    RUN(test_init_drops_the_sample_taken_at_the_old_gain);
    RUN(test_init_times_out_when_no_sample_arrives);
    RUN(test_zeroed_device_refused);
    RUN(test_device_refused_after_wrong_id);
    RUN(test_device_refused_after_init_timeout);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
