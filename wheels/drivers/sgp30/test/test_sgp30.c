// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_time.h"
#include <string.h>
#include "sgp30.h"

/* ---------- mock I2C (raw write/read, not register-based) ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

/* Capture last write for command verification */
static uint8_t mock_last_write[16];
static int     mock_last_write_len;

/* Pre-loaded read response */
static uint8_t mock_read_data[32];
static int     mock_read_data_len;

static void mock_reset(void)
{
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;
    memset(mock_last_write, 0, sizeof(mock_last_write));
    mock_last_write_len = 0;
    memset(mock_read_data, 0, sizeof(mock_read_data));
    mock_read_data_len = 0;
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write(void *ctx, uint8_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    if (len <= sizeof(mock_last_write)) {
        memcpy(mock_last_write, data, len);
        mock_last_write_len = (int)len;
    }
    return true;
}

static bool mock_read(void *ctx, uint8_t addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    for (uint16_t i = 0; i < len && i < (uint16_t)sizeof(mock_read_data); i++) {
        data[i] = mock_read_data[i];
    }
    return true;
}

static const sgp30_i2c_t mock_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay_ms_u16,
    .ctx      = NULL,
};

static sgp30_t dev;

/* ---------- helpers ---------- */

/** Plant a 2-word read response (e.g. measure_iaq: eCO2 + TVOC) */
static void plant_iaq_response(uint16_t eco2, uint16_t tvoc)
{
    uint8_t w0[2] = { (uint8_t)(eco2 >> 8), (uint8_t)(eco2 & 0xFF) };
    uint8_t w1[2] = { (uint8_t)(tvoc >> 8), (uint8_t)(tvoc & 0xFF) };
    mock_read_data[0] = w0[0];
    mock_read_data[1] = w0[1];
    mock_read_data[2] = sgp30_crc8(w0, 2);
    mock_read_data[3] = w1[0];
    mock_read_data[4] = w1[1];
    mock_read_data[5] = sgp30_crc8(w1, 2);
    mock_read_data_len = 6;
}

/** Plant a 3-word read response (serial ID: 3 x 2 bytes) */
static void plant_serial_response(uint16_t w0, uint16_t w1, uint16_t w2)
{
    uint8_t d0[2] = { (uint8_t)(w0 >> 8), (uint8_t)(w0 & 0xFF) };
    uint8_t d1[2] = { (uint8_t)(w1 >> 8), (uint8_t)(w1 & 0xFF) };
    uint8_t d2[2] = { (uint8_t)(w2 >> 8), (uint8_t)(w2 & 0xFF) };
    mock_read_data[0] = d0[0]; mock_read_data[1] = d0[1];
    mock_read_data[2] = sgp30_crc8(d0, 2);
    mock_read_data[3] = d1[0]; mock_read_data[4] = d1[1];
    mock_read_data[5] = sgp30_crc8(d1, 2);
    mock_read_data[6] = d2[0]; mock_read_data[7] = d2[1];
    mock_read_data[8] = sgp30_crc8(d2, 2);
    mock_read_data_len = 9;
}

static void setup(void)
{
    mock_reset();
    mock_time_reset();
    sgp30_init(&dev, &mock_i2c);
    mock_op_count = 0;  /* reset after init ops */
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    mock_time_reset();
    sgp30_t d;
    sgp30_err_t e = sgp30_init(&d, &mock_i2c);
    ASSERT_EQ(e, SGP30_OK);
    /* Verify iaq_init command 0x2003 was sent */
    ASSERT_EQ(mock_last_write[0], 0x20);
    ASSERT_EQ(mock_last_write[1], 0x03);
    ASSERT_EQ(mock_last_write_len, 2);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    sgp30_t d;
    sgp30_err_t e = sgp30_init(&d, &mock_i2c);
    ASSERT_EQ(e, SGP30_ERR_I2C);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(sgp30_init(NULL, &mock_i2c), SGP30_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    sgp30_t d;
    ASSERT_EQ(sgp30_init(&d, NULL), SGP30_ERR_PARAM);
}

TEST(test_init_null_write_fn)
{
    sgp30_i2c_t bad = { .write = NULL, .read = mock_read,
                        .delay_ms = mock_delay_ms_u16, .ctx = NULL };
    sgp30_t d;
    ASSERT_EQ(sgp30_init(&d, &bad), SGP30_ERR_PARAM);
}

TEST(test_init_null_read_fn)
{
    sgp30_i2c_t bad = { .write = mock_write, .read = NULL,
                        .delay_ms = mock_delay_ms_u16, .ctx = NULL };
    sgp30_t d;
    ASSERT_EQ(sgp30_init(&d, &bad), SGP30_ERR_PARAM);
}

TEST(test_init_null_delay_fn)
{
    /* p8: "When the measurement is in progress, no communication with the
     * sensor is possible", so a HAL with no delay cannot work. */
    mock_reset();
    sgp30_i2c_t bad = { .write = mock_write, .read = mock_read,
                        .delay_ms = NULL, .ctx = NULL };
    sgp30_t d;
    ASSERT_EQ(sgp30_init(&d, &bad), SGP30_ERR_PARAM);
    ASSERT_EQ(mock_last_write_len, 0);   /* iaq_init was not sent */
}

TEST(test_measure_iaq_ok)
{
    setup();
    plant_iaq_response(400, 0);  /* initial: eCO2=400ppm, TVOC=0ppb */

    sgp30_data_t data;
    sgp30_err_t e = sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ(data.eco2_ppm, 400);
    ASSERT_EQ(data.tvoc_ppb, 0);
}

TEST(test_measure_iaq_high_values)
{
    setup();
    plant_iaq_response(5000, 1200);

    sgp30_data_t data;
    sgp30_err_t e = sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ(data.eco2_ppm, 5000);
    ASSERT_EQ(data.tvoc_ppb, 1200);
}

TEST(test_measure_iaq_max_values)
{
    setup();
    plant_iaq_response(60000, 60000);

    sgp30_data_t data;
    sgp30_err_t e = sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ(data.eco2_ppm, 60000);
    ASSERT_EQ(data.tvoc_ppb, 60000);
}

TEST(test_measure_iaq_sends_cmd)
{
    setup();
    plant_iaq_response(400, 0);

    sgp30_data_t data;
    sgp30_measure_iaq(&dev, &data);
    /* Last write before read should be the measure command */
    ASSERT_EQ(mock_last_write[0], 0x20);
    ASSERT_EQ(mock_last_write[1], 0x08);
}

TEST(test_measure_iaq_cmd_fail)
{
    setup();
    mock_fail_next = 1;  /* fail the write (measure command) */
    sgp30_data_t data;
    sgp30_err_t e = sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(e, SGP30_ERR_I2C);
}

TEST(test_measure_iaq_read_fail)
{
    setup();
    mock_fail_at = 1;  /* pass write (op 0), fail read (op 1) */
    sgp30_data_t data;
    sgp30_err_t e = sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(e, SGP30_ERR_I2C);
}

TEST(test_measure_iaq_crc_fail_word0)
{
    setup();
    plant_iaq_response(400, 0);
    mock_read_data[2] = 0x00;  /* corrupt CRC of first word */

    sgp30_data_t data;
    sgp30_err_t e = sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(e, SGP30_ERR_CRC);
}

TEST(test_measure_iaq_crc_fail_word1)
{
    setup();
    plant_iaq_response(400, 0);
    mock_read_data[5] = 0x00;  /* corrupt CRC of second word */

    sgp30_data_t data;
    sgp30_err_t e = sgp30_measure_iaq(&dev, &data);
    ASSERT_EQ(e, SGP30_ERR_CRC);
}

TEST(test_measure_null_dev)
{
    sgp30_data_t data;
    ASSERT_EQ(sgp30_measure_iaq(NULL, &data), SGP30_ERR_PARAM);
}

TEST(test_measure_null_out)
{
    setup();
    ASSERT_EQ(sgp30_measure_iaq(&dev, NULL), SGP30_ERR_PARAM);
}

TEST(test_get_baseline_ok)
{
    setup();
    plant_iaq_response(0xABCD, 0x1234);  /* reuse: eco2_bl=0xABCD, tvoc_bl=0x1234 */

    sgp30_baseline_t bl;
    sgp30_err_t e = sgp30_get_baseline(&dev, &bl);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ_HEX(bl.eco2_baseline, 0xABCD);
    ASSERT_EQ_HEX(bl.tvoc_baseline, 0x1234);
}

TEST(test_get_baseline_sends_cmd)
{
    setup();
    plant_iaq_response(0, 0);
    sgp30_baseline_t bl;
    sgp30_get_baseline(&dev, &bl);
    ASSERT_EQ(mock_last_write[0], 0x20);
    ASSERT_EQ(mock_last_write[1], 0x15);
}

TEST(test_get_baseline_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    sgp30_baseline_t bl;
    ASSERT_EQ(sgp30_get_baseline(&dev, &bl), SGP30_ERR_I2C);
}

TEST(test_get_baseline_null)
{
    setup();
    sgp30_baseline_t bl;
    ASSERT_EQ(sgp30_get_baseline(NULL, &bl), SGP30_ERR_PARAM);
    ASSERT_EQ(sgp30_get_baseline(&dev, NULL), SGP30_ERR_PARAM);
}

TEST(test_set_baseline_ok)
{
    setup();
    sgp30_baseline_t bl = { .tvoc_baseline = 0x1234, .eco2_baseline = 0xABCD };
    sgp30_err_t e = sgp30_set_baseline(&dev, &bl);
    ASSERT_EQ(e, SGP30_OK);
    /* Command: 0x201E + TVOC word + CRC + eCO2 word + CRC */
    ASSERT_EQ(mock_last_write[0], 0x20);
    ASSERT_EQ(mock_last_write[1], 0x1E);
    /* TVOC baseline first in set_baseline */
    ASSERT_EQ(mock_last_write[2], 0x12);
    ASSERT_EQ(mock_last_write[3], 0x34);
    /* CRC of {0x12, 0x34} */
    uint8_t tvoc_pair[2] = { 0x12, 0x34 };
    ASSERT_EQ(mock_last_write[4], sgp30_crc8(tvoc_pair, 2));
    /* eCO2 baseline */
    ASSERT_EQ(mock_last_write[5], 0xAB);
    ASSERT_EQ(mock_last_write[6], 0xCD);
    uint8_t eco2_pair[2] = { 0xAB, 0xCD };
    ASSERT_EQ(mock_last_write[7], sgp30_crc8(eco2_pair, 2));
}

TEST(test_set_baseline_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    sgp30_baseline_t bl = { .tvoc_baseline = 0, .eco2_baseline = 0 };
    ASSERT_EQ(sgp30_set_baseline(&dev, &bl), SGP30_ERR_I2C);
}

TEST(test_set_baseline_null)
{
    setup();
    sgp30_baseline_t bl = { 0 };
    ASSERT_EQ(sgp30_set_baseline(NULL, &bl), SGP30_ERR_PARAM);
    ASSERT_EQ(sgp30_set_baseline(&dev, NULL), SGP30_ERR_PARAM);
}

TEST(test_get_serial_id_ok)
{
    setup();
    plant_serial_response(0x0000, 0x0140, 0x7FCA);

    sgp30_serial_t serial;
    sgp30_err_t e = sgp30_get_serial_id(&dev, &serial);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ(serial.id[0], 0x00);
    ASSERT_EQ(serial.id[1], 0x00);
    ASSERT_EQ(serial.id[2], 0x01);
    ASSERT_EQ(serial.id[3], 0x40);
    ASSERT_EQ(serial.id[4], 0x7F);
    ASSERT_EQ(serial.id[5], 0xCA);
}

TEST(test_get_serial_sends_cmd)
{
    setup();
    plant_serial_response(0, 0, 0);
    sgp30_serial_t serial;
    sgp30_get_serial_id(&dev, &serial);
    ASSERT_EQ(mock_last_write[0], 0x36);
    ASSERT_EQ(mock_last_write[1], 0x82);
}

TEST(test_get_serial_crc_fail)
{
    setup();
    plant_serial_response(0x0000, 0x0140, 0x7FCA);
    mock_read_data[8] = 0x00;  /* corrupt CRC of third word */

    sgp30_serial_t serial;
    ASSERT_EQ(sgp30_get_serial_id(&dev, &serial), SGP30_ERR_CRC);
}

TEST(test_get_serial_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    sgp30_serial_t serial;
    ASSERT_EQ(sgp30_get_serial_id(&dev, &serial), SGP30_ERR_I2C);
}

TEST(test_get_serial_null)
{
    setup();
    sgp30_serial_t serial;
    ASSERT_EQ(sgp30_get_serial_id(NULL, &serial), SGP30_ERR_PARAM);
    ASSERT_EQ(sgp30_get_serial_id(&dev, NULL), SGP30_ERR_PARAM);
}

TEST(test_set_humidity_ok)
{
    setup();
    /* 15.5 g/m3 = 0x0F80 in 8.8 fixed point */
    sgp30_err_t e = sgp30_set_humidity(&dev, 0x0F80);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ(mock_last_write[0], 0x20);
    ASSERT_EQ(mock_last_write[1], 0x61);
    ASSERT_EQ(mock_last_write[2], 0x0F);
    ASSERT_EQ(mock_last_write[3], 0x80);
    uint8_t hum_pair[2] = { 0x0F, 0x80 };
    ASSERT_EQ(mock_last_write[4], sgp30_crc8(hum_pair, 2));
}

TEST(test_set_humidity_disable)
{
    setup();
    /* 0x0000 disables humidity compensation */
    sgp30_err_t e = sgp30_set_humidity(&dev, 0x0000);
    ASSERT_EQ(e, SGP30_OK);
    ASSERT_EQ(mock_last_write[2], 0x00);
    ASSERT_EQ(mock_last_write[3], 0x00);
}

TEST(test_set_humidity_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(sgp30_set_humidity(&dev, 0x0F80), SGP30_ERR_I2C);
}

TEST(test_set_humidity_null)
{
    ASSERT_EQ(sgp30_set_humidity(NULL, 0x0F80), SGP30_ERR_PARAM);
}

TEST(test_crc8_known_vector)
{
    /* Sensirion datasheet: CRC of {0xBE, 0xEF} = 0x92 */
    uint8_t data[] = { 0xBE, 0xEF };
    ASSERT_EQ(sgp30_crc8(data, 2), 0x92);
}

TEST(test_crc8_zero)
{
    uint8_t data[] = { 0x00, 0x00 };
    uint8_t crc = sgp30_crc8(data, 2);
    /* Verify CRC is deterministic and non-zero for zero input */
    ASSERT_EQ(crc, sgp30_crc8(data, 2));
    /* CRC(0x00, 0x00) with init=0xFF, poly=0x31 = 0x81 */
    ASSERT_EQ_HEX(crc, 0x81);
}

TEST(test_crc8_ff)
{
    uint8_t data[] = { 0xFF, 0xFF };
    uint8_t crc = sgp30_crc8(data, 2);
    /* Verify deterministic */
    ASSERT_EQ(crc, sgp30_crc8(data, 2));
}

TEST(test_init_delay_called)
{
    mock_reset();
    mock_time_reset();
    sgp30_t d;
    sgp30_init(&d, &mock_i2c);
    /* init should delay 10ms after iaq_init command */
    ASSERT_TRUE(mock_time_delay_total_ms >= 10);
}

TEST(test_measure_delay_called)
{
    setup();
    mock_time_reset();
    plant_iaq_response(400, 0);
    sgp30_data_t data;
    sgp30_measure_iaq(&dev, &data);
    /* measure should delay 12ms for conversion */
    ASSERT_TRUE(mock_time_delay_total_ms >= 12);
}

TEST(test_zero_init_safety)
{
    sgp30_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    /* Zero-init struct: i2c is NULL, so param check catches it */
    sgp30_data_t data;
    ASSERT_EQ(sgp30_measure_iaq(&zero_dev, &data), SGP30_ERR_PARAM);
}

TEST_MAIN("sgp30")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_null_write_fn);
    RUN(test_init_null_read_fn);
    RUN(test_init_null_delay_fn);
    RUN(test_measure_iaq_ok);
    RUN(test_measure_iaq_high_values);
    RUN(test_measure_iaq_max_values);
    RUN(test_measure_iaq_sends_cmd);
    RUN(test_measure_iaq_cmd_fail);
    RUN(test_measure_iaq_read_fail);
    RUN(test_measure_iaq_crc_fail_word0);
    RUN(test_measure_iaq_crc_fail_word1);
    RUN(test_measure_null_dev);
    RUN(test_measure_null_out);
    RUN(test_get_baseline_ok);
    RUN(test_get_baseline_sends_cmd);
    RUN(test_get_baseline_i2c_fail);
    RUN(test_get_baseline_null);
    RUN(test_set_baseline_ok);
    RUN(test_set_baseline_i2c_fail);
    RUN(test_set_baseline_null);
    RUN(test_get_serial_id_ok);
    RUN(test_get_serial_sends_cmd);
    RUN(test_get_serial_crc_fail);
    RUN(test_get_serial_i2c_fail);
    RUN(test_get_serial_null);
    RUN(test_set_humidity_ok);
    RUN(test_set_humidity_disable);
    RUN(test_set_humidity_i2c_fail);
    RUN(test_set_humidity_null);
    RUN(test_crc8_known_vector);
    RUN(test_crc8_zero);
    RUN(test_crc8_ff);
    RUN(test_init_delay_called);
    RUN(test_measure_delay_called);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
