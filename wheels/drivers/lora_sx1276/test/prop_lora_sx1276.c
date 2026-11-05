// SPDX-License-Identifier: MIT
/*
 * prop_lora_sx1276.c — property-based tests for SX1276 LoRa radio
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_spi.h"
#include "mock_time.h"
#include "lora_sx1276.h"

/* SX1276 SPI protocol: byte 0 = addr (MSB=1 for write), subsequent = data */

static uint8_t sx_regs[128];
static int     sx_phase;

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (mock_spi_should_fail()) return 0xFF;

    int phase = sx_phase++;
    if (phase == 0) {
        uint8_t addr = tx & 0x7F;
        if (tx & 0x80) {
            /* Write */
        }
        (void)addr;
        return sx_regs[0x07]; /* status-like return */
    }
    uint8_t addr0 = (uint8_t)(mock_spi_tx_log_count > 0 ?
                    (mock_spi_tx_log[0] & 0x7F) : 0);
    uint8_t reg = (uint8_t)(addr0 + (uint8_t)(phase - 1));
    if (mock_spi_tx_log_count > 0 && (mock_spi_tx_log[0] & 0x80)) {
        if (reg < sizeof(sx_regs)) sx_regs[reg] = tx;
        return 0xFF;
    }
    return (reg < sizeof(sx_regs)) ? sx_regs[reg] : 0xFF;
}

static void mock_reset_pin(void *ctx) { (void)ctx; }

static void sx_cs_low(void *ctx)
{
    mock_spi_cs_low(ctx);
    mock_spi_tx_log_count = 0;
    sx_phase = 0;
}

static const lora_spi_t prop_spi = {
    .cs_low   = sx_cs_low,
    .cs_high  = mock_spi_cs_high,
    .xfer     = mock_xfer,
    .reset    = mock_reset_pin,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

static void full_reset(void)
{
    mock_spi_reset();
    memset(sx_regs, 0, sizeof(sx_regs));
    sx_regs[0x42] = 0x12; /* VERSION register = SX1276 */
    sx_phase = 0;
}

PROPERTY(init_no_crash, 10000)
{
    full_reset();
    for (int i = 0; i < (int)sizeof(sx_regs); i++)
        sx_regs[i] = ARBI_UINT8(seed);
    sx_regs[0x42] = 0x12;

    lora_t dev;
    lora_init(&dev, &prop_spi, 433000000);
}

PROPERTY(config_no_crash, 5000)
{
    full_reset();
    for (int i = 0; i < (int)sizeof(sx_regs); i++)
        sx_regs[i] = ARBI_UINT8(seed);
    sx_regs[0x42] = 0x12;

    lora_t dev;
    lora_err_t err = lora_init(&dev, &prop_spi, 868000000);
    if (err != LORA_OK) return;

    lora_set_bandwidth(&dev, ARBI_UINT8(seed) % 10);
    lora_set_spreading_factor(&dev, (uint8_t)(7 + ARBI_UINT8(seed) % 6));
    lora_set_coding_rate(&dev, (uint8_t)(1 + ARBI_UINT8(seed) % 4));
    lora_set_tx_power(&dev, (int8_t)(2 + (int)(ARBI_UINT8(seed) % 16)));
}

TEST_MAIN("prop_lora_sx1276")
    RUN(init_no_crash);
    RUN(config_no_crash);
TEST_MAIN_END()
