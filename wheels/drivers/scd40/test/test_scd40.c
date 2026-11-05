// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_time.h"
#include <string.h>
#include <math.h>
#include "scd40.h"

/* ---------- mock I2C (command-based, not register-based) ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

#define MOCK_READ_BUF_SIZE  16
static uint8_t mock_read_data[MOCK_READ_BUF_SIZE];
static uint16_t mock_read_len;

#define MOCK_WRITE_BUF_SIZE 16
static uint8_t mock_last_write[MOCK_WRITE_BUF_SIZE];
static int     mock_last_write_len;

static void mock_reset(void)
{
    mock_fail_next      = 0;
    mock_op_count       = 0;
    mock_fail_at        = -1;
    memset(mock_read_data, 0, sizeof(mock_read_data));
    mock_read_len       = 0;
    memset(mock_last_write, 0, sizeof(mock_last_write));
    mock_last_write_len = 0;
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
    if (len <= MOCK_WRITE_BUF_SIZE)
    {
        memcpy(mock_last_write, data, len);
        mock_last_write_len = (int)len;
    }
    return true;
}

static bool mock_read(void *ctx, uint8_t addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    for (uint16_t i = 0; i < len && i < MOCK_READ_BUF_SIZE; i++)
    {
        data[i] = mock_read_data[i];
    }
    return true;
}

static const scd40_i2c_t mock_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

/* ---------- helpers ---------- */

static scd40_t dev;

static void setup(void)
{
    mock_reset();
    scd40_init(&dev, &mock_i2c);
    mock_op_count = 0;   /* reset count after init */
}

/** Plant a 3-word measurement response (CO2, temp, humidity) with correct CRCs. */
static void plant_measurement(uint16_t co2_raw, uint16_t temp_raw, uint16_t hum_raw)
{
    mock_read_data[0] = (uint8_t)(co2_raw >> 8);
    mock_read_data[1] = (uint8_t)(co2_raw & 0xFF);
    mock_read_data[2] = scd40_crc8(&mock_read_data[0], 2);
    mock_read_data[3] = (uint8_t)(temp_raw >> 8);
    mock_read_data[4] = (uint8_t)(temp_raw & 0xFF);
    mock_read_data[5] = scd40_crc8(&mock_read_data[3], 2);
    mock_read_data[6] = (uint8_t)(hum_raw >> 8);
    mock_read_data[7] = (uint8_t)(hum_raw & 0xFF);
    mock_read_data[8] = scd40_crc8(&mock_read_data[6], 2);
}

/** Plant a 1-word status response with correct CRC. */
static void plant_status(uint16_t status)
{
    mock_read_data[0] = (uint8_t)(status >> 8);
    mock_read_data[1] = (uint8_t)(status & 0xFF);
    mock_read_data[2] = scd40_crc8(&mock_read_data[0], 2);
}

/** Plant a 3-word serial response with correct CRCs. */
static void plant_serial(uint16_t w0, uint16_t w1, uint16_t w2)
{
    mock_read_data[0] = (uint8_t)(w0 >> 8);
    mock_read_data[1] = (uint8_t)(w0 & 0xFF);
    mock_read_data[2] = scd40_crc8(&mock_read_data[0], 2);
    mock_read_data[3] = (uint8_t)(w1 >> 8);
    mock_read_data[4] = (uint8_t)(w1 & 0xFF);
    mock_read_data[5] = scd40_crc8(&mock_read_data[3], 2);
    mock_read_data[6] = (uint8_t)(w2 >> 8);
    mock_read_data[7] = (uint8_t)(w2 & 0xFF);
    mock_read_data[8] = scd40_crc8(&mock_read_data[6], 2);
}

/* ---------- tests ---------- */

TEST(test_init_null)
{
    ASSERT_EQ(scd40_init(NULL, &mock_i2c), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_init(&dev, NULL), SCD40_ERR_PARAM);
    scd40_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ASSERT_TRUE(scd40_init(&dev, &zero_hal) < 0);
}

TEST(test_init_ok)
{
    mock_reset();
    scd40_t d;
    scd40_err_t e = scd40_init(&d, &mock_i2c);
    ASSERT_EQ(e, SCD40_OK);
}

TEST(test_init_requires_delay)
{
    /* p7: "it is required to wait the command execution time before
     * issuing the read header", so a HAL without a delay cannot work. */
    scd40_i2c_t no_delay = mock_i2c;
    no_delay.delay_ms = NULL;
    scd40_t d;
    ASSERT_EQ(scd40_init(&d, &no_delay), SCD40_ERR_PARAM);
}

TEST(test_start_stop_periodic)
{
    setup();

    scd40_err_t e = scd40_start_periodic(&dev);
    ASSERT_EQ(e, SCD40_OK);
    /* Verify start_periodic command bytes: 0x21 0xB1 */
    ASSERT_EQ(mock_last_write[0], 0x21);
    ASSERT_EQ(mock_last_write[1], 0xB1);
    ASSERT_EQ(mock_last_write_len, 2);

    e = scd40_stop_periodic(&dev);
    ASSERT_EQ(e, SCD40_OK);
    /* Verify stop_periodic command bytes: 0x3F 0x86 */
    ASSERT_EQ(mock_last_write[0], 0x3F);
    ASSERT_EQ(mock_last_write[1], 0x86);
    ASSERT_EQ(mock_last_write_len, 2);
}

TEST(test_stop_periodic_waits)
{
    /* p10: "the sensor will only respond to other commands 500 ms after
     * the stop_periodic_measurement command has been issued" */
    setup();
    mock_time_reset();
    ASSERT_EQ(scd40_stop_periodic(&dev), SCD40_OK);
    ASSERT_EQ(mock_time_delay_total_ms, 500);
}

TEST(test_set_altitude_waits)
{
    /* Table 9, p8: set_sensor_altitude has a 1 ms execution time, and p7
     * forbids a new command while the previous one is being processed */
    setup();
    mock_time_reset();
    ASSERT_EQ(scd40_set_altitude(&dev, 1500), SCD40_OK);
    ASSERT_EQ(mock_time_delay_total_ms, 1);
}

TEST(test_read_measurement)
{
    setup();

    /* Plant realistic values:
     * CO2 = 415 ppm  (raw = 415)
     * Temp = 25 C  -> raw = (25 + 45) / 175 * 65535 = 26214
     * Hum  = 50%   -> raw = 50 / 100 * 65535 = 32768
     */
    plant_measurement(415, 26214, 32768);

    scd40_data_t data;
    scd40_err_t e = scd40_read(&dev, &data);
    ASSERT_EQ(e, SCD40_OK);
    ASSERT_EQ(data.co2_ppm, 415);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.1f);
    ASSERT_NEAR(data.humidity_pct, 50.0f, 0.1f);
}

TEST(test_data_ready)
{
    setup();
    bool ready = true;

    /* Table 23, p15: response 0x8000 0xa2 means the low 11 bits are 0,
     * so data is not ready. Bytes are the manual's, CRC included. */
    mock_read_data[0] = 0x80;
    mock_read_data[1] = 0x00;
    mock_read_data[2] = 0xA2;
    scd40_err_t e = scd40_data_ready(&dev, &ready);
    ASSERT_EQ(e, SCD40_OK);
    ASSERT_FALSE(ready);

    /* Only the low 11 bits count, whatever the top 5 hold */
    static const uint16_t not_ready[] = { 0x0000, 0x0800, 0xF800 };
    for (size_t i = 0; i < sizeof(not_ready) / sizeof(not_ready[0]); i++)
    {
        plant_status(not_ready[i]);
        mock_op_count = 0;
        ready = true;
        e = scd40_data_ready(&dev, &ready);
        ASSERT_EQ(e, SCD40_OK);
        ASSERT_FALSE(ready);
    }

    /* Any low bit set means ready, with the top bits clear or set */
    static const uint16_t is_ready[] = { 0x0001, 0x0400, 0x07FF, 0x8006 };
    for (size_t i = 0; i < sizeof(is_ready) / sizeof(is_ready[0]); i++)
    {
        plant_status(is_ready[i]);
        mock_op_count = 0;
        ready = false;
        e = scd40_data_ready(&dev, &ready);
        ASSERT_EQ(e, SCD40_OK);
        ASSERT_TRUE(ready);
    }

    /* Verify command bytes: 0xE4 0xB8 */
    ASSERT_EQ(mock_last_write[0], 0xE4);
    ASSERT_EQ(mock_last_write[1], 0xB8);
}

TEST(test_get_serial)
{
    setup();

    plant_serial(0x1234, 0x5678, 0x9ABC);

    uint16_t serial[3] = {0};
    scd40_err_t e = scd40_get_serial(&dev, serial);
    ASSERT_EQ(e, SCD40_OK);
    ASSERT_EQ_HEX(serial[0], 0x1234);
    ASSERT_EQ_HEX(serial[1], 0x5678);
    ASSERT_EQ_HEX(serial[2], 0x9ABC);

    /* Verify command bytes: 0x36 0x82 */
    ASSERT_EQ(mock_last_write[0], 0x36);
    ASSERT_EQ(mock_last_write[1], 0x82);
}

TEST(test_crc_error)
{
    setup();

    /* Plant measurement with valid CRCs, then corrupt one */
    plant_measurement(415, 26214, 32768);
    mock_read_data[2] = 0x00;  /* corrupt CO2 CRC */

    scd40_data_t data;
    scd40_err_t e = scd40_read(&dev, &data);
    ASSERT_EQ(e, SCD40_ERR_CRC);

    /* Also corrupt temp CRC with valid CO2 */
    plant_measurement(415, 26214, 32768);
    mock_read_data[5] = 0x00;  /* corrupt temp CRC */
    mock_op_count = 0;

    e = scd40_read(&dev, &data);
    ASSERT_EQ(e, SCD40_ERR_CRC);

    /* Also corrupt humidity CRC */
    plant_measurement(415, 26214, 32768);
    mock_read_data[8] = 0x00;  /* corrupt humidity CRC */
    mock_op_count = 0;

    e = scd40_read(&dev, &data);
    ASSERT_EQ(e, SCD40_ERR_CRC);
}

TEST(test_set_altitude)
{
    setup();

    scd40_err_t e = scd40_set_altitude(&dev, 1500);
    ASSERT_EQ(e, SCD40_OK);

    /* Verify command: 0x24 0x27 + arg MSB LSB + CRC */
    ASSERT_EQ(mock_last_write[0], 0x24);
    ASSERT_EQ(mock_last_write[1], 0x27);
    /* 1500 = 0x05DC */
    ASSERT_EQ(mock_last_write[2], 0x05);
    ASSERT_EQ(mock_last_write[3], 0xDC);
    /* Verify CRC of the argument bytes */
    uint8_t arg_bytes[2] = { 0x05, 0xDC };
    ASSERT_EQ(mock_last_write[4], scd40_crc8(arg_bytes, 2));
    ASSERT_EQ(mock_last_write_len, 5);
}

TEST(test_set_altitude_range)
{
    /* p11: "Valid input values are between 0 - 3000 m." */
    setup();
    ASSERT_EQ(scd40_set_altitude(&dev, 0), SCD40_OK);
    ASSERT_EQ(scd40_set_altitude(&dev, 3000), SCD40_OK);

    mock_last_write_len = 0;
    ASSERT_EQ(scd40_set_altitude(&dev, 3001), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_set_altitude(&dev, 0xFFFF), SCD40_ERR_PARAM);
    ASSERT_EQ(mock_last_write_len, 0);   /* nothing reached the sensor */
}

TEST(test_perform_reset)
{
    setup();
    mock_time_reset();

    scd40_err_t e = scd40_perform_reset(&dev);
    ASSERT_EQ(e, SCD40_OK);
    /* Table 9, p8: reinit is 0x3646 with a 30 ms execution time */
    ASSERT_EQ_HEX(mock_last_write[0], 0x36);
    ASSERT_EQ_HEX(mock_last_write[1], 0x46);
    ASSERT_EQ(mock_last_write_len, 2);
    ASSERT_EQ(mock_time_delay_total_ms, 30);
}

TEST(test_null_params_all_apis)
{
    scd40_data_t data;
    bool ready;
    uint16_t serial[3];

    ASSERT_EQ(scd40_start_periodic(NULL), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_stop_periodic(NULL), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_read(NULL, &data), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_read(&dev, NULL), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_data_ready(NULL, &ready), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_data_ready(&dev, NULL), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_get_serial(NULL, serial), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_get_serial(&dev, NULL), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_set_altitude(NULL, 0), SCD40_ERR_PARAM);
    ASSERT_EQ(scd40_perform_reset(NULL), SCD40_ERR_PARAM);
}

TEST(test_i2c_fail_propagation)
{
    setup();

    /* start_periodic I2C fail */
    mock_fail_next = 1;
    ASSERT_EQ(scd40_start_periodic(&dev), SCD40_ERR_I2C);

    /* read: command write fails */
    mock_fail_next = 1;
    scd40_data_t data;
    ASSERT_EQ(scd40_read(&dev, &data), SCD40_ERR_I2C);

    /* read: data read fails (fail at op index 1 = the read after write) */
    mock_op_count = 0;
    mock_fail_at = 1;
    plant_measurement(415, 26214, 32768);
    ASSERT_EQ(scd40_read(&dev, &data), SCD40_ERR_I2C);
}

TEST(test_crc8_known_vector)
{
    /* Sensirion CRC-8 test vector: 0xBEEF -> 0x92 */
    uint8_t data[] = { 0xBE, 0xEF };
    uint8_t crc = scd40_crc8(data, 2);
    ASSERT_EQ(crc, 0x92);
}

TEST(test_read_boundary_values)
{
    setup();

    /* Min values: CO2=0, temp raw=0 (-45C), hum raw=0 (0%) */
    plant_measurement(0, 0, 0);
    scd40_data_t data;
    scd40_err_t e = scd40_read(&dev, &data);
    ASSERT_EQ(e, SCD40_OK);
    ASSERT_EQ(data.co2_ppm, 0);
    ASSERT_NEAR(data.temperature_c, -45.0f, 0.1f);
    ASSERT_NEAR(data.humidity_pct, 0.0f, 0.1f);

    /* Max values: CO2=40000, temp raw=65535 (130C), hum raw=65535 (100%) */
    mock_op_count = 0;
    plant_measurement(40000, 65535, 65535);
    e = scd40_read(&dev, &data);
    ASSERT_EQ(e, SCD40_OK);
    ASSERT_EQ(data.co2_ppm, 40000);
    ASSERT_NEAR(data.temperature_c, 130.0f, 0.1f);
    ASSERT_NEAR(data.humidity_pct, 100.0f, 0.1f);
}

TEST_MAIN("scd40")
    RUN(test_init_null);
    RUN(test_init_ok);
    RUN(test_init_requires_delay);
    RUN(test_start_stop_periodic);
    RUN(test_stop_periodic_waits);
    RUN(test_set_altitude_waits);
    RUN(test_read_measurement);
    RUN(test_data_ready);
    RUN(test_get_serial);
    RUN(test_crc_error);
    RUN(test_set_altitude);
    RUN(test_set_altitude_range);
    RUN(test_perform_reset);
    RUN(test_null_params_all_apis);
    RUN(test_i2c_fail_propagation);
    RUN(test_crc8_known_vector);
    RUN(test_read_boundary_values);
TEST_MAIN_END()
