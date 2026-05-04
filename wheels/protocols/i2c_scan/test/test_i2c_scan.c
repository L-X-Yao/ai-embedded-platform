// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "i2c_scan.h"
#include <string.h>

/* ── Mock infrastructure ───────────────────────────────────────── */

static uint8_t mock_present[128];  /* 1 = device present at addr */
static int     mock_probe_calls;

static void mock_reset(void)
{
    memset(mock_present, 0, sizeof(mock_present));
    mock_probe_calls = 0;
}

static bool mock_probe(void *ctx, uint8_t addr)
{
    (void)ctx;
    mock_probe_calls++;
    if (addr >= 128) return false;
    return mock_present[addr] != 0;
}

/* Callback mock */
static uint8_t cb_found_addrs[I2C_SCAN_MAX_DEVICES];
static uint8_t cb_found_count;

static void mock_found(void *ctx, uint8_t addr)
{
    (void)ctx;
    if (cb_found_count < I2C_SCAN_MAX_DEVICES) {
        cb_found_addrs[cb_found_count] = addr;
    }
    cb_found_count++;
}

/* ── i2c_scan() tests ─────────────────────────────────────────── */

TEST(test_scan_null_probe)
{
    i2c_scan_result_t r;
    ASSERT_EQ(i2c_scan(NULL, NULL, &r), I2C_SCAN_ERR_PARAM);
}

TEST(test_scan_null_result)
{
    ASSERT_EQ(i2c_scan(mock_probe, NULL, NULL), I2C_SCAN_ERR_PARAM);
}

TEST(test_scan_empty_bus)
{
    mock_reset();
    i2c_scan_result_t r;
    ASSERT_EQ(i2c_scan(mock_probe, NULL, &r), I2C_SCAN_OK);
    ASSERT_EQ(r.count, 0);
    /* Should probe 0x08..0x77 = 112 addresses */
    ASSERT_EQ(mock_probe_calls, 112);
}

TEST(test_scan_finds_devices)
{
    mock_reset();
    mock_present[0x3C] = 1;  /* SSD1306 */
    mock_present[0x68] = 1;  /* MPU6050 */

    i2c_scan_result_t r;
    ASSERT_EQ(i2c_scan(mock_probe, NULL, &r), I2C_SCAN_OK);
    ASSERT_EQ(r.count, 2);
    ASSERT_EQ(r.addresses[0], 0x3C);
    ASSERT_EQ(r.addresses[1], 0x68);
}

TEST(test_scan_skips_reserved)
{
    mock_reset();
    /* Place devices at reserved addresses (0x00-0x07, 0x78-0x7F) */
    mock_present[0x00] = 1;
    mock_present[0x07] = 1;
    mock_present[0x78] = 1;
    mock_present[0x7F] = 1;

    i2c_scan_result_t r;
    ASSERT_EQ(i2c_scan(mock_probe, NULL, &r), I2C_SCAN_OK);
    ASSERT_EQ(r.count, 0);
}

/* ── i2c_scan_range() tests ────────────────────────────────────── */

TEST(test_range_null)
{
    ASSERT_EQ(i2c_scan_range(NULL, NULL, 0x10, 0x20, NULL),
              I2C_SCAN_ERR_PARAM);
}

TEST(test_range_inverted)
{
    i2c_scan_result_t r;
    ASSERT_EQ(i2c_scan_range(mock_probe, NULL, 0x50, 0x10, &r),
              I2C_SCAN_ERR_PARAM);
}

TEST(test_range_subset)
{
    mock_reset();
    mock_present[0x48] = 1;  /* ADS1115, inside range */
    mock_present[0x68] = 1;  /* MPU6050, outside range */

    i2c_scan_result_t r;
    ASSERT_EQ(i2c_scan_range(mock_probe, NULL, 0x40, 0x50, &r),
              I2C_SCAN_OK);
    ASSERT_EQ(r.count, 1);
    ASSERT_EQ(r.addresses[0], 0x48);
}

TEST(test_range_single_addr)
{
    mock_reset();
    mock_present[0x3C] = 1;

    i2c_scan_result_t r;
    ASSERT_EQ(i2c_scan_range(mock_probe, NULL, 0x3C, 0x3C, &r),
              I2C_SCAN_OK);
    ASSERT_EQ(r.count, 1);
    ASSERT_EQ(r.addresses[0], 0x3C);
}

/* ── Overflow: count tracks total even when array full ─────────── */

TEST(test_scan_overflow_count)
{
    mock_reset();
    /* Place more devices than I2C_SCAN_MAX_DEVICES (16) */
    for (uint8_t a = 0x10; a < 0x10 + I2C_SCAN_MAX_DEVICES + 2; a++) {
        mock_present[a] = 1;
    }

    i2c_scan_result_t r;
    ASSERT_EQ(i2c_scan(mock_probe, NULL, &r), I2C_SCAN_OK);
    /* count is capped at I2C_SCAN_MAX_DEVICES (array capacity) */
    ASSERT_EQ(r.count, I2C_SCAN_MAX_DEVICES);
    /* Array holds first I2C_SCAN_MAX_DEVICES entries */
    ASSERT_EQ(r.addresses[0], 0x10);
    ASSERT_EQ(r.addresses[I2C_SCAN_MAX_DEVICES - 1],
              (uint8_t)(0x10 + I2C_SCAN_MAX_DEVICES - 1));
}

/* ── i2c_scan_callback() tests ─────────────────────────────────── */

TEST(test_callback_null)
{
    ASSERT_EQ(i2c_scan_callback(NULL, NULL, mock_found, NULL),
              I2C_SCAN_ERR_PARAM);
    ASSERT_EQ(i2c_scan_callback(mock_probe, NULL, NULL, NULL),
              I2C_SCAN_ERR_PARAM);
}

TEST(test_callback_finds)
{
    mock_reset();
    cb_found_count = 0;
    mock_present[0x23] = 1;  /* BH1750 */
    mock_present[0x76] = 1;  /* BMP280 */

    ASSERT_EQ(i2c_scan_callback(mock_probe, NULL, mock_found, NULL),
              I2C_SCAN_OK);
    ASSERT_EQ(cb_found_count, 2);
    ASSERT_EQ(cb_found_addrs[0], 0x23);
    ASSERT_EQ(cb_found_addrs[1], 0x76);
}

/* ── i2c_scan_identify() tests ─────────────────────────────────── */

TEST(test_identify_known)
{
    const char *name = i2c_scan_identify(0x76);
    ASSERT_TRUE(name != NULL);
    ASSERT_EQ(strcmp(name, "BMP280/BME680"), 0);
}

TEST(test_identify_unknown)
{
    ASSERT_TRUE(i2c_scan_identify(0x0A) == NULL);
}

TEST(test_identify_multiple)
{
    ASSERT_EQ(strcmp(i2c_scan_identify(0x3C), "SSD1306/SH1106"), 0);
    ASSERT_EQ(strcmp(i2c_scan_identify(0x68), "DS3231/MPU6050"), 0);
    ASSERT_EQ(strcmp(i2c_scan_identify(0x48), "ADS1115/TMP102"), 0);
}

/* ── Test runner ───────────────────────────────────────────────── */

TEST_MAIN("i2c_scan")
    RUN(test_scan_null_probe);
    RUN(test_scan_null_result);
    RUN(test_scan_empty_bus);
    RUN(test_scan_finds_devices);
    RUN(test_scan_skips_reserved);
    RUN(test_range_null);
    RUN(test_range_inverted);
    RUN(test_range_subset);
    RUN(test_range_single_addr);
    RUN(test_scan_overflow_count);
    RUN(test_callback_null);
    RUN(test_callback_finds);
    RUN(test_identify_known);
    RUN(test_identify_unknown);
    RUN(test_identify_multiple);
TEST_MAIN_END()
