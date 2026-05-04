// SPDX-License-Identifier: MIT
/*
 * prop_w25qxx.c — property-based tests for W25Qxx SPI NOR flash
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_spi.h"
#include "w25qxx.h"

/* W25Q SPI protocol: first byte is command, subsequent are address/data.
 * Mock returns JEDEC ID bytes for cmd 0x9F, status for cmd 0x05. */

static uint8_t mock_jedec[3];
static uint8_t mock_status_reg;
static int     mock_cmd_phase;

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (mock_spi_should_fail()) return 0xFF;
    mock_spi_log_byte(tx);

    int phase = mock_cmd_phase++;
    /* Detect JEDEC read (0x9F): bytes 1,2,3 return MFR, type, cap */
    if (mock_spi_tx_log_count >= 1 && mock_spi_tx_log[0] == 0x9F) {
        if (phase >= 1 && phase <= 3)
            return mock_jedec[phase - 1];
    }
    /* Read status (0x05): byte 1 returns status */
    if (mock_spi_tx_log_count >= 1 && mock_spi_tx_log[0] == 0x05) {
        if (phase == 1) return mock_status_reg;
    }
    return 0xFF;
}

static const w25qxx_spi_t prop_spi = {
    .cs_low  = mock_spi_cs_low,
    .cs_high = mock_spi_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

static void full_reset(void)
{
    mock_spi_reset();
    mock_jedec[0] = 0xEF; /* Winbond */
    mock_jedec[1] = 0x40;
    mock_jedec[2] = 0x16; /* W25Q32 */
    mock_status_reg = 0x00; /* not busy */
    mock_cmd_phase = 0;
}

PROPERTY(init_no_crash, 10000)
{
    full_reset();
    mock_jedec[0] = ARBI_UINT8(seed);
    mock_jedec[1] = ARBI_UINT8(seed);
    mock_jedec[2] = ARBI_UINT8(seed);

    uint32_t jedec;
    w25qxx_init(&prop_spi, &jedec);
}

PROPERTY(read_no_crash, 5000)
{
    full_reset();
    uint32_t jedec;
    w25qxx_init(&prop_spi, &jedec);

    mock_spi_reset();
    mock_cmd_phase = 0;
    uint8_t buf[16];
    uint32_t addr = ARBI_UINT32(seed) & 0x3FFFFF;
    w25qxx_read(&prop_spi, addr, buf, sizeof(buf));
}

TEST_MAIN("prop_w25qxx")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
