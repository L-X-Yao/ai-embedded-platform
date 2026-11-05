// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_time.h"
#include <string.h>
#include <math.h>
#include "shtc3.h"

/* ---------- mock I2C ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

/* Read response data (up to 6 bytes for ID or measurement) */
static uint8_t mock_read_data[6];
static uint8_t mock_last_write[8];
static int     mock_last_write_len;
static int     mock_write_count;
static int     mock_read_count;

static void mock_reset(void)
{
    mock_fail_next    = 0;
    mock_op_count     = 0;
    mock_fail_at      = -1;
    mock_write_count  = 0;
    mock_read_count   = 0;
    memset(mock_read_data, 0, sizeof(mock_read_data));
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
    mock_write_count++;
    if (len <= sizeof(mock_last_write))
    {
        memcpy(mock_last_write, data, len);
        mock_last_write_len = len;
    }
    return true;
}

static bool mock_read(void *ctx, uint8_t addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    mock_read_count++;
    for (uint16_t i = 0; i < len && i < sizeof(mock_read_data); i++)
    {
        data[i] = mock_read_data[i];
    }
    return true;
}

static const shtc3_i2c_t mock_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay_ms_u16,
    .ctx      = NULL,
};

/* ---------- helpers ---------- */

/*
 * Plant a valid chip ID response in mock_read_data.
 * Chip ID: bits[5:0] = 0x07, bits[11] = 1 -> 0x0807
 * But any value where (val & 0x083F) == 0x0807 is valid.
 */
static void plant_id(uint16_t chip_id)
{
    mock_read_data[0] = (uint8_t)(chip_id >> 8);
    mock_read_data[1] = (uint8_t)(chip_id & 0xFF);
    mock_read_data[2] = shtc3_crc8(&mock_read_data[0], 2);
}

static void plant_reading(uint16_t raw_t, uint16_t raw_h)
{
    mock_read_data[0] = (uint8_t)(raw_t >> 8);
    mock_read_data[1] = (uint8_t)(raw_t & 0xFF);
    mock_read_data[2] = shtc3_crc8(&mock_read_data[0], 2);
    mock_read_data[3] = (uint8_t)(raw_h >> 8);
    mock_read_data[4] = (uint8_t)(raw_h & 0xFF);
    mock_read_data[5] = shtc3_crc8(&mock_read_data[3], 2);
}

static shtc3_t dev;

static void setup(void)
{
    mock_reset();
    plant_id(0x0807);
    shtc3_init(&dev, &mock_i2c);
    /* Reset counters after init for clean test state */
    mock_op_count    = 0;
    mock_fail_at     = -1;
    mock_fail_next   = 0;
    mock_write_count = 0;
    mock_read_count  = 0;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    plant_id(0x0807);
    shtc3_t d;
    shtc3_err_t e = shtc3_init(&d, &mock_i2c);
    ASSERT_EQ(e, SHTC3_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    plant_id(0x0000);  /* Invalid chip ID */
    shtc3_t d;
    shtc3_err_t e = shtc3_init(&d, &mock_i2c);
    ASSERT_EQ(e, SHTC3_ERR_ID);
}

TEST(test_init_id_crc_fail)
{
    mock_reset();
    plant_id(0x0807);
    mock_read_data[2] = 0x00;  /* Corrupt CRC */
    shtc3_t d;
    shtc3_err_t e = shtc3_init(&d, &mock_i2c);
    ASSERT_EQ(e, SHTC3_ERR_CRC);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;  /* Fail on first I2C op (wakeup) */
    shtc3_t d;
    shtc3_err_t e = shtc3_init(&d, &mock_i2c);
    ASSERT_EQ(e, SHTC3_ERR_I2C);
}

TEST(test_null_dev)
{
    ASSERT_EQ(shtc3_init(NULL, &mock_i2c), SHTC3_ERR_PARAM);
}

TEST(test_null_i2c)
{
    shtc3_t d;
    ASSERT_EQ(shtc3_init(&d, NULL), SHTC3_ERR_PARAM);
}

TEST(test_null_i2c_funcs)
{
    shtc3_i2c_t bad = { .write = NULL, .read = NULL, .ctx = NULL };
    shtc3_t d;
    ASSERT_EQ(shtc3_init(&d, &bad), SHTC3_ERR_PARAM);
}

TEST(test_read_null_dev)
{
    shtc3_data_t data;
    ASSERT_EQ(shtc3_read(NULL, &data), SHTC3_ERR_PARAM);
}

TEST(test_read_null_out)
{
    setup();
    ASSERT_EQ(shtc3_read(&dev, NULL), SHTC3_ERR_PARAM);
}

TEST(test_read_25c_50pct)
{
    setup();

    /* raw_t for 25 C: raw = (25+45)/175 * 65536 = 26214 */
    /* raw_h for 50%: raw = 50/100 * 65536 = 32768 */
    plant_reading(26214, 32768);

    shtc3_data_t data;
    shtc3_err_t e = shtc3_read(&dev, &data);
    ASSERT_EQ(e, SHTC3_OK);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.1f);
    ASSERT_NEAR(data.humidity_pct, 50.0f, 0.1f);
}

TEST(test_read_0c_0pct)
{
    setup();

    /* raw_t for 0 C: raw = 45/175 * 65536 = 16852 */
    /* raw_h for 0%: raw = 0 */
    plant_reading(16852, 0);

    shtc3_data_t data;
    shtc3_read(&dev, &data);
    ASSERT_NEAR(data.temperature_c, 0.0f, 0.2f);
    ASSERT_NEAR(data.humidity_pct, 0.0f, 0.1f);
}

TEST(test_read_125c_100pct)
{
    setup();

    /* raw_t for 125 C: raw = (125+45)/175 * 65536 = 63664 */
    /* raw_h for 100%: raw = 65535 is the largest the 16 bits can hold */
    plant_reading(63664, 65535);

    shtc3_data_t data;
    shtc3_read(&dev, &data);
    ASSERT_NEAR(data.temperature_c, 125.0f, 0.2f);
    ASSERT_NEAR(data.humidity_pct, 100.0f, 0.1f);
}

TEST(test_read_minus40c)
{
    setup();

    /* raw_t for -40 C: raw = (-40+45)/175 * 65536 = 1873 */
    plant_reading(1873, 0);

    shtc3_data_t data;
    shtc3_read(&dev, &data);
    ASSERT_NEAR(data.temperature_c, -40.0f, 0.2f);
}

TEST(test_read_full_scale_divides_by_2_16)
{
    /* p10: T = -45 + 175 * ST / 2^16 and RH = 100 * SRH / 2^16, with no
     * "-1" term, so full scale falls just short of 130 C and 100 %RH. */
    setup();
    plant_reading(0xFFFF, 0xFFFF);

    shtc3_data_t data;
    ASSERT_EQ(shtc3_read(&dev, &data), SHTC3_OK);
    ASSERT_NEAR(data.temperature_c, 129.99733f, 0.0005f);
    ASSERT_NEAR(data.humidity_pct, 99.99847f, 0.0005f);
}

TEST(test_read_crc_fail_temp)
{
    setup();
    plant_reading(26214, 32768);
    mock_read_data[2] = 0x00;  /* Corrupt temp CRC */

    shtc3_data_t data;
    shtc3_err_t e = shtc3_read(&dev, &data);
    ASSERT_EQ(e, SHTC3_ERR_CRC);
}

TEST(test_read_crc_fail_humidity)
{
    setup();
    plant_reading(26214, 32768);
    mock_read_data[5] = 0x00;  /* Corrupt humidity CRC */

    shtc3_data_t data;
    shtc3_err_t e = shtc3_read(&dev, &data);
    ASSERT_EQ(e, SHTC3_ERR_CRC);
}

TEST(test_read_i2c_fail_wakeup)
{
    setup();
    mock_fail_next = 1;  /* Fail on wakeup write */
    shtc3_data_t data;
    shtc3_err_t e = shtc3_read(&dev, &data);
    ASSERT_EQ(e, SHTC3_ERR_I2C);
}

TEST(test_read_i2c_fail_data)
{
    setup();
    /* Fail on the 3rd I2C op in read (data read after wakeup + measure cmd) */
    mock_fail_at = 2;
    plant_reading(26214, 32768);
    shtc3_data_t data;
    shtc3_err_t e = shtc3_read(&dev, &data);
    ASSERT_EQ(e, SHTC3_ERR_I2C);
}

TEST(test_sleep_ok)
{
    setup();
    shtc3_err_t e = shtc3_sleep(&dev);
    ASSERT_EQ(e, SHTC3_OK);
    /* Verify sleep command bytes */
    ASSERT_EQ(mock_last_write[0], 0xB0);
    ASSERT_EQ(mock_last_write[1], 0x98);
}

TEST(test_wakeup_ok)
{
    setup();
    shtc3_err_t e = shtc3_wakeup(&dev);
    ASSERT_EQ(e, SHTC3_OK);
    /* Verify wakeup command bytes */
    ASSERT_EQ(mock_last_write[0], 0x35);
    ASSERT_EQ(mock_last_write[1], 0x17);
}

TEST(test_reset_ok)
{
    setup();
    shtc3_err_t e = shtc3_reset(&dev);
    ASSERT_EQ(e, SHTC3_OK);
    /* Verify reset command bytes */
    ASSERT_EQ(mock_last_write[0], 0x80);
    ASSERT_EQ(mock_last_write[1], 0x5D);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    shtc3_err_t e = shtc3_reset(&dev);
    ASSERT_EQ(e, SHTC3_ERR_I2C);
}

TEST(test_sleep_null)
{
    ASSERT_EQ(shtc3_sleep(NULL), SHTC3_ERR_PARAM);
}

TEST(test_wakeup_null)
{
    ASSERT_EQ(shtc3_wakeup(NULL), SHTC3_ERR_PARAM);
}

TEST(test_reset_null)
{
    ASSERT_EQ(shtc3_reset(NULL), SHTC3_ERR_PARAM);
}

TEST(test_crc8_known_vector)
{
    /* Sensirion CRC-8: data {0xBE, 0xEF} -> CRC = 0x92 */
    uint8_t data[] = { 0xBE, 0xEF };
    uint8_t crc = shtc3_crc8(data, 2);
    ASSERT_EQ(crc, 0x92);
}

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)SHTC3_OK);
    ASSERT_EQ(-1, (int)SHTC3_ERR_I2C);
    ASSERT_EQ(-2, (int)SHTC3_ERR_ID);
    ASSERT_EQ(-3, (int)SHTC3_ERR_PARAM);
    ASSERT_EQ(-4, (int)SHTC3_ERR_CRC);
}

TEST(test_init_valid_id_variant)
{
    /* SHTC3 ID: any value where (val & 0x083F) == 0x0807 is valid.
     * 0x08C7 = 0x0807 | 0x00C0 has extra bits in non-masked positions. */
    mock_reset();
    plant_id(0x08C7);
    shtc3_t d;
    shtc3_err_t e = shtc3_init(&d, &mock_i2c);
    ASSERT_EQ(e, SHTC3_OK);
}

TEST(test_read_sends_measure_cmd)
{
    setup();
    plant_reading(26214, 32768);

    shtc3_data_t data;
    shtc3_read(&dev, &data);

    /* The last write before the read should be the sleep command (0xB098),
     * but the measurement command (0x7866) was sent before that.
     * We can verify the sleep command was the last thing sent. */
    ASSERT_EQ(mock_last_write[0], 0xB0);
    ASSERT_EQ(mock_last_write[1], 0x98);
}

TEST_MAIN("shtc3")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_id_crc_fail);
    RUN(test_init_i2c_fail);
    RUN(test_null_dev);
    RUN(test_null_i2c);
    RUN(test_null_i2c_funcs);
    RUN(test_read_null_dev);
    RUN(test_read_null_out);
    RUN(test_read_25c_50pct);
    RUN(test_read_0c_0pct);
    RUN(test_read_125c_100pct);
    RUN(test_read_minus40c);
    RUN(test_read_full_scale_divides_by_2_16);
    RUN(test_read_crc_fail_temp);
    RUN(test_read_crc_fail_humidity);
    RUN(test_read_i2c_fail_wakeup);
    RUN(test_read_i2c_fail_data);
    RUN(test_sleep_ok);
    RUN(test_wakeup_ok);
    RUN(test_reset_ok);
    RUN(test_reset_i2c_fail);
    RUN(test_sleep_null);
    RUN(test_wakeup_null);
    RUN(test_reset_null);
    RUN(test_crc8_known_vector);
    RUN(test_error_codes);
    RUN(test_init_valid_id_variant);
    RUN(test_read_sends_measure_cmd);
TEST_MAIN_END()
