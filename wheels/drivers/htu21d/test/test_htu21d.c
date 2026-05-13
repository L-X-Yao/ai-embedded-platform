// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_time.h"
#include <string.h>
#include <math.h>
#include "htu21d.h"

/* ---------- mock I2C (command-based, not register-based) ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

static uint8_t mock_read_buf[3];
static uint8_t mock_last_cmd;
static int     mock_cmd_count;
static int     mock_read_count;

static void mock_reset(void)
{
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;
    memset(mock_read_buf, 0, sizeof(mock_read_buf));
    mock_last_cmd   = 0;
    mock_cmd_count  = 0;
    mock_read_count = 0;
    mock_time_reset();
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write_cmd(void *ctx, uint8_t dev_addr, uint8_t cmd)
{
    (void)ctx; (void)dev_addr;
    if (mock_should_fail()) return false;
    mock_last_cmd = cmd;
    mock_cmd_count++;
    return true;
}

static bool mock_read_data(void *ctx, uint8_t dev_addr,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_should_fail()) return false;
    mock_read_count++;
    for (uint16_t i = 0; i < len && i < sizeof(mock_read_buf); i++)
    {
        buf[i] = mock_read_buf[i];
    }
    return true;
}

static const htu21d_i2c_t mock_i2c = {
    .write_cmd = mock_write_cmd,
    .read_data = mock_read_data,
    .delay_ms  = mock_delay_ms,
    .ctx       = NULL,
};

/* Plant a 3-byte reading with correct CRC */
static void plant_reading(uint16_t raw)
{
    mock_read_buf[0] = (uint8_t)(raw >> 8);
    mock_read_buf[1] = (uint8_t)(raw & 0xFF);
    mock_read_buf[2] = htu21d_crc8(mock_read_buf, 2);
}

/* ---------- test helpers ---------- */
static htu21d_t dev;

static void setup(void)
{
    mock_reset();
    htu21d_init(&dev, &mock_i2c);
    mock_op_count = 0;  /* reset counter after init's reset command */
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    htu21d_t d;
    htu21d_err_t e = htu21d_init(&d, &mock_i2c);
    ASSERT_EQ(e, HTU21D_OK);
    /* init sends soft reset command */
    ASSERT_EQ(mock_last_cmd, 0xFE);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    htu21d_t d;
    htu21d_err_t e = htu21d_init(&d, &mock_i2c);
    ASSERT_EQ(e, HTU21D_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(htu21d_init(NULL, &mock_i2c), HTU21D_ERR_PARAM);
    htu21d_t d;
    ASSERT_EQ(htu21d_init(&d, NULL), HTU21D_ERR_PARAM);

    float val;
    ASSERT_EQ(htu21d_read_temperature(NULL, &val), HTU21D_ERR_PARAM);
    ASSERT_EQ(htu21d_read_humidity(NULL, &val), HTU21D_ERR_PARAM);
    ASSERT_EQ(htu21d_reset(NULL), HTU21D_ERR_PARAM);

    /* NULL output pointer */
    setup();
    ASSERT_EQ(htu21d_read_temperature(&dev, NULL), HTU21D_ERR_PARAM);
    ASSERT_EQ(htu21d_read_humidity(&dev, NULL), HTU21D_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    htu21d_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    htu21d_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(htu21d_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)HTU21D_OK);
    ASSERT_EQ(-1, (int)HTU21D_ERR_I2C);
    ASSERT_EQ(-2, (int)HTU21D_ERR_PARAM);
    ASSERT_EQ(-3, (int)HTU21D_ERR_CRC);
}

/* ---------- temperature tests ---------- */

TEST(test_read_temp_25c)
{
    setup();
    /*
     * T = -46.85 + 175.72 * raw / 65536
     * 25 = -46.85 + 175.72 * raw / 65536
     * raw = (25 + 46.85) / 175.72 * 65536 = 26780 (approx)
     * Status bits cleared: 26780 & 0xFFFC = 26780
     */
    uint16_t raw = (uint16_t)((25.0f + 46.85f) / 175.72f * 65536.0f);
    raw &= 0xFFFC;
    plant_reading(raw);

    float temp;
    htu21d_err_t e = htu21d_read_temperature(&dev, &temp);
    ASSERT_EQ(e, HTU21D_OK);
    ASSERT_NEAR(temp, 25.0f, 0.1f);
    /* Verify correct command was sent */
    ASSERT_EQ(mock_last_cmd, 0xE3);
}

TEST(test_read_temp_0c)
{
    setup();
    uint16_t raw = (uint16_t)((0.0f + 46.85f) / 175.72f * 65536.0f);
    raw &= 0xFFFC;
    plant_reading(raw);

    float temp;
    htu21d_err_t e = htu21d_read_temperature(&dev, &temp);
    ASSERT_EQ(e, HTU21D_OK);
    ASSERT_NEAR(temp, 0.0f, 0.2f);
}

TEST(test_read_temp_i2c_fail_cmd)
{
    setup();
    mock_fail_next = 1;
    float temp;
    htu21d_err_t e = htu21d_read_temperature(&dev, &temp);
    ASSERT_EQ(e, HTU21D_ERR_I2C);
}

TEST(test_read_temp_i2c_fail_data)
{
    setup();
    mock_fail_at = 1;
    float temp;
    htu21d_err_t e = htu21d_read_temperature(&dev, &temp);
    ASSERT_EQ(e, HTU21D_ERR_I2C);
}

/* ---------- humidity tests ---------- */

TEST(test_read_hum_50pct)
{
    setup();
    /*
     * RH = -6 + 125 * raw / 65536
     * 50 = -6 + 125 * raw / 65536
     * raw = (50 + 6) / 125 * 65536 = 29360 (approx)
     */
    uint16_t raw = (uint16_t)((50.0f + 6.0f) / 125.0f * 65536.0f);
    raw &= 0xFFFC;
    plant_reading(raw);

    float rh;
    htu21d_err_t e = htu21d_read_humidity(&dev, &rh);
    ASSERT_EQ(e, HTU21D_OK);
    ASSERT_NEAR(rh, 50.0f, 0.2f);
    /* Verify correct command was sent */
    ASSERT_EQ(mock_last_cmd, 0xE5);
}

TEST(test_read_hum_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    float rh;
    htu21d_err_t e = htu21d_read_humidity(&dev, &rh);
    ASSERT_EQ(e, HTU21D_ERR_I2C);
}

/* ---------- CRC tests ---------- */

TEST(test_crc_check)
{
    setup();
    uint16_t raw = 26780u & 0xFFFC;
    plant_reading(raw);
    /* corrupt CRC */
    mock_read_buf[2] = (uint8_t)(mock_read_buf[2] ^ 0xFF);

    float temp;
    htu21d_err_t e = htu21d_read_temperature(&dev, &temp);
    ASSERT_EQ(e, HTU21D_ERR_CRC);
}

TEST(test_crc_known_vector)
{
    /* Verify CRC-8 with known input */
    uint8_t data[] = { 0x00, 0x00 };
    uint8_t crc = htu21d_crc8(data, 2);
    /* poly 0x131, init 0x00: CRC(0x00,0x00) = 0x00 */
    ASSERT_EQ(crc, 0x00);

    /* Another vector */
    uint8_t data2[] = { 0xFF, 0xFF };
    uint8_t crc2 = htu21d_crc8(data2, 2);
    /* Verify it is not trivially zero */
    ASSERT_TRUE(crc2 != 0x00);
}

/* ---------- reset tests ---------- */

TEST(test_reset)
{
    setup();
    htu21d_err_t e = htu21d_reset(&dev);
    ASSERT_EQ(e, HTU21D_OK);
    ASSERT_EQ(mock_last_cmd, 0xFE);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    htu21d_err_t e = htu21d_reset(&dev);
    ASSERT_EQ(e, HTU21D_ERR_I2C);
}

/* ---------- delay tests ---------- */

TEST(test_init_delays_after_reset)
{
    mock_reset();
    htu21d_t d;
    htu21d_init(&d, &mock_i2c);
    /* init should call delay_ms after soft reset (15 ms) */
    ASSERT_TRUE(mock_time_delay_total_ms > 0);
}

TEST(test_no_delay_hal_null)
{
    /* delay_ms = NULL should not crash */
    static const htu21d_i2c_t no_delay_i2c = {
        .write_cmd = mock_write_cmd,
        .read_data = mock_read_data,
        .delay_ms  = NULL,
        .ctx       = NULL,
    };
    mock_reset();
    htu21d_t d;
    htu21d_err_t e = htu21d_init(&d, &no_delay_i2c);
    ASSERT_EQ(e, HTU21D_OK);
}

TEST_MAIN("htu21d")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_read_temp_25c);
    RUN(test_read_temp_0c);
    RUN(test_read_temp_i2c_fail_cmd);
    RUN(test_read_temp_i2c_fail_data);
    RUN(test_read_hum_50pct);
    RUN(test_read_hum_i2c_fail);
    RUN(test_crc_check);
    RUN(test_crc_known_vector);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_init_delays_after_reset);
    RUN(test_no_delay_hal_null);
TEST_MAIN_END()
