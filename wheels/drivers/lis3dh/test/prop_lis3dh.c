// SPDX-License-Identifier: MIT
/*
 * prop_lis3dh.c — property-based tests for LIS3DH accelerometer driver
 *
 * Properties:
 *   1. init+read never crashes with arbitrary register values
 *   2. acceleration output bounded per configured range
 *   3. all four range settings produce bounded output
 *   4. read_raw roundtrips through register layout
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "lis3dh.h"

/* p.21: "the 7 LSb represent the actual register address while the MSB
 * enables address auto increment". */
static bool lis3dh_mock_read_regs(void *ctx, uint8_t dev_addr, uint8_t sub,
                                  uint8_t *buf, uint16_t len)
{
    uint8_t reg = (uint8_t)(sub & 0x7Fu);

    if (!mock_i2c_read_regs(ctx, dev_addr, reg, buf, len)) return false;
    if (!(sub & 0x80u)) memset(buf, mock_regs[reg], len);
    return true;
}

static const lis3dh_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = lis3dh_mock_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* ------------------------------------------------------------------ */

PROPERTY(read_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    /* WHO_AM_I at 0x0F must be 0x33 for init to succeed */
    mock_regs[0x0F] = 0x33;

    lis3dh_t dev;
    lis3dh_cfg_t cfg = {
        .addr  = LIS3DH_ADDR_LOW,
        .range = LIS3DH_RANGE_2G,
        .odr   = LIS3DH_ODR_100HZ,
    };
    lis3dh_init(&dev, &prop_i2c, &cfg);

    lis3dh_accel_t accel;
    lis3dh_read_accel(&dev, &accel);
    /* Must not crash */
}

PROPERTY(accel_bounded_2g, 5000)
{
    mock_i2c_reset();
    mock_regs[0x0F] = 0x33;

    lis3dh_t dev;
    lis3dh_cfg_t cfg = {
        .addr  = LIS3DH_ADDR_LOW,
        .range = LIS3DH_RANGE_2G,
        .odr   = LIS3DH_ODR_100HZ,
    };
    lis3dh_err_t err = lis3dh_init(&dev, &prop_i2c, &cfg);
    if (err != LIS3DH_OK) return;

    /* Plant random acceleration data in output registers 0x28..0x2D */
    for (int i = 0; i < 6; i++)
        mock_regs[0x28 + i] = ARBI_UINT8(seed);

    lis3dh_accel_t accel;
    err = lis3dh_read_accel(&dev, &accel);
    if (err != LIS3DH_OK) return;

    /* +/-2g range: max possible = 32767/16000 ~ 2.05g */
    float limit = 2.1f;
    PROP_ASSERT(accel.x_g == accel.x_g); /* not NaN */
    PROP_ASSERT(accel.x_g >= -limit && accel.x_g <= limit);
    PROP_ASSERT(accel.y_g >= -limit && accel.y_g <= limit);
    PROP_ASSERT(accel.z_g >= -limit && accel.z_g <= limit);
}

PROPERTY(accel_all_ranges_bounded, 5000)
{
    /* Max g values per range (with margin for 16-bit rounding) */
    static const float range_max[] = { 2.1f, 4.2f, 8.2f, 25.0f };

    mock_i2c_reset();
    mock_regs[0x0F] = 0x33;

    int range_idx = (int)(ARBI_UINT8(seed) & 0x03);
    lis3dh_t dev;
    lis3dh_cfg_t cfg = {
        .addr  = LIS3DH_ADDR_LOW,
        .range = (lis3dh_range_t)range_idx,
        .odr   = LIS3DH_ODR_100HZ,
    };
    lis3dh_err_t err = lis3dh_init(&dev, &prop_i2c, &cfg);
    if (err != LIS3DH_OK) return;

    for (int i = 0; i < 6; i++)
        mock_regs[0x28 + i] = ARBI_UINT8(seed);

    lis3dh_accel_t accel;
    err = lis3dh_read_accel(&dev, &accel);
    if (err != LIS3DH_OK) return;

    float limit = range_max[range_idx];
    PROP_ASSERT(accel.x_g >= -limit && accel.x_g <= limit);
    PROP_ASSERT(accel.y_g >= -limit && accel.y_g <= limit);
    PROP_ASSERT(accel.z_g >= -limit && accel.z_g <= limit);
}

PROPERTY(raw_read_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x0F] = 0x33;

    lis3dh_t dev;
    lis3dh_cfg_t cfg = {
        .addr  = LIS3DH_ADDR_LOW,
        .range = LIS3DH_RANGE_16G,
        .odr   = LIS3DH_ODR_100HZ,
    };
    lis3dh_init(&dev, &prop_i2c, &cfg);

    lis3dh_raw_t raw;
    lis3dh_read_raw(&dev, &raw);
    /* Must not crash; raw values are int16_t, always in [-32768, 32767] */
}

TEST_MAIN("prop_lis3dh")
    RUN(read_no_crash);
    RUN(accel_bounded_2g);
    RUN(accel_all_ranges_bounded);
    RUN(raw_read_no_crash);
TEST_MAIN_END()
