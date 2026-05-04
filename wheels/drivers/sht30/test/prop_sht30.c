// SPDX-License-Identifier: MIT
/*
 * prop_sht30.c — property-based tests for SHT30 driver
 *
 * SHT30 uses raw I2C write/read (not register-based), so we provide
 * a custom mock rather than mock_i2c.h.
 *
 * Properties:
 *   1. init+read never crashes with arbitrary response bytes
 *   2. temperature output in [-45, 130] C when CRC is valid
 *   3. humidity output in [0, 100]% when CRC is valid
 *   4. CRC mismatch always detected (never silent corruption)
 */

#include "test_harness.h"
#include "property_test.h"
#include "sht30.h"
#include <string.h>

/* ---------- custom mock for SHT30 raw I2C ---------- */

static uint8_t mock_read_data[6];
static int     mock_fail_flag;

static bool mock_write(void *ctx, uint8_t addr, const uint8_t *data,
                       uint16_t len)
{
    (void)ctx; (void)addr; (void)data; (void)len;
    if (mock_fail_flag) return false;
    return true;
}

static bool mock_read(void *ctx, uint8_t addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_fail_flag) return false;
    for (uint16_t i = 0; i < len && i < sizeof(mock_read_data); i++)
        data[i] = mock_read_data[i];
    return true;
}

static void mock_delay(void *ctx, uint16_t ms)
{
    (void)ctx; (void)ms;
}

static const sht30_i2c_t prop_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

static void mock_reset(void)
{
    memset(mock_read_data, 0, sizeof(mock_read_data));
    mock_fail_flag = 0;
}

/* Plant raw values with correct CRC */
static void plant_valid(uint16_t raw_t, uint16_t raw_h)
{
    mock_read_data[0] = (uint8_t)(raw_t >> 8);
    mock_read_data[1] = (uint8_t)(raw_t & 0xFF);
    mock_read_data[2] = sht30_crc8(&mock_read_data[0], 2);
    mock_read_data[3] = (uint8_t)(raw_h >> 8);
    mock_read_data[4] = (uint8_t)(raw_h & 0xFF);
    mock_read_data[5] = sht30_crc8(&mock_read_data[3], 2);
}

/* ------------------------------------------------------------------ */

PROPERTY(read_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();

    /* Fill response with arbitrary bytes (CRC likely wrong) */
    for (int i = 0; i < 6; i++)
        mock_read_data[i] = ARBI_UINT8(seed);

    sht30_t dev;
    sht30_cfg_t cfg = { .addr = SHT30_ADDR_A };
    sht30_init(&dev, &prop_i2c, &cfg);

    sht30_data_t data;
    sht30_read(&dev, &data);
    /* Must not crash — may return CRC error */
}

PROPERTY(temp_in_range, 5000)
{
    mock_reset();

    uint16_t raw_t = ARBI_UINT16(seed);
    uint16_t raw_h = ARBI_UINT16(seed);
    plant_valid(raw_t, raw_h);

    sht30_t dev;
    sht30_cfg_t cfg = { .addr = SHT30_ADDR_A };
    sht30_init(&dev, &prop_i2c, &cfg);

    sht30_data_t data;
    sht30_err_t err = sht30_read(&dev, &data);
    if (err != SHT30_OK) return;

    /* SHT30 formula: T = -45 + 175 * raw_t / 65535
     * Range: raw=0 -> -45, raw=65535 -> 130 */
    PROP_ASSERT(data.temperature_c == data.temperature_c); /* not NaN */
    PROP_ASSERT(data.temperature_c >= -45.5f && data.temperature_c <= 130.5f);
}

PROPERTY(humidity_in_range, 5000)
{
    mock_reset();

    uint16_t raw_t = ARBI_UINT16(seed);
    uint16_t raw_h = ARBI_UINT16(seed);
    plant_valid(raw_t, raw_h);

    sht30_t dev;
    sht30_cfg_t cfg = { .addr = SHT30_ADDR_A };
    sht30_init(&dev, &prop_i2c, &cfg);

    sht30_data_t data;
    sht30_err_t err = sht30_read(&dev, &data);
    if (err != SHT30_OK) return;

    /* SHT30 formula: H = 100 * raw_h / 65535
     * Range: [0, 100] */
    PROP_ASSERT(data.humidity_pct == data.humidity_pct); /* not NaN */
    PROP_ASSERT(data.humidity_pct >= -0.5f && data.humidity_pct <= 100.5f);
}

PROPERTY(crc_mismatch_detected, 5000)
{
    mock_reset();

    uint16_t raw_t = ARBI_UINT16(seed);
    uint16_t raw_h = ARBI_UINT16(seed);
    plant_valid(raw_t, raw_h);

    /* Corrupt one CRC byte */
    int which = ARBI_UINT8(seed) & 1;
    int crc_idx = which ? 5 : 2;
    mock_read_data[crc_idx] ^= (uint8_t)(1 + (ARBI_UINT8(seed) % 255));

    sht30_t dev;
    sht30_cfg_t cfg = { .addr = SHT30_ADDR_A };
    sht30_init(&dev, &prop_i2c, &cfg);

    sht30_data_t data;
    sht30_err_t err = sht30_read(&dev, &data);
    PROP_ASSERT(err == SHT30_ERR_CRC);
}

TEST_MAIN("prop_sht30")
    RUN(read_no_crash);
    RUN(temp_in_range);
    RUN(humidity_in_range);
    RUN(crc_mismatch_detected);
TEST_MAIN_END()
