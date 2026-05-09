// SPDX-License-Identifier: MIT
/*
 * prop_aht20.c — property-based tests for aht20
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_aht20.c ../aht20.c -o prop_aht20
 */

#include "test_harness.h"
#include "property_test.h"
#include "aht20.h"
#include <string.h>

/* ---------- mock I2C (aht20 uses raw write/read, not register-based) ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;
static uint8_t mock_read_buf[16];

static void mock_reset(void)
{
    mock_fail_next = 0;
    mock_op_count  = 0;
    mock_fail_at   = -1;
    memset(mock_read_buf, 0, sizeof(mock_read_buf));
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
    (void)ctx; (void)addr; (void)data; (void)len;
    if (mock_should_fail()) return false;
    return true;
}

static bool mock_read(void *ctx, uint8_t addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    for (uint16_t i = 0; i < len && i < sizeof(mock_read_buf); i++)
        data[i] = mock_read_buf[i];
    return true;
}

static void mock_delay(void *ctx, uint16_t ms)
{
    (void)ctx; (void)ms;
}

static const aht20_i2c_t prop_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

/* Plant a measurement with CRC */
static void plant_measurement(uint32_t raw_h, uint32_t raw_t, uint8_t status)
{
    mock_read_buf[0] = status;
    mock_read_buf[1] = (uint8_t)((raw_h >> 12) & 0xFF);
    mock_read_buf[2] = (uint8_t)((raw_h >> 4) & 0xFF);
    mock_read_buf[3] = (uint8_t)(((raw_h & 0x0F) << 4) | ((raw_t >> 16) & 0x0F));
    mock_read_buf[4] = (uint8_t)((raw_t >> 8) & 0xFF);
    mock_read_buf[5] = (uint8_t)(raw_t & 0xFF);
    mock_read_buf[6] = aht20_crc8(mock_read_buf, 6);
}

/* ------------------------------------------------------------------ */

PROPERTY(init_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();
    /* Set calibrated status so init proceeds */
    mock_read_buf[0] = (uint8_t)(ARBI_UINT8(seed) | 0x08);

    aht20_t dev;
    aht20_init(&dev, &prop_i2c);
    /* Must not crash */
}

PROPERTY(read_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();
    /* Calibrated status for init */
    mock_read_buf[0] = 0x08;

    aht20_t dev;
    aht20_init(&dev, &prop_i2c);

    /* Fill read buffer with random data (including potentially bad CRC) */
    for (int i = 0; i < 7; i++)
        mock_read_buf[i] = ARBI_UINT8(seed);

    aht20_data_t data;
    aht20_read(&dev, &data);
    /* Must not crash regardless of data content */
}

PROPERTY(read_in_range, 5000)
{
    mock_reset();
    mock_read_buf[0] = 0x08;

    aht20_t dev;
    aht20_err_t err = aht20_init(&dev, &prop_i2c);
    if (err != AHT20_OK) return;

    /* Generate random but valid 20-bit raw values */
    uint32_t raw_h = ARBI_UINT32(seed) & 0xFFFFF;
    uint32_t raw_t = ARBI_UINT32(seed) & 0xFFFFF;
    /* Status: calibrated, not busy */
    plant_measurement(raw_h, raw_t, 0x08);

    mock_op_count = 0;
    aht20_data_t data;
    err = aht20_read(&dev, &data);
    if (err != AHT20_OK) return;

    /* Temperature: -50 to +150 C based on formula */
    PROP_ASSERT(data.temperature_c >= -50.0f);
    PROP_ASSERT(data.temperature_c <= 150.0f);
    /* Humidity: 0 to 100% */
    PROP_ASSERT(data.humidity_pct >= 0.0f);
    PROP_ASSERT(data.humidity_pct <= 100.1f);
}

PROPERTY(double_init_safe, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();
    mock_read_buf[0] = 0x08;

    aht20_t dev;
    aht20_init(&dev, &prop_i2c);

    /* Second init on same dev */
    mock_reset();
    mock_read_buf[0] = 0x08;
    aht20_init(&dev, &prop_i2c);
    /* Must not crash */
}

PROPERTY(crc_deterministic, 5000)
{
    uint8_t buf[8];
    size_t len = ARBI_SIZE(seed, 7);
    ARBI_FILL(seed, buf, len);

    uint8_t crc1 = aht20_crc8(buf, (uint16_t)len);
    uint8_t crc2 = aht20_crc8(buf, (uint16_t)len);
    PROP_ASSERT_EQ(crc1, crc2);
}

TEST_MAIN("aht20 properties")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(read_in_range);
    RUN(double_init_safe);
    RUN(crc_deterministic);
TEST_MAIN_END()
