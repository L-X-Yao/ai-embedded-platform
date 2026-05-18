// SPDX-License-Identifier: MIT
/*
 * prop_sht40.c -- property-based tests for sht40
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_sht40.c ../sht40.c -o prop_sht40
 */

#include "test_harness.h"
#include "property_test.h"
#include <string.h>
#include "../sht40.h"

/* --- mock I2C (raw write/read, not register-based) --- */

static int     g_fail_next;
static int     g_op_count;
static int     g_fail_at;
static uint8_t g_read_data[6];

static void mock_reset(void)
{
    g_fail_next = 0;
    g_op_count  = 0;
    g_fail_at   = -1;
    memset(g_read_data, 0, sizeof(g_read_data));
}

static bool mock_should_fail(void)
{
    int n = g_op_count++;
    if (g_fail_next) { g_fail_next = 0; return true; }
    if (g_fail_at >= 0 && n == g_fail_at) return true;
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
    for (uint16_t i = 0; i < len && i < (uint16_t)sizeof(g_read_data); i++)
        data[i] = g_read_data[i];
    return true;
}

static void mock_delay(void *ctx, uint16_t ms)
{
    (void)ctx; (void)ms;
}

static const sht40_i2c_t mock_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

static void plant_serial(uint16_t w0, uint16_t w1)
{
    g_read_data[0] = (uint8_t)(w0 >> 8);
    g_read_data[1] = (uint8_t)(w0 & 0xFF);
    g_read_data[2] = sht40_crc8(&g_read_data[0], 2);
    g_read_data[3] = (uint8_t)(w1 >> 8);
    g_read_data[4] = (uint8_t)(w1 & 0xFF);
    g_read_data[5] = sht40_crc8(&g_read_data[3], 2);
}

static void plant_reading(uint16_t raw_t, uint16_t raw_h)
{
    g_read_data[0] = (uint8_t)(raw_t >> 8);
    g_read_data[1] = (uint8_t)(raw_t & 0xFF);
    g_read_data[2] = sht40_crc8(&g_read_data[0], 2);
    g_read_data[3] = (uint8_t)(raw_h >> 8);
    g_read_data[4] = (uint8_t)(raw_h & 0xFF);
    g_read_data[5] = sht40_crc8(&g_read_data[3], 2);
}

static sht40_t g_dev;

static void setup_dev(void)
{
    mock_reset();
    plant_serial(0x1234, 0x5678);
    sht40_init(&g_dev, &mock_i2c, SHT40_ADDR_A);
    g_op_count = 0;
}

/* --- crc8_deterministic: same input always produces same CRC --- */
PROPERTY(crc8_deterministic, 10000) {
    uint8_t data[2];
    data[0] = ARBI_UINT8(seed);
    data[1] = ARBI_UINT8(seed);
    uint8_t c1 = sht40_crc8(data, 2);
    uint8_t c2 = sht40_crc8(data, 2);
    PROP_ASSERT_EQ(c1, c2);
}

/* --- temp_in_physical_range: any raw value yields temp in [-45, 130] C --- */
PROPERTY(temp_in_range, 10000) {
    setup_dev();
    uint16_t raw_t = ARBI_UINT16(seed);
    uint16_t raw_h = ARBI_UINT16(seed);
    plant_reading(raw_t, raw_h);

    sht40_data_t data;
    sht40_err_t err = sht40_read_high(&g_dev, &data);
    PROP_ASSERT(err == SHT40_OK);
    /* T = -45 + 175 * raw / 65535, range: [-45, 130] */
    PROP_ASSERT(data.temperature_c >= -45.1f);
    PROP_ASSERT(data.temperature_c <= 130.1f);
}

/* --- humidity_clamped: humidity always in [0, 100] --- */
PROPERTY(humidity_clamped, 10000) {
    setup_dev();
    uint16_t raw_t = ARBI_UINT16(seed);
    uint16_t raw_h = ARBI_UINT16(seed);
    plant_reading(raw_t, raw_h);

    sht40_data_t data;
    sht40_err_t err = sht40_read_high(&g_dev, &data);
    PROP_ASSERT(err == SHT40_OK);
    /* Humidity should be clamped to [0, 100] */
    PROP_ASSERT(data.humidity_pct >= 0.0f);
    PROP_ASSERT(data.humidity_pct <= 100.0f);
}

/* --- double_init_safe: repeated init always safe --- */
PROPERTY(double_init_safe, 5000) {
    mock_reset();
    plant_serial(0xAAAA, 0xBBBB);
    sht40_t dev;
    sht40_err_t e1 = sht40_init(&dev, &mock_i2c, SHT40_ADDR_A);
    plant_serial(0xAAAA, 0xBBBB);
    sht40_err_t e2 = sht40_init(&dev, &mock_i2c, SHT40_ADDR_A);
    PROP_ASSERT_EQ(e1, e2);
}

/* --- all_precisions_consistent: all precision modes produce same-range values --- */
PROPERTY(all_precisions_consistent, 5000) {
    setup_dev();
    uint16_t raw_t = ARBI_UINT16(seed);
    uint16_t raw_h = ARBI_UINT16(seed);

    sht40_data_t dh, dm, dl;

    plant_reading(raw_t, raw_h);
    sht40_read_high(&g_dev, &dh);

    g_op_count = 0;
    plant_reading(raw_t, raw_h);
    sht40_read_medium(&g_dev, &dm);

    g_op_count = 0;
    plant_reading(raw_t, raw_h);
    sht40_read_low(&g_dev, &dl);

    /* All precisions should compute same result from same raw data */
    PROP_ASSERT(dh.temperature_c == dm.temperature_c);
    PROP_ASSERT(dh.temperature_c == dl.temperature_c);
    PROP_ASSERT(dh.humidity_pct == dm.humidity_pct);
    PROP_ASSERT(dh.humidity_pct == dl.humidity_pct);
}

TEST_MAIN("sht40 properties") {
    RUN(crc8_deterministic);
    RUN(temp_in_range);
    RUN(humidity_clamped);
    RUN(double_init_safe);
    RUN(all_precisions_consistent);
}
TEST_MAIN_END()
