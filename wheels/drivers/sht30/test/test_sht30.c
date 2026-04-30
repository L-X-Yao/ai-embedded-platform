#include "test_harness.h"
#include <string.h>
#include <math.h>
#include "sht30.h"

/* ---------- mock I2C ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

static uint8_t mock_read_data[6];
static uint8_t mock_last_write[8];
static int     mock_last_write_len;

static void mock_reset(void)
{
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;
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
    for (uint16_t i = 0; i < len && i < sizeof(mock_read_data); i++)
    {
        data[i] = mock_read_data[i];
    }
    return true;
}

static const sht30_i2c_t mock_i2c = {
    .write = mock_write,
    .read  = mock_read,
    .ctx   = NULL,
};

static void plant_reading(uint16_t raw_t, uint16_t raw_h)
{
    mock_read_data[0] = (uint8_t)(raw_t >> 8);
    mock_read_data[1] = (uint8_t)(raw_t & 0xFF);
    mock_read_data[2] = sht30_crc8(&mock_read_data[0], 2);
    mock_read_data[3] = (uint8_t)(raw_h >> 8);
    mock_read_data[4] = (uint8_t)(raw_h & 0xFF);
    mock_read_data[5] = sht30_crc8(&mock_read_data[3], 2);
}

/* ---------- test helpers ---------- */
static sht30_t dev;
static sht30_cfg_t default_cfg = { .addr = SHT30_ADDR_A };

static void setup(void)
{
    mock_reset();
    sht30_init(&dev, &mock_i2c, &default_cfg);
    mock_op_count = 0;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    sht30_t d;
    sht30_err_t e = sht30_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, SHT30_OK);
    ASSERT_EQ(mock_last_write[0], 0x30);
    ASSERT_EQ(mock_last_write[1], 0xA2);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    sht30_t d;
    sht30_err_t e = sht30_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, SHT30_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(sht30_init(NULL, &mock_i2c, &default_cfg), SHT30_ERR_PARAM);
    sht30_t d;
    ASSERT_EQ(sht30_init(&d, NULL, &default_cfg), SHT30_ERR_PARAM);
    ASSERT_EQ(sht30_init(&d, &mock_i2c, NULL), SHT30_ERR_PARAM);
    sht30_data_t data;
    ASSERT_EQ(sht30_read(NULL, &data), SHT30_ERR_PARAM);
    ASSERT_EQ(sht30_read(&d, NULL), SHT30_ERR_PARAM);
    ASSERT_EQ(sht30_reset(NULL), SHT30_ERR_PARAM);
}

TEST(test_read_25c_50pct)
{
    setup();

    /* raw_t for 25°C: raw = (25+45)/175 * 65535 = 26214 */
    /* raw_h for 50%: raw = 50/100 * 65535 = 32768 */
    plant_reading(26214, 32768);

    sht30_data_t data;
    sht30_err_t e = sht30_read(&dev, &data);
    ASSERT_EQ(e, SHT30_OK);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.1f);
    ASSERT_NEAR(data.humidity_pct, 50.0f, 0.1f);
}

TEST(test_read_0c_0pct)
{
    setup();

    /* raw_t for 0°C: raw = 45/175 * 65535 = 16852 */
    /* raw_h for 0%: raw = 0 */
    plant_reading(16852, 0);

    sht30_data_t data;
    sht30_read(&dev, &data);
    ASSERT_NEAR(data.temperature_c, 0.0f, 0.2f);
    ASSERT_NEAR(data.humidity_pct, 0.0f, 0.1f);
}

TEST(test_read_minus40c)
{
    setup();

    plant_reading(0, 0);

    sht30_data_t data;
    sht30_read(&dev, &data);
    ASSERT_NEAR(data.temperature_c, -45.0f, 0.1f);
}

TEST(test_read_crc_fail_temp)
{
    setup();
    plant_reading(26214, 32768);
    mock_read_data[2] = 0x00;

    sht30_data_t data;
    sht30_err_t e = sht30_read(&dev, &data);
    ASSERT_EQ(e, SHT30_ERR_CRC);
}

TEST(test_read_crc_fail_humidity)
{
    setup();
    plant_reading(26214, 32768);
    mock_read_data[5] = 0x00;

    sht30_data_t data;
    sht30_err_t e = sht30_read(&dev, &data);
    ASSERT_EQ(e, SHT30_ERR_CRC);
}

TEST(test_read_i2c_fail_cmd)
{
    setup();
    mock_fail_next = 1;
    sht30_data_t data;
    sht30_err_t e = sht30_read(&dev, &data);
    ASSERT_EQ(e, SHT30_ERR_I2C);
}

TEST(test_read_i2c_fail_data)
{
    setup();
    mock_fail_at = 1;
    sht30_data_t data;
    sht30_err_t e = sht30_read(&dev, &data);
    ASSERT_EQ(e, SHT30_ERR_I2C);
}

TEST(test_reset)
{
    setup();
    sht30_err_t e = sht30_reset(&dev);
    ASSERT_EQ(e, SHT30_OK);
    ASSERT_EQ(mock_last_write[0], 0x30);
    ASSERT_EQ(mock_last_write[1], 0xA2);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    sht30_err_t e = sht30_reset(&dev);
    ASSERT_EQ(e, SHT30_ERR_I2C);
}

TEST(test_addr_b)
{
    mock_reset();
    sht30_cfg_t cfg = { .addr = SHT30_ADDR_B };
    sht30_t d;
    sht30_err_t e = sht30_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, SHT30_OK);
}

TEST(test_crc8_known_vector)
{
    uint8_t data[] = { 0xBE, 0xEF };
    uint8_t crc = sht30_crc8(data, 2);
    ASSERT_EQ(crc, 0x92);
}

TEST(test_read_sends_measure_cmd)
{
    setup();
    plant_reading(26214, 32768);

    sht30_data_t data;
    sht30_read(&dev, &data);
    ASSERT_EQ(mock_last_write[0], 0x24);
    ASSERT_EQ(mock_last_write[1], 0x00);
}

TEST_MAIN("sht30")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_25c_50pct);
    RUN(test_read_0c_0pct);
    RUN(test_read_minus40c);
    RUN(test_read_crc_fail_temp);
    RUN(test_read_crc_fail_humidity);
    RUN(test_read_i2c_fail_cmd);
    RUN(test_read_i2c_fail_data);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_addr_b);
    RUN(test_crc8_known_vector);
    RUN(test_read_sends_measure_cmd);
TEST_MAIN_END()

