// SPDX-License-Identifier: MIT
/*
 * test_hw_model_mcp3008.c -- hw_model self-tests
 *
 * Validates that hw_model_mcp3008 decodes and encodes the 3-byte SPI
 * protocol correctly for all 8 single-ended channels and 4 diff pairs.
 * These tests run on host and do NOT require hardware.
 */
#include "test_harness.h"
#include "hw_model_mcp3008.h"
#include "mcp3008.h"

static mcp3008_model_t model;

static const mcp3008_spi_t hw_spi = {
    .ctx         = &model,
    .cs_select   = mcp3008_model_cs_select,
    .cs_deselect = mcp3008_model_cs_deselect,
    .transfer    = mcp3008_model_transfer,
};

static mcp3008_t dev;

static void setup(void)
{
    mcp3008_model_init(&model);
    (void)mcp3008_init(&dev, &hw_spi);
}

/* hw_model initialises all channels to 0 after reset */
TEST(test_hw_model_init_all_zero)
{
    setup();
    for (uint8_t ch = 0; ch < 8; ch++)
    {
        uint16_t raw;
        ASSERT_EQ(mcp3008_read_single(&dev, ch, &raw), MCP3008_OK);
        ASSERT_EQ(raw, 0);
    }
}

/* Round-trip: set each single-ended channel, read it back */
TEST(test_hw_model_single_round_trip_all_channels)
{
    setup();
    for (uint8_t ch = 0; ch < 8; ch++)
    {
        uint16_t expected = (uint16_t)(ch * 128u + 1u);
        mcp3008_model_set_single(&model, ch, expected);

        uint16_t raw;
        ASSERT_EQ(mcp3008_read_single(&dev, ch, &raw), MCP3008_OK);
        ASSERT_EQ(raw, expected);
    }
}

/* hw_model clamps values to 10 bits */
TEST(test_hw_model_clamps_to_10bit)
{
    setup();
    mcp3008_model_set_single(&model, 0, 0xFFFFu);  /* wider than 10 bits */

    uint16_t raw;
    ASSERT_EQ(mcp3008_read_single(&dev, 0, &raw), MCP3008_OK);
    ASSERT_EQ(raw, 0x03FFu);  /* clamped to 10 bits */
}

/* Round-trip: set each differential pair, read it back */
TEST(test_hw_model_diff_round_trip_all_pairs)
{
    setup();
    for (uint8_t pair = 0; pair < 4; pair++)
    {
        uint16_t expected = (uint16_t)(pair * 250u + 50u);
        mcp3008_model_set_diff(&model, pair, expected);

        uint16_t raw;
        ASSERT_EQ(mcp3008_read_diff(&dev, pair, &raw), MCP3008_OK);
        ASSERT_EQ(raw, expected);
    }
}

/* DS21295D Table 5-2: raw D2 D1 D0 codes reach the pair the table names.
 * The frame is sent directly, so this does not depend on the driver. */
TEST(test_hw_model_diff_table_5_2)
{
    /* SGL/DIFF D2 D1 D0 in the top nibble of byte 2 (Figure 6-1) */
    static const struct { uint8_t cfg; uint16_t raw; } rows[] = {
        { 0x00u, 100u },  /* 0 0 0 0: CH0 = IN+, CH1 = IN- */
        { 0x10u,   0u },  /* 0 0 0 1: CH0 = IN-, CH1 = IN+ */
        { 0x20u, 200u },  /* 0 0 1 0: CH2 = IN+, CH3 = IN- */
        { 0x30u,   0u },  /* 0 0 1 1: CH2 = IN-, CH3 = IN+ */
        { 0x40u, 300u },  /* 0 1 0 0: CH4 = IN+, CH5 = IN- */
        { 0x50u,   0u },  /* 0 1 0 1: CH4 = IN-, CH5 = IN+ */
        { 0x60u, 400u },  /* 0 1 1 0: CH6 = IN+, CH7 = IN- */
        { 0x70u,   0u },  /* 0 1 1 1: CH6 = IN-, CH7 = IN+ */
    };

    setup();
    mcp3008_model_set_diff(&model, 0, 100);
    mcp3008_model_set_diff(&model, 1, 200);
    mcp3008_model_set_diff(&model, 2, 300);
    mcp3008_model_set_diff(&model, 3, 400);

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
    {
        uint8_t tx[3] = { 0x01u, rows[i].cfg, 0x00u };
        uint8_t rx[3] = { 0xFFu, 0xFFu, 0xFFu };
        ASSERT_TRUE(mcp3008_model_transfer(&model, tx, rx, 3));
        ASSERT_EQ((rx[1] & 0x03u) << 8 | rx[2], rows[i].raw);
    }
}

/* hw_model counts transfers correctly */
TEST(test_hw_model_transfer_count)
{
    setup();
    uint16_t raw;
    (void)mcp3008_read_single(&dev, 0, &raw);
    (void)mcp3008_read_single(&dev, 1, &raw);
    ASSERT_EQ(model.transfer_count, 2);
}

/* hw_model fail_cs causes MCP3008_ERR_SPI */
TEST(test_hw_model_fail_cs)
{
    setup();
    model.fail_cs = true;
    uint16_t raw;
    ASSERT_EQ(mcp3008_read_single(&dev, 0, &raw), MCP3008_ERR_SPI);
}

/* hw_model fail_transfer causes MCP3008_ERR_SPI */
TEST(test_hw_model_fail_transfer)
{
    setup();
    model.fail_transfer = true;
    uint16_t raw;
    ASSERT_EQ(mcp3008_read_single(&dev, 0, &raw), MCP3008_ERR_SPI);
}

TEST_MAIN("mcp3008_hw_model")
    RUN(test_hw_model_init_all_zero);
    RUN(test_hw_model_single_round_trip_all_channels);
    RUN(test_hw_model_clamps_to_10bit);
    RUN(test_hw_model_diff_round_trip_all_pairs);
    RUN(test_hw_model_diff_table_5_2);
    RUN(test_hw_model_transfer_count);
    RUN(test_hw_model_fail_cs);
    RUN(test_hw_model_fail_transfer);
TEST_MAIN_END()
