// SPDX-License-Identifier: MIT
/*
 * i2c_scan — I2C bus scanner and device identifier
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "i2c_scan.h"

/* ── Known-address table ───────────────────────────────────────── */

typedef struct {
    uint8_t     addr;
    const char *name;
} i2c_scan_known_t;

static const i2c_scan_known_t known_devices[] = {
    { 0x1E, "HMC5883L/QMC5883" },
    { 0x20, "PCF8574" },
    { 0x23, "BH1750" },
    { 0x27, "PCF8574A/LCD" },
    { 0x3C, "SSD1306/SH1106" },
    { 0x3D, "SSD1306 alt" },
    { 0x40, "INA219/HDC1080" },
    { 0x44, "SHT30/SHT31" },
    { 0x48, "ADS1115/TMP102" },
    { 0x50, "AT24C32/EEPROM" },
    { 0x57, "MAX30102" },
    { 0x5A, "MLX90614" },
    { 0x5B, "CCS811" },
    { 0x68, "DS3231/MPU6050" },
    { 0x69, "MPU6050 alt/ICM20948" },
    { 0x76, "BMP280/BME680" },
    { 0x77, "BMP180/BME280 alt" },
};

#define KNOWN_COUNT  (sizeof(known_devices) / sizeof(known_devices[0]))

/* ── Internal helpers ──────────────────────────────────────────── */

static i2c_scan_err_t scan_range_impl(i2c_scan_probe_fn probe,
                                       void *probe_ctx,
                                       uint8_t start, uint8_t end,
                                       i2c_scan_result_t *result)
{
    result->count = 0;
    uint8_t total_found = 0;

    for (unsigned a = start; a <= end; a++) {
        if (probe(probe_ctx, (uint8_t)a)) {
            if (result->count < I2C_SCAN_MAX_DEVICES) {
                result->addresses[result->count] = (uint8_t)a;
                result->count++;
            }
            total_found++;
        }
    }
    (void)total_found;
    return I2C_SCAN_OK;
}

/* ── Public API ────────────────────────────────────────────────── */

i2c_scan_err_t i2c_scan(i2c_scan_probe_fn probe, void *probe_ctx,
                         i2c_scan_result_t *result)
{
    if (!probe || !result) return I2C_SCAN_ERR_PARAM;
    return scan_range_impl(probe, probe_ctx,
                           I2C_SCAN_ADDR_MIN, I2C_SCAN_ADDR_MAX, result);
}

i2c_scan_err_t i2c_scan_range(i2c_scan_probe_fn probe, void *probe_ctx,
                               uint8_t start, uint8_t end,
                               i2c_scan_result_t *result)
{
    if (!probe || !result) return I2C_SCAN_ERR_PARAM;
    if (start > end)       return I2C_SCAN_ERR_PARAM;
    return scan_range_impl(probe, probe_ctx, start, end, result);
}

i2c_scan_err_t i2c_scan_callback(i2c_scan_probe_fn probe, void *probe_ctx,
                                  i2c_scan_found_fn found, void *found_ctx)
{
    if (!probe || !found) return I2C_SCAN_ERR_PARAM;

    for (unsigned a = I2C_SCAN_ADDR_MIN; a <= I2C_SCAN_ADDR_MAX; a++) {
        if (probe(probe_ctx, (uint8_t)a)) {
            found(found_ctx, (uint8_t)a);
        }
    }
    return I2C_SCAN_OK;
}

const char *i2c_scan_identify(uint8_t addr)
{
    for (size_t i = 0; i < KNOWN_COUNT; i++) {
        if (known_devices[i].addr == addr) {
            return known_devices[i].name;
        }
    }
    return NULL;
}
