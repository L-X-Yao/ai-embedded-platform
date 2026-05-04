// SPDX-License-Identifier: MIT
/*
 * prop_ds18b20.c — property-based tests for DS18B20 1-Wire temp sensor
 */

#include "test_harness.h"
#include "property_test.h"
#include "ds18b20.h"

/* --- 1-Wire mock --- */

static uint8_t mock_scratchpad[9];
static int     mock_byte_idx;
static bool    mock_fail;

static bool mock_reset(void *ctx)
{
    (void)ctx;
    mock_byte_idx = 0;
    return !mock_fail; /* true = presence pulse detected */
}

static bool mock_write_byte(void *ctx, uint8_t byte)
{
    (void)ctx; (void)byte;
    return !mock_fail;
}

static bool mock_read_byte(void *ctx, uint8_t *byte)
{
    (void)ctx;
    if (mock_fail) return false;
    int idx = mock_byte_idx++;
    *byte = (idx < (int)sizeof(mock_scratchpad)) ? mock_scratchpad[idx] : 0xFF;
    return true;
}

static const ds18b20_ow_t prop_ow = {
    .reset      = mock_reset,
    .write_byte = mock_write_byte,
    .read_byte  = mock_read_byte,
    .ctx        = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_fail = false;
    mock_byte_idx = 0;
    memset(mock_scratchpad, 0, sizeof(mock_scratchpad));

    ds18b20_t dev;
    ds18b20_init(&dev, &prop_ow);
}

PROPERTY(read_no_crash, 5000)
{
    mock_fail = false;
    mock_byte_idx = 0;
    for (int i = 0; i < (int)sizeof(mock_scratchpad); i++)
        mock_scratchpad[i] = ARBI_UINT8(seed);
    /* Compute valid CRC for scratchpad[0..7] */
    mock_scratchpad[8] = ds18b20_crc8(mock_scratchpad, 8);

    ds18b20_t dev;
    ds18b20_init(&dev, &prop_ow);
    ds18b20_start_conversion(&dev);

    mock_byte_idx = 0;
    float temp;
    ds18b20_read_temperature(&dev, &temp);
}

PROPERTY(crc_pure, 5000)
{
    uint8_t buf[9];
    size_t len = ARBI_SIZE(seed, 9);
    ARBI_FILL(seed, buf, len);
    uint8_t c = ds18b20_crc8(buf, len);
    (void)c;
}

TEST_MAIN("prop_ds18b20")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(crc_pure);
TEST_MAIN_END()
