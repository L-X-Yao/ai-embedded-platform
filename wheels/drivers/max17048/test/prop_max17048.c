/*
 * prop_max17048.c -- property-based tests for max17048
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_max17048.c ../max17048.c -o prop_max17048
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "../max17048.h"

#define REG_VCELL   0x02
#define REG_SOC     0x04
#define REG_MODE    0x06
#define REG_VERSION 0x08
#define REG_CONFIG  0x0C
#define REG_CRATE   0x16

static const max17048_i2c_t mock_bus = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static void mock_reset_chip(void)
{
    mock_i2c_reset();
    mock_regs[REG_VERSION]     = 0x00;
    mock_regs[REG_VERSION + 1] = 0x11;
    mock_regs[REG_CONFIG]      = 0x97;
    mock_regs[REG_CONFIG + 1]  = 0x1C;
}

/* --- init_no_crash: random register values, init never crashes --- */
PROPERTY(init_no_crash, 10000) {
    mock_i2c_reset();
    /* Fill registers with random data */
    ARBI_FILL(seed, mock_regs, 256);
    max17048_t dev;
    /* init may fail (wrong version, etc.) but must not crash */
    (void)max17048_init(&dev, &mock_bus);
    PROP_ASSERT(1);
}

/* --- read_voltage_in_range: any register data, voltage is non-negative --- */
PROPERTY(read_voltage_in_range, 10000) {
    mock_reset_chip();
    max17048_t dev;
    max17048_init(&dev, &mock_bus);

    /* Set random VCELL register (16-bit big-endian) */
    uint8_t hi = ARBI_UINT8(seed);
    uint8_t lo = ARBI_UINT8(seed);
    mock_regs[REG_VCELL]     = hi;
    mock_regs[REG_VCELL + 1] = lo;

    float voltage;
    max17048_err_t err = max17048_read_voltage(&dev, &voltage);
    PROP_ASSERT(err == MAX17048_OK);
    /* Voltage formula: raw * 78.125e-6. Max raw=0xFFFF -> ~5.12V */
    PROP_ASSERT(voltage >= 0.0f);
    PROP_ASSERT(voltage <= 5.2f);
}

/* --- read_soc_in_range: any register data, SOC in [0, 256) --- */
PROPERTY(read_soc_in_range, 10000) {
    mock_reset_chip();
    max17048_t dev;
    max17048_init(&dev, &mock_bus);

    uint8_t hi = ARBI_UINT8(seed);
    uint8_t lo = ARBI_UINT8(seed);
    mock_regs[REG_SOC]     = hi;
    mock_regs[REG_SOC + 1] = lo;

    float soc;
    max17048_err_t err = max17048_read_soc(&dev, &soc);
    PROP_ASSERT(err == MAX17048_OK);
    /* SOC = high byte + low byte / 256. Max = 255 + 255/256 ~= 256 */
    PROP_ASSERT(soc >= 0.0f);
    PROP_ASSERT(soc <= 256.0f);
}

/* --- double_init_safe: calling init twice does not corrupt state --- */
PROPERTY(double_init_safe, 5000) {
    mock_reset_chip();
    /* Randomize some regs */
    mock_regs[REG_VCELL]     = ARBI_UINT8(seed);
    mock_regs[REG_VCELL + 1] = ARBI_UINT8(seed);

    max17048_t dev;
    max17048_err_t e1 = max17048_init(&dev, &mock_bus);
    max17048_err_t e2 = max17048_init(&dev, &mock_bus);
    /* Both calls should return the same result */
    PROP_ASSERT_EQ(e1, e2);

    if (e1 == MAX17048_OK) {
        float v1, v2;
        max17048_read_voltage(&dev, &v1);
        max17048_read_voltage(&dev, &v2);
        PROP_ASSERT(v1 == v2);
    }
}

/* --- alert_threshold_roundtrip: valid thresholds accepted --- */
PROPERTY(alert_threshold_valid, 10000) {
    mock_reset_chip();
    max17048_t dev;
    max17048_init(&dev, &mock_bus);

    /* Generate threshold in valid range [1, 32] */
    uint8_t thr = (uint8_t)(ARBI_RANGE(seed, 1, 32));
    max17048_err_t err = max17048_set_alert_threshold(&dev, thr);
    PROP_ASSERT(err == MAX17048_OK);
}

TEST_MAIN("max17048 properties") {
    RUN(init_no_crash);
    RUN(read_voltage_in_range);
    RUN(read_soc_in_range);
    RUN(double_init_safe);
    RUN(alert_threshold_valid);
}
TEST_MAIN_END()
