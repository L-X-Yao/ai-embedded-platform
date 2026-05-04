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

static const opt3001_i2c_t prop_i2c = {
    .write_regs = mock_i2c_write_regs,
    .read_regs  = mock_i2c_read_regs,
    .delay_ms   = mock_i2c_delay_ms,
    .ctx        = NULL,
};

/* Plant manufacturer and device IDs at registers 0x7E and 0x7F (big-endian) */
static void plant_ids(void)
{
    /* Manufacturer ID = 0x5449, big-endian at reg 0x7E*2=0xFC..0xFD */
    mock_regs[0x7E] = 0x54; mock_regs[0x7F] = 0x49;
    /* Device ID = 0x3001, big-endian at reg 0x7F*2... */
    /* The mock reads sequentially from reg offset, so for read_regs(0x7E, buf, 2)
     * it returns mock_regs[0x7E] and mock_regs[0x7F]. For 0x7F: regs[0x7F..0x80] */
    mock_regs[0x7F] = 0x30; /* device ID MSB at reg 0x7F */
    mock_regs[0x80] = 0x01; /* device ID LSB at reg 0x80 */
    /* But we also need manufacturer ID: reg 0x7E reads [0x7E]=0x54, [0x7F]=0x49 */
    mock_regs[0x7E] = 0x54; mock_regs[0x7F] = 0x49;
}

PROPERTY(init_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    plant_ids();

    opt3001_t dev;
    opt3001_init(&dev, &prop_i2c, OPT3001_ADDR_GND_GND);
}

PROPERTY(read_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    plant_ids();
    /* Set CRF bit (bit 7 of config reg high byte) so data_ready returns true */
    mock_regs[0x01] = 0x80;

    opt3001_t dev;
    opt3001_err_t err = opt3001_init(&dev, &prop_i2c, OPT3001_ADDR_GND_GND);
    if (err != OPT3001_OK) return;

    float lux;
    opt3001_read_lux(&dev, &lux);
}

TEST_MAIN("prop_opt3001")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
