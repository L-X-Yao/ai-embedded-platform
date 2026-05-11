// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "sgp40.h"
#include <string.h>

/*
 * SGP40 uses raw I2C write/read (not register-based), so we need custom
 * mock that captures written data and returns pre-configured responses.
 */

static uint8_t mock_write_buf[64];
static uint16_t mock_write_len;
static uint8_t mock_read_buf[64];
static uint16_t mock_read_len;
static int mock_fail_next;
static int mock_op_count;
static int mock_fail_at;
static uint32_t mock_delay_total;

static void mock_reset(void)
{
    memset(mock_write_buf, 0, sizeof(mock_write_buf));
    mock_write_len = 0;
    memset(mock_read_buf, 0, sizeof(mock_read_buf));
    mock_read_len = 0;
    mock_fail_next = 0;
    mock_op_count = 0;
    mock_fail_at = -1;
    mock_delay_total = 0;
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write(void *ctx, uint8_t dev_addr, const uint8_t *data,
                       uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_should_fail()) return false;
    if (len <= sizeof(mock_write_buf))
    {
        memcpy(mock_write_buf, data, len);
        mock_write_len = len;
    }
    return true;
}

static bool mock_read(void *ctx, uint8_t dev_addr, uint8_t *data,
                      uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_should_fail()) return false;
    for (uint16_t i = 0; i < len && i < mock_read_len; i++)
    {
        data[i] = mock_read_buf[i];
    }
    return true;
}

static void mock_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    mock_delay_total += ms;
}

static const sgp40_i2c_t mock_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

/* Helper: plant self-test OK response (0xD400 + CRC) */
static void plant_selftest_ok(void)
{
    mock_read_buf[0] = 0xD4;
    mock_read_buf[1] = 0x00;
    mock_read_buf[2] = sgp40_crc8(mock_read_buf, 2);
    mock_read_len = 3;
}

/* Helper: plant a raw measurement response */
static void plant_sraw(uint16_t sraw)
{
    mock_read_buf[0] = (uint8_t)(sraw >> 8);
    mock_read_buf[1] = (uint8_t)(sraw & 0xFF);
    mock_read_buf[2] = sgp40_crc8(mock_read_buf, 2);
    mock_read_len = 3;
}

static sgp40_t dev;

static void setup(void)
{
    mock_reset();
    plant_selftest_ok();
    sgp40_init(&dev, &mock_i2c);
}

/* ---------- CRC tests ---------- */

TEST(test_crc8_known)
{
    /* Known Sensirion CRC-8: for {0xBE, 0xEF}, CRC = 0x92 */
    uint8_t data[] = { 0xBE, 0xEF };
    ASSERT_EQ(sgp40_crc8(data, 2), 0x92);
}

TEST(test_crc8_zeros)
{
    uint8_t data[] = { 0x00, 0x00 };
    uint8_t crc = sgp40_crc8(data, 2);
    /* CRC of {0,0} with poly 0x31 init 0xFF = 0x81 */
    ASSERT_EQ(crc, 0x81);
}

TEST(test_crc8_selftest_val)
{
    uint8_t data[] = { 0xD4, 0x00 };
    uint8_t crc = sgp40_crc8(data, 2);
    /* Verify the CRC we use in selftest matches */
    ASSERT_TRUE(crc != 0);  /* non-trivial */
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    plant_selftest_ok();
    sgp40_t d;
    sgp40_err_t e = sgp40_init(&d, &mock_i2c);
    ASSERT_EQ(e, SGP40_OK);
}

TEST(test_init_selftest_fail)
{
    mock_reset();
    /* Plant wrong self-test response */
    mock_read_buf[0] = 0x00;
    mock_read_buf[1] = 0x00;
    mock_read_buf[2] = sgp40_crc8(mock_read_buf, 2);
    mock_read_len = 3;
    sgp40_t d;
    sgp40_err_t e = sgp40_init(&d, &mock_i2c);
    ASSERT_EQ(e, SGP40_ERR_SELFTEST);
}

TEST(test_init_crc_fail)
{
    mock_reset();
    mock_read_buf[0] = 0xD4;
    mock_read_buf[1] = 0x00;
    mock_read_buf[2] = 0xFF;  /* Wrong CRC */
    mock_read_len = 3;
    sgp40_t d;
    sgp40_err_t e = sgp40_init(&d, &mock_i2c);
    ASSERT_EQ(e, SGP40_ERR_CRC);
}

TEST(test_init_i2c_write_fail)
{
    mock_reset();
    mock_fail_at = 0;
    sgp40_t d;
    sgp40_err_t e = sgp40_init(&d, &mock_i2c);
    ASSERT_EQ(e, SGP40_ERR_I2C);
}

TEST(test_init_i2c_read_fail)
{
    mock_reset();
    mock_fail_at = 1;  /* Write succeeds, read fails */
    sgp40_t d;
    sgp40_err_t e = sgp40_init(&d, &mock_i2c);
    ASSERT_EQ(e, SGP40_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(sgp40_init(NULL, &mock_i2c), SGP40_ERR_PARAM);
    sgp40_t d;
    ASSERT_EQ(sgp40_init(&d, NULL), SGP40_ERR_PARAM);
    sgp40_data_t data;
    ASSERT_EQ(sgp40_measure_raw(NULL, &data), SGP40_ERR_PARAM);
    ASSERT_EQ(sgp40_measure_raw(&d, NULL), SGP40_ERR_PARAM);
    ASSERT_EQ(sgp40_measure_raw_comp(NULL, &data, 0, 0), SGP40_ERR_PARAM);
    ASSERT_EQ(sgp40_measure_raw_comp(&d, NULL, 0, 0), SGP40_ERR_PARAM);
}

/* ---------- measurement tests ---------- */

TEST(test_measure_raw_ok)
{
    setup();
    plant_sraw(30000);
    sgp40_data_t data;
    sgp40_err_t e = sgp40_measure_raw(&dev, &data);
    ASSERT_EQ(e, SGP40_OK);
    ASSERT_EQ(data.sraw_voc, 30000);
}

TEST(test_measure_raw_zero)
{
    setup();
    plant_sraw(0);
    sgp40_data_t data;
    sgp40_err_t e = sgp40_measure_raw(&dev, &data);
    ASSERT_EQ(e, SGP40_OK);
    ASSERT_EQ(data.sraw_voc, 0);
}

TEST(test_measure_raw_max)
{
    setup();
    plant_sraw(65535);
    sgp40_data_t data;
    sgp40_err_t e = sgp40_measure_raw(&dev, &data);
    ASSERT_EQ(e, SGP40_OK);
    ASSERT_EQ(data.sraw_voc, 65535);
}

TEST(test_measure_raw_crc_fail)
{
    setup();
    mock_read_buf[0] = 0x75;
    mock_read_buf[1] = 0x30;
    mock_read_buf[2] = 0xFF;  /* Wrong CRC */
    mock_read_len = 3;
    sgp40_data_t data;
    sgp40_err_t e = sgp40_measure_raw(&dev, &data);
    ASSERT_EQ(e, SGP40_ERR_CRC);
}

TEST(test_measure_raw_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    sgp40_data_t data;
    sgp40_err_t e = sgp40_measure_raw(&dev, &data);
    ASSERT_EQ(e, SGP40_ERR_I2C);
}

TEST(test_measure_comp_cmd_format)
{
    setup();
    plant_sraw(25000);
    sgp40_data_t data;
    sgp40_measure_raw_comp(&dev, &data, 0x8000, 0x6666);
    /* Verify command structure */
    ASSERT_EQ(mock_write_buf[0], 0x26);  /* Measure raw command MSB */
    ASSERT_EQ(mock_write_buf[1], 0x0F);  /* Measure raw command LSB */
    ASSERT_EQ(mock_write_buf[2], 0x80);  /* Humidity MSB */
    ASSERT_EQ(mock_write_buf[3], 0x00);  /* Humidity LSB */
    /* CRC of humidity bytes */
    uint8_t hum_bytes[] = { 0x80, 0x00 };
    ASSERT_EQ(mock_write_buf[4], sgp40_crc8(hum_bytes, 2));
    ASSERT_EQ(mock_write_buf[5], 0x66);  /* Temperature MSB */
    ASSERT_EQ(mock_write_buf[6], 0x66);  /* Temperature LSB */
    /* CRC of temperature bytes */
    uint8_t temp_bytes[] = { 0x66, 0x66 };
    ASSERT_EQ(mock_write_buf[7], sgp40_crc8(temp_bytes, 2));
}

TEST(test_delay_during_selftest)
{
    mock_reset();
    plant_selftest_ok();
    mock_delay_total = 0;
    sgp40_t d;
    sgp40_init(&d, &mock_i2c);
    /* Self-test requires 320ms delay */
    ASSERT_TRUE(mock_delay_total >= 320);
}

TEST(test_delay_during_measure)
{
    setup();
    plant_sraw(20000);
    mock_delay_total = 0;
    sgp40_data_t data;
    sgp40_measure_raw(&dev, &data);
    /* Measurement requires 30ms delay */
    ASSERT_TRUE(mock_delay_total >= 30);
}

TEST(test_selftest_command_bytes)
{
    mock_reset();
    plant_selftest_ok();
    sgp40_t d;
    sgp40_init(&d, &mock_i2c);
    /* Verify self-test command was 0x28, 0x0E */
    ASSERT_EQ(mock_write_buf[0], 0x28);
    ASSERT_EQ(mock_write_buf[1], 0x0E);
}

TEST_MAIN("sgp40")
    RUN(test_crc8_known);
    RUN(test_crc8_zeros);
    RUN(test_crc8_selftest_val);
    RUN(test_init_ok);
    RUN(test_init_selftest_fail);
    RUN(test_init_crc_fail);
    RUN(test_init_i2c_write_fail);
    RUN(test_init_i2c_read_fail);
    RUN(test_null_safety);
    RUN(test_measure_raw_ok);
    RUN(test_measure_raw_zero);
    RUN(test_measure_raw_max);
    RUN(test_measure_raw_crc_fail);
    RUN(test_measure_raw_i2c_fail);
    RUN(test_measure_comp_cmd_format);
    RUN(test_delay_during_selftest);
    RUN(test_delay_during_measure);
    RUN(test_selftest_command_bytes);
TEST_MAIN_END()
