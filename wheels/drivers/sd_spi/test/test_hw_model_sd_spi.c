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

    /* A card that says nothing after R1 */
    model.silent_read = true;

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

/* --- 13. What the card does, from Physical Layer Spec v3.01 --- */

/* One command frame sent straight to the model. Returns the first byte
 * with the top bit clear, or 0xFF after the 8 bytes of NCR. */
static uint8_t raw_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    sd_model_cs_high(&model);
    sd_model_cs_low(&model);
    (void)sd_model_xfer(&model, 0xFF);
    (void)sd_model_xfer(&model, (uint8_t)(0x40U | cmd));
    (void)sd_model_xfer(&model, (uint8_t)(arg >> 24));
    (void)sd_model_xfer(&model, (uint8_t)(arg >> 16));
    (void)sd_model_xfer(&model, (uint8_t)(arg >> 8));
    (void)sd_model_xfer(&model, (uint8_t)arg);
    (void)sd_model_xfer(&model, crc);

    uint8_t r1 = 0xFF;
    for (int i = 0; i < 8 && (r1 & 0x80U); i++)
        r1 = sd_model_xfer(&model, 0xFF);
    return r1;
}

static uint8_t raw_acmd41(uint32_t arg)
{
    (void)raw_cmd(55, 0, 0xFF);
    return raw_cmd(41, arg, 0xFF);
}

#define VALID_CMD0_CRC  0x95   /* 7.2.2: 0x40, 0x0, 0x0, 0x0, 0x0, 0x95 */
#define VALID_CMD8_CRC  0x87   /* CRC7 of 48 00 00 01 AA, then the end bit */
#define HCS             0x40000000U

TEST(test_crc7_matches_the_examples_in_4_5)
{
    /* CMD0 (Argument=0) --> 01 000000 00...00 "1001010" 1 */
    static const uint8_t cmd0[5] = { 0x40, 0, 0, 0, 0 };
    /* CMD17 (Argument=0) --> 01 010001 00...00 "0101010" 1 */
    static const uint8_t cmd17[5] = { 0x51, 0, 0, 0, 0 };
    /* Response of CMD17 --> 00 010001 00..00100100000000 "0110011" 1 */
    static const uint8_t resp17[5] = { 0x11, 0, 0, 0x09, 0 };

    ASSERT_EQ_HEX(sd_model_crc7(cmd0, 5), 0x4A);
    ASSERT_EQ_HEX(sd_model_crc7(cmd17, 5), 0x2A);
    ASSERT_EQ_HEX(sd_model_crc7(resp17, 5), 0x33);
}

TEST(test_first_cmd0_needs_a_valid_crc)
{
    sd_model_init(&model);

    /* 7.2.2: CMD0 is received in SD mode, "therefore, shall have a valid
     * CRC field". */
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0xFF), 0xFF);
    ASSERT_EQ_HEX(raw_cmd(0, 0, VALID_CMD0_CRC), 0x01);

    /* In SPI mode the CRC is off until CMD59 turns it on. */
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0xFF), 0x01);
}

TEST(test_cmd0_must_come_first)
{
    sd_model_init(&model);

    /* 6.4.1: "CMD0 shall be the first command to send the card to SPI
     * mode". */
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, VALID_CMD8_CRC), 0xFF);
    ASSERT_EQ_HEX(raw_cmd(58, 0, 0xFF), 0xFF);
}

TEST(test_bad_cmd8_crc_gets_a_crc_error)
{
    sd_model_init(&model);
    ASSERT_EQ_HEX(raw_cmd(0, 0, VALID_CMD0_CRC), 0x01);

    /* 7.2.2: CMD8 CRC is always checked. 7.3.2: only the first byte is
     * sent when a CRC error is indicated. */
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, 0xFF), 0x09);
    ASSERT_EQ_HEX(sd_model_xfer(&model, 0xFF), 0xFF);

    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, VALID_CMD8_CRC), 0x01);
    ASSERT_EQ_HEX(sd_model_xfer(&model, 0xFF), 0x00);
    ASSERT_EQ_HEX(sd_model_xfer(&model, 0xFF), 0x00);
    ASSERT_EQ_HEX(sd_model_xfer(&model, 0xFF), 0x01);
    ASSERT_EQ_HEX(sd_model_xfer(&model, 0xFF), 0xAA);
}

TEST(test_sdhc_never_ready_without_hcs)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_HC;
    ASSERT_EQ_HEX(raw_cmd(0, 0, VALID_CMD0_CRC), 0x01);
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, VALID_CMD8_CRC), 0x01);

    /* 4.2.3: "If HCS is set to 0, SDHC and SDXC Cards never return ready
     * status". */
    for (int i = 0; i < 50; i++)
        ASSERT_EQ_HEX(raw_acmd41(0), 0x01);

    /* 7.2.1: "The card checks the HCS bit in the OCR only at the first
     * ACMD41", so setting it now is too late. */
    ASSERT_EQ_HEX(raw_acmd41(HCS), 0x01);

    /* CMD0 starts over. */
    ASSERT_EQ_HEX(raw_cmd(0, 0, 0xFF), 0x01);
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, VALID_CMD8_CRC), 0x01);
    ASSERT_EQ_HEX(raw_acmd41(HCS), 0x00);
}

TEST(test_sdhc_needs_cmd8_before_acmd41)
{
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_HC;
    ASSERT_EQ_HEX(raw_cmd(0, 0, VALID_CMD0_CRC), 0x01);

    /* 7.2.1: "It is mandatory to issue CMD8 prior to the first ACMD41",
     * and only CMD8 makes ACMD41 look at HCS. */
    ASSERT_EQ_HEX(raw_acmd41(HCS), 0x01);
    ASSERT_EQ_HEX(raw_acmd41(HCS), 0x01);
}

TEST(test_standard_capacity_cards_ignore_hcs)
{
    /* 4.2.3: "Standard Capacity SD Memory Card ignores HCS". */
    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV2_SC;
    ASSERT_EQ_HEX(raw_cmd(0, 0, VALID_CMD0_CRC), 0x01);
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, VALID_CMD8_CRC), 0x01);
    ASSERT_EQ_HEX(raw_acmd41(0), 0x00);

    sd_model_init(&model);
    model.card_type = SD_MODEL_TYPE_SDV1;
    ASSERT_EQ_HEX(raw_cmd(0, 0, VALID_CMD0_CRC), 0x01);
    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, VALID_CMD8_CRC), 0x05);
    ASSERT_EQ_HEX(raw_acmd41(HCS), 0x00);
}

TEST(test_idle_state_allows_only_the_init_commands)
{
    sd_model_init(&model);
    ASSERT_EQ_HEX(raw_cmd(0, 0, VALID_CMD0_CRC), 0x01);

    /* 7.2.7: only CMD8, ACMD41, CMD58 and CMD59 are valid here, and the
     * idle bit stays set (7.3.2.1). 0x05 = idle + illegal command. */
    ASSERT_EQ_HEX(raw_cmd(17, 0, 0xFF), 0x05);
    ASSERT_EQ_HEX(raw_cmd(24, 0, 0xFF), 0x05);
    ASSERT_EQ_HEX(raw_cmd(16, 512, 0xFF), 0x05);
    ASSERT_EQ_HEX(raw_cmd(13, 0, 0xFF), 0x05);

    /* CMD58 is valid, but the power up bit is clear and CCS is not
     * valid yet (5.1). */
    ASSERT_EQ_HEX(raw_cmd(58, 0, 0xFF), 0x01);
    ASSERT_EQ_HEX(sd_model_xfer(&model, 0xFF) & 0xC0U, 0x00);

    ASSERT_EQ_HEX(raw_cmd(8, 0x1AA, VALID_CMD8_CRC), 0x01);
    ASSERT_EQ_HEX(raw_acmd41(HCS), 0x00);

    /* Ready: the idle bit is gone from every R1 and transfers work. */
    ASSERT_EQ_HEX(raw_cmd(55, 0, 0xFF), 0x00);
    ASSERT_EQ_HEX(raw_cmd(58, 0, 0xFF), 0x00);
    ASSERT_EQ_HEX(sd_model_xfer(&model, 0xFF) & 0xC0U, 0xC0);
    ASSERT_EQ_HEX(raw_cmd(17, 0, 0xFF), 0x00);
}

TEST(test_read_and_write_are_refused_before_init)
{
    sd_model_init(&model);
    ASSERT_EQ_HEX(raw_cmd(0, 0, VALID_CMD0_CRC), 0x01);
    sd_model_cs_high(&model);

    sd_spi_t sd;
    memset(&sd, 0, sizeof(sd));
    sd.hal = &model_hal;
    sd.card_type = SD_SPI_TYPE_SDV2_HC;

    uint8_t buf[512] = { 0 };
    ASSERT_EQ(sd_spi_read_block(&sd, 0, buf), SD_SPI_ERR_CMD);
    ASSERT_EQ(sd_spi_write_block(&sd, 0, buf), SD_SPI_ERR_CMD);
}

TEST(test_write_waits_for_the_busy_line)
{
    sd_model_init(&model);
    sd_spi_t sd;
    ASSERT_EQ(sd_spi_init(&sd, &model_hal), SD_SPI_OK);

    /* 7.2.4: "a continuous stream of busy tokens will be sent to the
     * host (effectively holding the DataOut line low)". */
    model.busy_polls = 300;
    uint8_t buf[512] = { 0 };
    ASSERT_EQ(sd_spi_write_block(&sd, 1, buf), SD_SPI_OK);
    ASSERT_EQ(model.busy_seen, 300);
}

TEST(test_write_gives_up_when_busy_never_ends)
{
    sd_model_init(&model);
    sd_spi_t sd;
    ASSERT_EQ(sd_spi_init(&sd, &model_hal), SD_SPI_OK);

    model.busy_polls = -1;
    uint8_t buf[512] = { 0 };
    ASSERT_EQ(sd_spi_write_block(&sd, 1, buf), SD_SPI_ERR_TIMEOUT);
    /* 4.6.2.2: an SDXC card "can indicate write busy up to 500ms" */
    ASSERT_TRUE(model.delay_count > 500);
}

TEST(test_reselect_during_programming_keeps_dataout_low)
{
    sd_model_init(&model);
    sd_spi_t sd;
    ASSERT_EQ(sd_spi_init(&sd, &model_hal), SD_SPI_OK);

    model.busy_polls = -1;
    uint8_t buf[512] = { 0 };
    ASSERT_EQ(sd_spi_write_block(&sd, 1, buf), SD_SPI_ERR_TIMEOUT);

    /* 7.2.4: with CS high "The card will release the DataOut line".
     * "If the card is reselected before the programming is finished, the
     * DataOut line will be forced back to low and all commands will be
     * rejected." */
    sd_model_cs_high(&model);
    ASSERT_EQ_HEX(sd_model_xfer(&model, 0xFF), 0xFF);
    sd_model_cs_low(&model);
    ASSERT_EQ_HEX(sd_model_xfer(&model, 0xFF), 0x00);
    sd_model_cs_high(&model);

    /* A card in this state must not look like a working one. */
    ASSERT_TRUE(sd_spi_read_block(&sd, 1, buf) != SD_SPI_OK);
    ASSERT_TRUE(sd_spi_write_block(&sd, 1, buf) != SD_SPI_OK);
}

TEST(test_programming_error_shows_only_in_cmd13)
{
    sd_model_init(&model);
    sd_spi_t sd;
    ASSERT_EQ(sd_spi_init(&sd, &model_hal), SD_SPI_OK);

    /* 7.2.4: "Some errors (e.g. address out of range, write protect
     * violation etc.) are detected during programming only." */
    model.status_r2 = 0x20;   /* WP violation, Figure 7-10 */
    uint8_t buf[512] = { 0 };
    ASSERT_EQ(sd_spi_write_block(&sd, 1, buf), SD_SPI_ERR_WRITE);

    /* 7.3.4: "error bits are cleared when read by the host" */
    ASSERT_EQ(sd_spi_write_block(&sd, 1, buf), SD_SPI_OK);
}

TEST(test_read_error_token_reaches_the_caller)
{
    sd_model_init(&model);
    sd_spi_t sd;
    ASSERT_EQ(sd_spi_init(&sd, &model_hal), SD_SPI_OK);

    /* 7.3.3.3: "it will send a data error token instead", Figure 7-13:
     * 0000 then out of range, card ECC failed, CC error, error. */
    uint8_t buf[512];
    static const uint8_t tokens[] = { 0x01, 0x02, 0x04, 0x08 };
    model.fail_read = true;
    for (unsigned i = 0; i < sizeof(tokens); i++) {
        model.read_error_token = tokens[i];
        ASSERT_EQ(sd_spi_read_block(&sd, 0, buf), SD_SPI_ERR_READ);
    }
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
    RUN(test_crc7_matches_the_examples_in_4_5);
    RUN(test_first_cmd0_needs_a_valid_crc);
    RUN(test_cmd0_must_come_first);
    RUN(test_bad_cmd8_crc_gets_a_crc_error);
    RUN(test_sdhc_never_ready_without_hcs);
    RUN(test_sdhc_needs_cmd8_before_acmd41);
    RUN(test_standard_capacity_cards_ignore_hcs);
    RUN(test_idle_state_allows_only_the_init_commands);
    RUN(test_read_and_write_are_refused_before_init);
    RUN(test_write_waits_for_the_busy_line);
    RUN(test_write_gives_up_when_busy_never_ends);
    RUN(test_reselect_during_programming_keeps_dataout_low);
    RUN(test_programming_error_shows_only_in_cmd13);
    RUN(test_read_error_token_reaches_the_caller);
TEST_MAIN_END()
