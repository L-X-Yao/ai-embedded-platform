// SPDX-License-Identifier: MIT
/*
 * prop_opt3001.c — property-based tests for OPT3001 ambient light sensor
 *
 * OPT3001 uses write_regs/read_regs I2C (16-bit big-endian registers).
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "opt3001.h"

#define REG_RESULT          0x00
#define REG_MANUFACTURER_ID 0x7E
#define REG_DEVICE_ID       0x7F

/* mock_i2c.h stores bytes and OPT3001 registers are 16 bits, so neighbours
 * overlap: the low byte of 0x7E is the high byte of 0x7F. Both IDs cannot live
 * in mock_regs, so they are answered here. */
static bool prop_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    if (len == 2 && reg == REG_MANUFACTURER_ID) {
        buf[0] = 0x54;
        buf[1] = 0x49;
        return true;
    }
    if (len == 2 && reg == REG_DEVICE_ID) {
        buf[0] = 0x30;
        buf[1] = 0x01;
        return true;
    }
    return mock_i2c_read_regs(ctx, dev_addr, reg, buf, len);
}

static const opt3001_i2c_t prop_i2c = {
    .write_regs = mock_i2c_write_regs,
    .read_regs  = prop_read_regs,
    .delay_ms   = mock_i2c_delay_ms,
    .ctx        = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    opt3001_t dev;
    opt3001_init(&dev, &prop_i2c, OPT3001_ADDR_GND_GND);
}

PROPERTY(read_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    opt3001_t dev;
    PROP_ASSERT_EQ(opt3001_init(&dev, &prop_i2c, OPT3001_ADDR_GND_GND), OPT3001_OK);

    /* init wrote the config over the low byte of the result: make it random again */
    mock_regs[REG_RESULT + 1] = ARBI_UINT8(seed);

    float lux = -1.0f;
    PROP_ASSERT_EQ(opt3001_read_lux(&dev, &lux), OPT3001_OK);
    PROP_ASSERT(lux >= 0.0f);
}

TEST_MAIN("prop_opt3001")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
