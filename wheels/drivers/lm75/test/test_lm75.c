// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "lm75.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* Temperature register POR value = 0x0000 (0 C) */
    mock_regs[0x00] = 0x00;
    mock_regs[0x01] = 0x00;
}

/* Inject a temperature value into the mock register 0x00 (2 bytes).
 * raw = (int16_t)(msb << 8 | lsb). The 9-bit value occupies bits [15:7]. */
static void inject_temp_raw(int16_t raw)
{
    mock_regs[0x00] = (uint8_t)((uint16_t)raw >> 8);
    mock_regs[0x01] = (uint8_t)((uint16_t)raw & 0xFFu);
}

static const lm75_i2c_t mock_i2c = {
    .write_reg  = mock_i2c_write_reg,
    .read_regs  = mock_i2c_read_regs,
    .write_regs = mock_i2c_write_regs,
    .ctx        = NULL,
};

static lm75_t dev;

static void setup(void)
{
    mock_reset();
    (void)lm75_init(&dev, &mock_i2c);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    lm75_t d;
    lm75_err_t e = lm75_init(&d, &mock_i2c);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_EQ(d.addr, 0x48);
}

TEST(test_read_25c)
{
    setup();
    /* +25.0 C: 9-bit value = 50 (0x32), stored in bits [15:7].
     * raw = 50 << 7 = 6400 = 0x1900 */
    inject_temp_raw(0x1900);
    float temp = 0.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, 25.0f, 0.01f);
}

TEST(test_read_0c)
{
    setup();
    /* 0 C: raw = 0x0000 */
    inject_temp_raw(0x0000);
    float temp = 99.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, 0.0f, 0.01f);
}

TEST(test_read_neg25c)
{
    setup();
    /* -25.0 C: 9-bit two's complement.
     * raw = (int16_t)(-25.0f * 256) = -6400 = 0xE700 */
    inject_temp_raw((int16_t)0xE700);
    float temp = 0.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, -25.0f, 0.01f);
}

TEST(test_read_125c_max)
{
    setup();
    /* +125.0 C: raw = 125 * 256 = 32000 = 0x7D00 */
    inject_temp_raw(0x7D00);
    float temp = 0.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, 125.0f, 0.01f);
}

TEST(test_read_neg55c_min)
{
    setup();
    /* -55.0 C: raw = -55 * 256 = -14080 = 0xC900 */
    inject_temp_raw((int16_t)0xC900);
    float temp = 0.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, -55.0f, 0.01f);
}

TEST(test_read_half_degree)
{
    setup();
    /* +0.5 C: raw = 0.5 * 256 = 128 = 0x0080 */
    inject_temp_raw(0x0080);
    float temp = 0.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_NEAR(temp, 0.5f, 0.01f);
}

TEST(test_custom_address)
{
    mock_reset();
    lm75_t d;
    /* A0=1, A1=1, A2=1 -> address 0x4F */
    lm75_err_t e = lm75_init_addr(&d, &mock_i2c, 0x4F);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_EQ(d.addr, 0x4F);
}

TEST(test_invalid_address)
{
    mock_reset();
    lm75_t d;
    /* Address below valid range */
    ASSERT_EQ(lm75_init_addr(&d, &mock_i2c, 0x47), LM75_ERR_PARAM);
    /* Address above valid range */
    ASSERT_EQ(lm75_init_addr(&d, &mock_i2c, 0x50), LM75_ERR_PARAM);
}

TEST(test_set_os_threshold)
{
    setup();
    lm75_err_t e = lm75_set_os_temp(&dev, 80.0f);
    ASSERT_EQ(e, LM75_OK);
    /* 80.0 C: raw = 80 * 256 = 20480 = 0x5000 */
    ASSERT_EQ(mock_regs[0x03], 0x50);
    ASSERT_EQ(mock_regs[0x04], 0x00);
}

TEST(test_set_hyst_threshold)
{
    setup();
    lm75_err_t e = lm75_set_hyst_temp(&dev, 75.0f);
    ASSERT_EQ(e, LM75_OK);
    /* 75.0 C: raw = 75 * 256 = 19200 = 0x4B00 */
    ASSERT_EQ(mock_regs[0x02], 0x4B);
    ASSERT_EQ(mock_regs[0x03], 0x00);
}

TEST(test_i2c_failure_propagation)
{
    setup();
    mock_fail_next = 1;
    float temp = 0.0f;
    lm75_err_t e = lm75_read_temp(&dev, &temp);
    ASSERT_EQ(e, LM75_ERR_I2C);
}

TEST(test_read_raw)
{
    setup();
    inject_temp_raw(0x1900);
    int16_t raw = 0;
    lm75_err_t e = lm75_read_raw(&dev, &raw);
    ASSERT_EQ(e, LM75_OK);
    ASSERT_EQ(raw, 0x1900);
}

TEST(test_null_rejection)
{
    ASSERT_EQ(lm75_init(NULL, &mock_i2c), LM75_ERR_PARAM);
    lm75_t d;
    ASSERT_EQ(lm75_init(&d, NULL), LM75_ERR_PARAM);
    ASSERT_EQ(lm75_init_addr(NULL, &mock_i2c, 0x48), LM75_ERR_PARAM);
    ASSERT_EQ(lm75_init_addr(&d, NULL, 0x48), LM75_ERR_PARAM);
    ASSERT_EQ(lm75_read_temp(NULL, &(float){0}), LM75_ERR_PARAM);
    setup();
    ASSERT_EQ(lm75_read_temp(&dev, NULL), LM75_ERR_PARAM);
    ASSERT_EQ(lm75_read_raw(NULL, &(int16_t){0}), LM75_ERR_PARAM);
    ASSERT_EQ(lm75_read_raw(&dev, NULL), LM75_ERR_PARAM);
    ASSERT_EQ(lm75_set_os_temp(NULL, 80.0f), LM75_ERR_PARAM);
    ASSERT_EQ(lm75_set_hyst_temp(NULL, 75.0f), LM75_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    lm75_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    lm75_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(lm75_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)LM75_OK);
    ASSERT_EQ(-1, (int)LM75_ERR_I2C);
    ASSERT_EQ(-2, (int)LM75_ERR_PARAM);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    lm75_t d;
    ASSERT_EQ(lm75_init(&d, &mock_i2c), LM75_ERR_I2C);
}

TEST(test_read_ignores_the_undefined_low_bits)
{
    setup();

    /* 25.0 C is 0x1900, and D0-D6 carry no data */
    mock_regs[0x00] = 0x19;
    mock_regs[0x01] = 0x7F;
    float temp = 0.0f;
    ASSERT_EQ(lm75_read_temp(&dev, &temp), LM75_OK);
    ASSERT_NEAR(temp, 25.0f, 0.001f);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(lm75_t *d)
{
    float t;
    int16_t raw;
    ASSERT_EQ(lm75_read_temp(d, &t), LM75_ERR_PARAM);
    ASSERT_EQ(lm75_read_raw(d, &raw), LM75_ERR_PARAM);
    ASSERT_EQ(lm75_set_os_temp(d, 80.0f), LM75_ERR_PARAM);
    ASSERT_EQ(lm75_set_hyst_temp(d, 75.0f), LM75_ERR_PARAM);
}

TEST(test_zeroed_device_refused)
{
    static lm75_t zeroed;
    assert_device_refused(&zeroed);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    lm75_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(lm75_init(&d, NULL), LM75_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(lm75_init_addr(&d, &mock_i2c, 0x47), LM75_ERR_PARAM);
    assert_device_refused(&d);

    /* The bus failed once: the device was never read, so it stays refused */
    mock_fail_at = 0;
    ASSERT_EQ(lm75_init(&d, &mock_i2c), LM75_ERR_I2C);
    mock_reset();
    assert_device_refused(&d);
}

TEST_MAIN("lm75")
    RUN(test_init_ok);
    RUN(test_read_25c);
    RUN(test_read_0c);
    RUN(test_read_neg25c);
    RUN(test_read_125c_max);
    RUN(test_read_neg55c_min);
    RUN(test_read_half_degree);
    RUN(test_custom_address);
    RUN(test_invalid_address);
    RUN(test_set_os_threshold);
    RUN(test_set_hyst_threshold);
    RUN(test_i2c_failure_propagation);
    RUN(test_read_raw);
    RUN(test_null_rejection);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_init_i2c_fail);
    RUN(test_read_ignores_the_undefined_low_bits);
    RUN(test_zeroed_device_refused);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
