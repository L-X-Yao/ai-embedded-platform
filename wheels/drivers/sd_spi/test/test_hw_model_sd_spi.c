// SPDX-License-Identifier: MIT
/*
 * test_hw_model_sd_spi.c -- tests using SD SPI card hardware model
 *
 * Validates the sd_spi driver against a protocol-accurate model with
 * command-response state machine, card type detection (SDv1/SDv2/SDHC),
 * block read/write via data tokens, and configurable error injection.
 *
 * Tests: init sequence, card type detection (SDv2 HC, SDv2 SC, SDv1),
 * read single block, write single block, write data verification,
 * ACMD41 retry loop, CMD0 failure, read timeout, write rejection,
 * CRC error response, SDHC block addressing vs SDSC byte addressing.
 */

#include "test_harness.h"
#include "hw_model_sd_spi.h"
#include "../sd_spi.h"

static sd_model_t model;

static const sd_spi_hal_t model_hal = {
    .cs_low   = sd_model_cs_low,
    .cs_high  = sd_model_cs_high,
    .xfer     = sd_model_xfer,
    .delay_ms = sd_model_delay_ms,
    .ctx      = &model,
};

/* --- 1. Init sequence: CMD0 -> CMD8 -> ACMD41 -> CMD58 (SDHC) --- */

TEST(test_init_sdhc)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_HC;

    sd_spi_t sd;
    sd_spi_err_t err = sd_spi_init(&sd, &model_hal);
    ASSERT_EQ(err, SD_SPI_OK);
    ASSERT_EQ(sd.card_type, SD_SPI_TYPE_SDV2_HC);

    /* Verify command sequence was executed */
    ASSERT_TRUE(model.cmd0_count > 0);
    ASSERT_TRUE(model.cmd8_count > 0);
    ASSERT_TRUE(model.acmd41_count > 0);
    ASSERT_TRUE(model.cmd58_count > 0);

    /* Card should be in transfer state */
    ASSERT_EQ(model.state, SD_STATE_TRANSFER);
}

/* --- 2. Init sequence: SDv2 standard capacity --- */

TEST(test_init_sdsc)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_SC;

    sd_spi_t sd;
    sd_spi_err_t err = sd_spi_init(&sd, &model_hal);
    ASSERT_EQ(err, SD_SPI_OK);
    ASSERT_EQ(sd.card_type, SD_SPI_TYPE_SDV2_SC);
}

/* --- 3. Init sequence: SDv1 (CMD8 returns illegal command) --- */

TEST(test_init_sdv1)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV1;

    sd_spi_t sd;
    sd_spi_err_t err = sd_spi_init(&sd, &model_hal);
    ASSERT_EQ(err, SD_SPI_OK);
    ASSERT_EQ(sd.card_type, SD_SPI_TYPE_SDV1);

    /* CMD8 should have been attempted but returned illegal */
    ASSERT_TRUE(model.cmd8_count > 0);
    /* ACMD41 still called for SDv1 */
    ASSERT_TRUE(model.acmd41_count > 0);
    /* CMD58 is NOT called for SDv1 path in the driver */
}

/* --- 4. Read single block: CMD17 -> 0xFE + 512 bytes + 2 CRC --- */

TEST(test_read_single_block)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_HC;

    /* Pre-load block 0 with known data */
    uint8_t block_data[512];
    for (int i = 0; i < 512; i++)
        block_data[i] = (uint8_t)(i & 0xFF);
    sd_model_set_block(&model, 0, block_data);

    sd_spi_t sd;
    sd_spi_init(&sd, &model_hal);

    uint8_t buf[512];
    memset(buf, 0, sizeof(buf));
    sd_spi_err_t err = sd_spi_read_block(&sd, 0, buf);
    ASSERT_EQ(err, SD_SPI_OK);

    /* Verify data matches what was pre-loaded */
    ASSERT_EQ(buf[0], 0x00);
    ASSERT_EQ(buf[1], 0x01);
    ASSERT_EQ(buf[255], 0xFF);
    ASSERT_EQ(buf[256], 0x00);
    ASSERT_EQ(buf[511], 0xFF);
}

/* --- 5. Write single block: CMD24 -> 0xFE + 512 bytes + response --- */

TEST(test_write_single_block)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_HC;

    sd_spi_t sd;
    sd_spi_init(&sd, &model_hal);

    uint8_t data[512];
    for (int i = 0; i < 512; i++)
        data[i] = (uint8_t)(0xFF - (i & 0xFF));

    sd_spi_err_t err = sd_spi_write_block(&sd, 0, data);
    ASSERT_EQ(err, SD_SPI_OK);

    /* Verify data was captured by the model */
    ASSERT_TRUE(model.write_captured);
    ASSERT_EQ(model.last_write_block, 0U);

    /* Verify block storage contains the written data */
    ASSERT_TRUE(model.block_valid[0]);
    ASSERT_EQ(model.blocks[0][0], 0xFF);
    ASSERT_EQ(model.blocks[0][1], 0xFE);
    ASSERT_EQ(model.blocks[0][511], 0x00);
}

/* --- 6. Write then read back: data integrity --- */

TEST(test_write_read_cycle)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_HC;

    sd_spi_t sd;
    sd_spi_init(&sd, &model_hal);

    /* Write a block with a pattern */
    uint8_t wr[512];
    for (int i = 0; i < 512; i++)
        wr[i] = (uint8_t)((i * 7 + 13) & 0xFF);

    ASSERT_EQ(sd_spi_write_block(&sd, 3, wr), SD_SPI_OK);

    /* Read it back */
    uint8_t rd[512];
    memset(rd, 0, sizeof(rd));
    ASSERT_EQ(sd_spi_read_block(&sd, 3, rd), SD_SPI_OK);

    /* Data should match */
    ASSERT_TRUE(memcmp(wr, rd, 512) == 0);
}

/* --- 7. ACMD41 retry loop (card needs multiple attempts) --- */

TEST(test_acmd41_retry)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_HC;
    model.acmd41_remain = 5;   /* needs 5 retries before ready */

    sd_spi_t sd;
    sd_spi_err_t err = sd_spi_init(&sd, &model_hal);
    ASSERT_EQ(err, SD_SPI_OK);
    ASSERT_EQ(sd.card_type, SD_SPI_TYPE_SDV2_HC);

    /* ACMD41 should have been called at least 6 times (5 retries + 1 success) */
    ASSERT_TRUE(model.acmd41_count >= 6);
}

/* --- 8. CMD0 failure: card doesn't go idle --- */

TEST(test_cmd0_failure)
{
    sd_model_init(&model);
    model.fail_cmd0 = true;

    sd_spi_t sd;
    sd_spi_err_t err = sd_spi_init(&sd, &model_hal);
    ASSERT_EQ(err, SD_SPI_ERR_INIT);
}

/* --- 9. Read timeout: no data token after CMD17 --- */

TEST(test_read_timeout)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_HC;

    sd_spi_t sd;
    sd_spi_init(&sd, &model_hal);

    /* Enable read failure: no 0xFE token sent */
    model.fail_read = true;

    uint8_t buf[512];
    sd_spi_err_t err = sd_spi_read_block(&sd, 0, buf);
    ASSERT_EQ(err, SD_SPI_ERR_TIMEOUT);
}

/* --- 10. Write rejection: card returns error response --- */

TEST(test_write_rejected)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_HC;

    sd_spi_t sd;
    sd_spi_init(&sd, &model_hal);

    model.fail_write = true;

    uint8_t data[512];
    memset(data, 0xAA, sizeof(data));
    sd_spi_err_t err = sd_spi_write_block(&sd, 0, data);
    ASSERT_EQ(err, SD_SPI_ERR_WRITE);
}

/* --- 11. SDHC block addressing vs SDSC byte addressing --- */

TEST(test_sdhc_block_addressing)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_HC;

    /* Pre-load block 5 */
    uint8_t block_data[512];
    memset(block_data, 0x55, sizeof(block_data));
    block_data[0] = 0xAA;
    sd_model_set_block(&model, 5, block_data);

    sd_spi_t sd;
    sd_spi_init(&sd, &model_hal);

    /* Read block 5: SDHC passes block number directly as arg */
    uint8_t buf[512];
    ASSERT_EQ(sd_spi_read_block(&sd, 5, buf), SD_SPI_OK);
    ASSERT_EQ(buf[0], 0xAA);

    /* model.read_block should be 5 (block address) */
    ASSERT_EQ(model.read_block, 5U);
}

TEST(test_sdsc_byte_addressing)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_SC;

    /* Pre-load block 2 */
    uint8_t block_data[512];
    memset(block_data, 0x77, sizeof(block_data));
    block_data[0] = 0xBB;
    sd_model_set_block(&model, 2, block_data);

    sd_spi_t sd;
    sd_spi_init(&sd, &model_hal);

    /* Read block 2: SDSC converts to byte address (2 * 512 = 1024) */
    uint8_t buf[512];
    ASSERT_EQ(sd_spi_read_block(&sd, 2, buf), SD_SPI_OK);
    ASSERT_EQ(buf[0], 0xBB);

    /* model.read_block should be 2 (converted from byte address 1024) */
    ASSERT_EQ(model.read_block, 2U);
}

/* --- 12. SPI bus failure returns error --- */

TEST(test_spi_bus_failure)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_HC;

    sd_spi_t sd;
    sd_spi_init(&sd, &model_hal);

    /* Enable SPI failure: all xfer return 0xFF */
    model.fail_spi = true;

    uint8_t buf[512];
    sd_spi_err_t err = sd_spi_read_block(&sd, 0, buf);
    /* With all-0xFF bus, send_cmd returns 0xFF (no valid R1),
     * driver treats as r1 != 0x00 -> ERR_CMD */
    ASSERT_TRUE(err != SD_SPI_OK);
}

TEST_MAIN("sd_spi hardware model")
    RUN(test_init_sdhc);
    RUN(test_init_sdsc);
    RUN(test_init_sdv1);
    RUN(test_read_single_block);
    RUN(test_write_single_block);
    RUN(test_write_read_cycle);
    RUN(test_acmd41_retry);
    RUN(test_cmd0_failure);
    RUN(test_read_timeout);
    RUN(test_write_rejected);
    RUN(test_sdhc_block_addressing);
    RUN(test_sdsc_byte_addressing);
    RUN(test_spi_bus_failure);
TEST_MAIN_END()
