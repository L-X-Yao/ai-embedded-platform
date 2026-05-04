// SPDX-License-Identifier: MIT
/*
 * prop_max31855.c — property-based tests for MAX31855 thermocouple
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_spi.h"
#include "max31855.h"

/* MAX31855 protocol: CS low, read 4 bytes (32-bit frame), CS high.
 * Mock returns random bytes per iteration. */

static uint8_t mock_frame[4];
static int     mock_byte_idx;

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx; (void)tx;
    if (mock_spi_should_fail()) return 0xFF;
    int idx = mock_byte_idx++;
    return (idx < 4) ? mock_frame[idx] : 0xFF;
}

static const max31855_spi_t prop_spi = {
    .cs_low  = mock_spi_cs_low,
    .cs_high = mock_spi_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

static void full_reset(void)
{
    mock_spi_reset();
    memset(mock_frame, 0, sizeof(mock_frame));
    mock_byte_idx = 0;
}

PROPERTY(init_no_crash, 10000)
{
    full_reset();
    max31855_t dev;
    max31855_init(&dev, &prop_spi);
}

PROPERTY(read_no_crash, 5000)
{
    full_reset();
    mock_frame[0] = ARBI_UINT8(seed);
    mock_frame[1] = ARBI_UINT8(seed);
    mock_frame[2] = ARBI_UINT8(seed);
    mock_frame[3] = ARBI_UINT8(seed);

    max31855_t dev;
    max31855_init(&dev, &prop_spi);

    mock_byte_idx = 0;
    float tc, internal;
    max31855_read(&dev, &tc, &internal);
    /* must not crash regardless of frame content */
}

PROPERTY(read_raw_no_crash, 5000)
{
    full_reset();
    mock_frame[0] = ARBI_UINT8(seed);
    mock_frame[1] = ARBI_UINT8(seed);
    mock_frame[2] = ARBI_UINT8(seed);
    mock_frame[3] = ARBI_UINT8(seed);

    max31855_t dev;
    max31855_init(&dev, &prop_spi);

    mock_byte_idx = 0;
    uint32_t raw;
    max31855_read_raw(&dev, &raw);
}

TEST_MAIN("prop_max31855")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(read_raw_no_crash);
TEST_MAIN_END()
