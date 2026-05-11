// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "sgp40.h"

/*
 * SGP40 mock: command-based protocol with CRC.
 * write_reg sends command bytes, read_regs returns prepared response.
 */

static uint8_t last_cmd_msb;
static uint8_t last_cmd_lsb;
static uint16_t response_word;
static uint8_t  response_crc;
static int      cmd_count;

static void mock_reset(void)
{
    mock_i2c_reset();
    last_cmd_msb = 0;
    last_cmd_lsb = 0;
    cmd_count = 0;

    /* Default: self-test passes (0xD400 + CRC) */
    response_word = 0xD400;
    uint8_t d[2] = { 0xD4, 0x00 };
    response_crc = sgp40_crc8(d, 2);
}

static bool mock_sgp_write(void *ctx, uint8_t dev_addr,
                           uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_should_fail()) return false;
    mock_write_count++;
    last_cmd_msb = reg;
    last_cmd_lsb = val;
    cmd_count++;
    return true;
}

static bool mock_sgp_read(void *ctx, uint8_t dev_addr,
                          uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)reg;
    if (mock_i2c_should_fail()) return false;
    mock_read_count++;

    if (len >= 3) {
        buf[0] = (uint8_t)(response_word >> 8);
        buf[1] = (uint8_t)(response_word & 0xFF);
        buf[2] = response_crc;
    }
    return true;
}

static const sgp40_i2c_t mock_sgp_i2c = {
    .write_reg = mock_sgp_write,
    .read_regs = mock_sgp_read,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static sgp40_t dev;

static void set_response(uint16_t word)
{
    response_word = word;
    uint8_t d[2] = { (uint8_t)(word >> 8), (uint8_t)(word & 0xFF) };
    response_crc = sgp40_crc8(d, 2);
}

static void setup(void)
{
    mock_reset();
    set_response(0xD400);  /* self-test pass */
    sgp40_init(&dev, &mock_sgp_i2c);
}

/* ---------- tests ---------- */

TEST(test_crc8_known_values)
{
    /* Sensirion CRC test vector: {0xBE, 0xEF} => CRC = 0x92 */
    uint8_t data1[] = { 0xBE, 0xEF };
    ASSERT_EQ(sgp40_crc8(data1, 2), 0x92);

    /* {0x00, 0x00} */
    uint8_t data2[] = { 0x00, 0x00 };
    uint8_t crc2 = sgp40_crc8(data2, 2);
    ASSERT_TRUE(crc2 != 0);  /* non-trivial */

    /* NULL safety */
    ASSERT_EQ(sgp40_crc8(NULL, 0), 0);
}

TEST(test_init_ok)
{
    mock_reset();
    set_response(0xD400);
    sgp40_t d;
    sgp40_err_t e = sgp40_init(&d, &mock_sgp_i2c);
    ASSERT_EQ(e, SGP40_OK);
}

TEST(test_init_self_test_fail)
{
    mock_reset();
    set_response(0x0000);  /* wrong self-test result */
    sgp40_t d;
    sgp40_err_t e = sgp40_init(&d, &mock_sgp_i2c);
    ASSERT_EQ(e, SGP40_ERR_SELF_TEST);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    sgp40_t d;
    sgp40_err_t e = sgp40_init(&d, &mock_sgp_i2c);
    ASSERT_EQ(e, SGP40_ERR_I2C);
}

TEST(test_init_crc_fail)
{
    mock_reset();
    response_word = 0xD400;
    response_crc  = 0x00;  /* wrong CRC */
    sgp40_t d;
    sgp40_err_t e = sgp40_init(&d, &mock_sgp_i2c);
    ASSERT_EQ(e, SGP40_ERR_CRC);
}

TEST(test_null_safety)
{
    ASSERT_EQ(sgp40_init(NULL, &mock_sgp_i2c), SGP40_ERR_PARAM);
    sgp40_t d;
    ASSERT_EQ(sgp40_init(&d, NULL), SGP40_ERR_PARAM);
    sgp40_data_t data;
    ASSERT_EQ(sgp40_measure_raw(NULL, 0x8000, 0x6666, &data), SGP40_ERR_PARAM);
    ASSERT_EQ(sgp40_measure_raw(&d, 0x8000, 0x6666, NULL), SGP40_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    sgp40_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    sgp40_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(sgp40_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)SGP40_ERR_I2C);
    ASSERT_EQ(-2, (int)SGP40_ERR_CRC);
    ASSERT_EQ(-3, (int)SGP40_ERR_PARAM);
    ASSERT_EQ(-4, (int)SGP40_ERR_SELF_TEST);
}

TEST(test_measure_raw)
{
    setup();
    set_response(26000);
    sgp40_data_t data;
    sgp40_err_t e = sgp40_measure_raw(&dev, 0x8000, 0x6666, &data);
    ASSERT_EQ(e, SGP40_OK);
    ASSERT_EQ(data.sraw, 26000);
}

TEST(test_measure_raw_zero)
{
    setup();
    set_response(0);
    sgp40_data_t data;
    sgp40_measure_raw(&dev, 0x8000, 0x6666, &data);
    ASSERT_EQ(data.sraw, 0);
}

TEST(test_measure_raw_max)
{
    setup();
    set_response(65535);
    sgp40_data_t data;
    sgp40_measure_raw(&dev, 0x8000, 0x6666, &data);
    ASSERT_EQ(data.sraw, 65535);
}

TEST(test_measure_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    sgp40_data_t data;
    sgp40_err_t e = sgp40_measure_raw(&dev, 0x8000, 0x6666, &data);
    ASSERT_EQ(e, SGP40_ERR_I2C);
}

TEST(test_measure_crc_fail)
{
    setup();
    response_word = 26000;
    response_crc  = 0x00;  /* wrong CRC */
    sgp40_data_t data;
    sgp40_err_t e = sgp40_measure_raw(&dev, 0x8000, 0x6666, &data);
    ASSERT_EQ(e, SGP40_ERR_CRC);
}

TEST(test_delay_called)
{
    mock_reset();
    mock_delay_ms_count = 0;
    set_response(0xD400);
    sgp40_t d;
    sgp40_init(&d, &mock_sgp_i2c);
    ASSERT_TRUE(mock_delay_ms_count > 0);
}

TEST_MAIN("sgp40")
    RUN(test_crc8_known_values);
    RUN(test_init_ok);
    RUN(test_init_self_test_fail);
    RUN(test_init_i2c_fail);
    RUN(test_init_crc_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_measure_raw);
    RUN(test_measure_raw_zero);
    RUN(test_measure_raw_max);
    RUN(test_measure_i2c_fail);
    RUN(test_measure_crc_fail);
    RUN(test_delay_called);
TEST_MAIN_END()
