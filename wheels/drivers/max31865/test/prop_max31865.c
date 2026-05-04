// SPDX-License-Identifier: MIT
/*
 * prop_max31865.c — property-based tests for MAX31865 RTD converter
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_spi.h"
#include "max31865.h"

/* MAX31865 SPI: register-based. Byte 0 = reg addr (read: 0x00-0x07,
 * write: 0x80-0x87). Subsequent bytes are register data. */

static uint8_t mock_regs_spi[16];
static int     mock_phase;
static uint8_t mock_selected_reg;

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (mock_spi_should_fail()) return 0xFF;

    int phase = mock_phase++;
    if (phase == 0) {
        /* Address byte */
        if (tx & 0x80) {
            /* Write: next byte is data */
            mock_selected_reg = (uint8_t)(tx & 0x7F);
            return 0xFF;
        }
        mock_selected_reg = tx;
        return 0xFF;
    }
    /* Data bytes */
    if (tx != 0xFF && (mock_selected_reg & 0x80) == 0) {
        /* Write data */
        uint8_t reg = (uint8_t)(mock_selected_reg + (uint8_t)(phase - 1));
        if (reg < sizeof(mock_regs_spi)) mock_regs_spi[reg] = tx;
        return 0xFF;
    }
    /* Read data */
    uint8_t reg = (uint8_t)(mock_selected_reg + (uint8_t)(phase - 1));
    return (reg < sizeof(mock_regs_spi)) ? mock_regs_spi[reg] : 0xFF;
}

static void spi_cs_low(void *ctx)  { mock_spi_cs_low(ctx); mock_phase = 0; }
static void spi_cs_high(void *ctx) { mock_spi_cs_high(ctx); }

static const max31865_spi_t prop_spi = {
    .cs_low  = spi_cs_low,
    .cs_high = spi_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

static void full_reset(void)
{
    mock_spi_reset();
    memset(mock_regs_spi, 0, sizeof(mock_regs_spi));
    mock_phase = 0;
    mock_selected_reg = 0;
}

PROPERTY(init_no_crash, 10000)
{
    full_reset();
    for (int i = 0; i < (int)sizeof(mock_regs_spi); i++)
        mock_regs_spi[i] = ARBI_UINT8(seed);

    max31865_t dev;
    max31865_cfg_t cfg = {
        .r_ref = 430.0f,
        .r_nom = 100.0f,
        .wire  = MAX31865_WIRE_2,
        .filter_50hz = false,
    };
    max31865_init(&dev, &prop_spi, &cfg);
}

PROPERTY(read_no_crash, 5000)
{
    full_reset();
    for (int i = 0; i < (int)sizeof(mock_regs_spi); i++)
        mock_regs_spi[i] = ARBI_UINT8(seed);
    /* Clear fault bit (bit 0 of RTD LSB) */
    mock_regs_spi[0x02] &= 0xFE;

    max31865_t dev;
    max31865_cfg_t cfg = {
        .r_ref = 430.0f,
        .r_nom = 100.0f,
        .wire  = MAX31865_WIRE_2,
        .filter_50hz = false,
    };
    max31865_init(&dev, &prop_spi, &cfg);

    float temp;
    max31865_read_temperature(&dev, &temp);

    uint16_t rtd_raw;
    max31865_read_rtd_raw(&dev, &rtd_raw);
}

TEST_MAIN("prop_max31865")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
