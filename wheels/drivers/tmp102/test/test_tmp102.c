// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "tmp102.h"
#include <math.h>

/* ---------- chip-specific mock setup ---------- */

/*
 * TMP102 pointer register layout (each pointer → 2 data bytes):
 *   ptr 0x00 → mock_regs[0x00..0x01] = TEMP
 *   ptr 0x01 → mock_regs[0x01..0x02] = CONFIG  (default 0x60A0)
 *   ptr 0x02 → mock_regs[0x02..0x03] = T_LOW   (default 75 C = 0x4B00)
 *   ptr 0x03 → mock_regs[0x03..0x04] = T_HIGH  (default 80 C = 0x5000)
 *
 * Note: mock_i2c read_regs/write_regs maps reg+offset into mock_regs[],
 * so pointer registers with 2-byte payloads overlap in the flat array.
 * We seed only the non-overlapping config bytes for init verification.
 * Tests that check specific registers plant values just before reading.
 */
static void mock_reset(void)
{
    mock_i2c_reset();
    /*
     * Seed config at ptr 0x01 → mock_regs[0x01]=MSB, mock_regs[0x02]=LSB.
     * Default config 0x60A0: SD=0, 12-bit, continuous.
     */
    mock_regs[0x01] = 0x60;
    mock_regs[0x02] = 0xA0;
}

static const tmp102_i2c_t mock_i2c = {
    .write_reg  = mock_i2c_write_reg,
    .write_regs = mock_i2c_write_regs,
    .read_regs  = mock_i2c_read_regs,
    .ctx        = NULL,
};

static tmp102_t dev;

static void setup(void)
{
    mock_reset();
    tmp102_cfg_t cfg = { .addr = TMP102_ADDR_GND };
    tmp102_init(&dev, &mock_i2c, &cfg);
}

/* Plant a 12-bit temperature value into the temp register (big-endian,
 * bits[15:4]). raw is the signed 12-bit value. */
static void plant_temp(int16_t raw)
{
    uint16_t val = (uint16_t)((uint16_t)raw << 4);
    mock_regs[0x00] = (uint8_t)(val >> 8);
    mock_regs[0x01] = (uint8_t)(val & 0xFFu);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    tmp102_t d;
    tmp102_cfg_t cfg = { .addr = TMP102_ADDR_GND };
    ASSERT_EQ(tmp102_init(&d, &mock_i2c, &cfg), TMP102_OK);
}

TEST(test_init_clears_shutdown)
{
    mock_reset();
    /* Set SD bit in config MSB (ptr 0x01 → mock_regs[0x01]) */
    mock_regs[0x01] = 0x61;  /* 0x60 | SD */
    tmp102_t d;
    tmp102_cfg_t cfg = { .addr = TMP102_ADDR_GND };
    ASSERT_EQ(tmp102_init(&d, &mock_i2c, &cfg), TMP102_OK);
    /* Config MSB should have SD cleared */
    ASSERT_EQ(mock_regs[0x01] & 0x01, 0);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    tmp102_t d;
    tmp102_cfg_t cfg = { .addr = TMP102_ADDR_GND };
    ASSERT_EQ(tmp102_init(&d, &mock_i2c, &cfg), TMP102_ERR_I2C);
}

TEST(test_null_safety)
{
    tmp102_cfg_t cfg = { .addr = TMP102_ADDR_GND };
    float temp;
    ASSERT_EQ(tmp102_init(NULL, &mock_i2c, &cfg), TMP102_ERR_PARAM);
    ASSERT_EQ(tmp102_init(&dev, NULL, &cfg), TMP102_ERR_PARAM);
    ASSERT_EQ(tmp102_init(&dev, &mock_i2c, NULL), TMP102_ERR_PARAM);
    ASSERT_EQ(tmp102_read_temp(NULL, &temp), TMP102_ERR_PARAM);
    ASSERT_EQ(tmp102_read_temp(&dev, NULL), TMP102_ERR_PARAM);
    ASSERT_EQ(tmp102_set_alert_high(NULL, 80.0f), TMP102_ERR_PARAM);
    ASSERT_EQ(tmp102_set_alert_low(NULL, 75.0f), TMP102_ERR_PARAM);
    ASSERT_EQ(tmp102_shutdown(NULL), TMP102_ERR_PARAM);
    ASSERT_EQ(tmp102_wake(NULL), TMP102_ERR_PARAM);
}

TEST(test_read_25c)
{
    setup();
    /* 25 C = 25/0.0625 = 400 = 0x190 (12-bit) */
    plant_temp(400);
    float temp;
    ASSERT_EQ(tmp102_read_temp(&dev, &temp), TMP102_OK);
    ASSERT_NEAR(temp, 25.0f, 0.001f);
}

TEST(test_read_0c)
{
    setup();
    plant_temp(0);
    float temp;
    ASSERT_EQ(tmp102_read_temp(&dev, &temp), TMP102_OK);
    ASSERT_NEAR(temp, 0.0f, 0.001f);
}

TEST(test_read_negative)
{
    setup();
    /* -25 C = -25/0.0625 = -400 */
    plant_temp(-400);
    float temp;
    ASSERT_EQ(tmp102_read_temp(&dev, &temp), TMP102_OK);
    ASSERT_NEAR(temp, -25.0f, 0.001f);
}

TEST(test_read_max_positive)
{
    setup();
    /* 127.9375 C = 2047 * 0.0625 */
    plant_temp(2047);
    float temp;
    ASSERT_EQ(tmp102_read_temp(&dev, &temp), TMP102_OK);
    ASSERT_NEAR(temp, 127.9375f, 0.01f);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    float temp;
    ASSERT_EQ(tmp102_read_temp(&dev, &temp), TMP102_ERR_I2C);
}

TEST(test_alert_high)
{
    setup();
    ASSERT_EQ(tmp102_set_alert_high(&dev, 80.0f), TMP102_OK);
    /* 80 C = 1280 raw = 0x500; bits[15:4] → 0x5000
     * T_HIGH ptr 0x03 → mock_regs[0x03]=MSB, mock_regs[0x04]=LSB */
    ASSERT_EQ(mock_regs[0x03], 0x50);
    ASSERT_EQ(mock_regs[0x04], 0x00);
}

TEST(test_alert_low)
{
    setup();
    ASSERT_EQ(tmp102_set_alert_low(&dev, 50.0f), TMP102_OK);
    /* 50 C = 800 raw = 0x320; bits[15:4] → 0x3200
     * T_LOW ptr 0x02 → mock_regs[0x02]=MSB, mock_regs[0x03]=LSB */
    ASSERT_EQ(mock_regs[0x02], 0x32);
    ASSERT_EQ(mock_regs[0x03], 0x00);
}

TEST(test_alert_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(tmp102_set_alert_high(&dev, 80.0f), TMP102_ERR_I2C);
}

/* T_HIGH as written: ptr 0x03 lands in mock_regs[0x03..0x04] */
static uint16_t t_high_word(void)
{
    return (uint16_t)((uint16_t)mock_regs[0x03] << 8 | mock_regs[0x04]);
}

/* TI SBOS397I, Table 6-2 (p.9). The limit registers use the same format
 * (p.19), and the 12 bits are left-justified in the 16-bit word. */
TEST(test_alert_table_6_2)
{
    static const struct {
        float    temp_c;
        uint16_t word;
    } rows[] = {
        {  128.0f,    0x7FF0 }, {  127.9375f, 0x7FF0 }, { 100.0f, 0x6400 },
        {   80.0f,    0x5000 }, {   75.0f,    0x4B00 }, {  50.0f, 0x3200 },
        {   25.0f,    0x1900 }, {    0.25f,   0x0040 }, {   0.0f, 0x0000 },
        {   -0.25f,   0xFFC0 }, {  -25.0f,    0xE700 }, { -55.0f, 0xC900 },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        setup();
        ASSERT_EQ(tmp102_set_alert_high(&dev, rows[i].temp_c), TMP102_OK);
        ASSERT_EQ(t_high_word(), rows[i].word);
    }
}

/* Past the ends of the 12-bit format a limit must not wrap to the other
 * sign: 150 C used to become 0x9600, which is -106 C. */
TEST(test_alert_saturates)
{
    static const struct {
        float    temp_c;
        uint16_t word;
    } rows[] = {
        {  150.0f,   0x7FF0 }, {  1.0e6f,   0x7FF0 }, {  INFINITY, 0x7FF0 },
        { -128.0f,   0x8000 }, { -150.0f,   0x8000 }, { -1.0e6f,   0x8000 },
        { -INFINITY, 0x8000 },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        setup();
        ASSERT_EQ(tmp102_set_alert_high(&dev, rows[i].temp_c), TMP102_OK);
        ASSERT_EQ(t_high_word(), rows[i].word);
    }
}

TEST(test_alert_refuses_nan)
{
    setup();
    mock_regs[0x03] = 0x50;
    mock_regs[0x04] = 0x00;
    ASSERT_EQ(tmp102_set_alert_high(&dev, NAN), TMP102_ERR_PARAM);
    ASSERT_EQ(tmp102_set_alert_low(&dev, NAN), TMP102_ERR_PARAM);
    ASSERT_EQ(t_high_word(), 0x5000);
}

TEST(test_shutdown_sets_bit)
{
    setup();
    ASSERT_EQ(tmp102_shutdown(&dev), TMP102_OK);
    /* Config MSB at mock_regs[0x01], SD is bit 0 */
    ASSERT_EQ(mock_regs[0x01] & 0x01, 0x01);
}

TEST(test_wake_clears_bit)
{
    setup();
    tmp102_shutdown(&dev);
    ASSERT_EQ(tmp102_wake(&dev), TMP102_OK);
    ASSERT_EQ(mock_regs[0x01] & 0x01, 0x00);
}

TEST(test_shutdown_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(tmp102_shutdown(&dev), TMP102_ERR_I2C);
}

TEST(test_addr_variants)
{
    mock_reset();
    tmp102_t d;
    tmp102_cfg_t cfg;

    cfg.addr = TMP102_ADDR_VS;
    ASSERT_EQ(tmp102_init(&d, &mock_i2c, &cfg), TMP102_OK);

    cfg.addr = TMP102_ADDR_SDA;
    ASSERT_EQ(tmp102_init(&d, &mock_i2c, &cfg), TMP102_OK);

    cfg.addr = TMP102_ADDR_SCL;
    ASSERT_EQ(tmp102_init(&d, &mock_i2c, &cfg), TMP102_OK);
}

TEST(test_init_write_config_fail)
{
    /* read16(REG_CONFIG) succeeds (op 0), write16(REG_CONFIG) fails (op 1). */
    mock_reset();
    mock_fail_at = 1;
    tmp102_t d;
    tmp102_cfg_t cfg = { .addr = TMP102_ADDR_GND };
    ASSERT_EQ(tmp102_init(&d, &mock_i2c, &cfg), TMP102_ERR_I2C);
}

TEST(test_shutdown_write_fail)
{
    /* After init (2 ops: read16=0, write16=1), shutdown reads config (op 2 ok)
     * then writes config (op 3) — make that write fail. */
    setup();
    mock_fail_at = 3;
    ASSERT_EQ(tmp102_shutdown(&dev), TMP102_ERR_I2C);
}

TEST(test_wake_write_fail)
{
    /* After init (2 ops), wake reads config (op 2 ok) then writes (op 3) — fail. */
    setup();
    mock_fail_at = 3;
    ASSERT_EQ(tmp102_wake(&dev), TMP102_ERR_I2C);
}

TEST_MAIN("tmp102")
    RUN(test_init_ok);
    RUN(test_init_clears_shutdown);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_25c);
    RUN(test_read_0c);
    RUN(test_read_negative);
    RUN(test_read_max_positive);
    RUN(test_read_i2c_fail);
    RUN(test_alert_high);
    RUN(test_alert_low);
    RUN(test_alert_i2c_fail);
    RUN(test_alert_table_6_2);
    RUN(test_alert_saturates);
    RUN(test_alert_refuses_nan);
    RUN(test_shutdown_sets_bit);
    RUN(test_wake_clears_bit);
    RUN(test_shutdown_i2c_fail);
    RUN(test_addr_variants);
    RUN(test_init_write_config_fail);
    RUN(test_shutdown_write_fail);
    RUN(test_wake_write_fail);
TEST_MAIN_END()
